# TOCTOU

**Time-of-Check to Time-of-Use (TOCTOU)** is a class of software/firmware race condition vulnerabilities. It occurs when a program validates a resource (the "Check") and then uses that resource (the "Use"), but the resource is mutated by an external entity **between** the check and the use.

In hardware-firmware security, TOCTOU is also known as a **Double Fetch** vulnerability. It typically happens at the boundary where a high-privilege processor (like a Security Processor) interacts with a low-privilege processor (like an AI Tile) via **Shared Memory**.

---

### The Anatomy of a Hardware TOCTOU Attack

Imagine the AI Tile and the Security Processor share a region of RAM (e.g., Mailbox buffers) to communicate. The AI Tile writes its data transfer request to this shared memory, and the Security Processor reads it.

Here is the timeline of how an attacker exploits a naive firmware check:

```
Shared Memory (Untrusted)        Security Processor (Firmware)       Attacker Thread (AI Tile)
+-----------------------+        +---------------------------+       +-----------------------+
|                       |        |                           |       |                       |
|                       |        | 1. Read request parameters|       |                       |
| addr = 0x80001000     |=======>|    (addr = 0x80001000)    |       |                       |
| (Benign memory address)        |                           |       |                       |
|                       |        | 2. Check: Is address      |       |                       |
|                       |        |    0x80001000 allowed?    |       |                       |
|                       |        |    [RESULT: YES]          |       |                       |
|                       |        |                           |       | 3. SWAP ADDRESS!      |
| addr = 0x00000000     |<===========================================| Overwrites memory with|
| (Secure boot ROM)     |        |                           |       | private/secure address|
|                       |        |                           |       +-----------------------+
|                       |        | 4. Use: Read from shared  |
|                       |        |    memory again to program|
|                       |=======>|    the HW register.       |
|                       |        |    (addr = 0x00000000)    |
|                       |        |                           |
|                       |        | 5. Hardware transfers the |
|                       |        |    Secure Boot ROM data   |
|                       |        |    to the AI Tile's SRAM! |
+-----------------------+        +---------------------------+
```

---

### Concrete Code Demonstration

#### The Vulnerable Code (Double Fetch)
The code below looks correct at first glance, but it is highly vulnerable to TOCTOU because it accesses the pointer `untrusted_req` twice.

```cpp
// DANGEROUS: Double Fetch Vulnerability
bool unsafe_handle_request(SharedMemoryRequest* untrusted_req, MailboxRegisters& hw_regs) {
    
    // --- TIME OF CHECK ---
    // First Fetch: Compiler generates a read from shared memory
    if (untrusted_req->requested_address < ALLOWED_BASE || 
        untrusted_req->requested_address > ALLOWED_BASE + ALLOWED_SIZE) {
        return false; // Address out of bounds
    }

    // <--- Vulnerability Window: Attacker swaps the pointer in shared memory here! --->

    // --- TIME OF USE ---
    // Second Fetch: Compiler generates ANOTHER read from shared memory
    // because untrusted_req is marked volatile or not cached.
    hw_regs.address = untrusted_req->requested_address; // Attacker's swapped address is used!
    hw_regs.command = CMD_START;

    return true;
}
```

#### The Secure Code (Copy-Before-Validate)
To fix this, the firmware must snap a local copy of the data inside its own private memory (the stack or CPU registers) which the AI Tile cannot access. Once the copy is made, the shared memory is never accessed again for this request.

```cpp
// SECURE: Copy-Before-Validate
bool safe_handle_request(SharedMemoryRequest* untrusted_req, MailboxRegisters& hw_regs) {
    
    // 1. Snapshot / Single Fetch
    // We copy the contents to a local stack variable.
    // The attacker can change 'untrusted_req' now, but they cannot touch 'local_address'.
    uint64_t local_address = untrusted_req->requested_address; 

    // --- TIME OF CHECK ---
    // We validate the local copy
    if (local_address < ALLOWED_BASE || local_address > ALLOWED_BASE + ALLOWED_SIZE) {
        return false;
    }

    // --- TIME OF USE ---
    // We program the hardware using the local copy
    hw_regs.address = local_address; 
    hw_regs.command = CMD_START;

    return true;
}
```

### Why is this hard to spot?
TOCTOU bugs are notoriously hard to catch during normal unit testing because they rely on precise multi-threaded or multi-processor timing. In a test harness, the code might run sequentially (Cycle 0, Cycle 1) and pass every time. But in silicon, where the AI Tile has dedicated hardware execution pipelines, it can easily win the race against a slow microcontroller firmware loop.

---

To see how this works, we must look at what is executing **concurrently** on the two different processors. 

Here is the code executing in parallel on the **Platform Security Processor (Firmware)** and the **AI Compute Tile (Attacker)**, sharing the same physical memory space.

### The Setup: Shared State
```cpp
// This struct resides in a region of SRAM accessible by BOTH processors.
struct SharedMemoryRequest {
    volatile uint64_t requested_address;
};

// Global pointer to the shared memory interface
SharedMemoryRequest* g_shared_req = (SharedMemoryRequest*)0x50000000; 

const uint64_t SECURE_KEYS_ADDR = 0x00001000; // Restricted!
const uint64_t SAFE_DATA_ADDR   = 0x80005000; // Allowed!
```

---

### Sequential Execution Trace (Nanosecond by Nanosecond)

Here is the exact instruction sequence. The attacker wins the race because the Security Processor performs the **check** and the **use** as two separate read operations from the shared memory pointer.

| Time (ns) | Security Processor (Firmware) | Memory State (`g_shared_req->requested_address`) | AI Compute Tile (Attacker Kernel) |
| :--- | :--- | :--- | :--- |
| **T0** | *(Idle)* | `0x00000000` | **Attacker prepares normal request:**<br>`g_shared_req->requested_address = SAFE_DATA_ADDR;` |
| **T1** | *(Idle)* | `0x80005000` *(SAFE)* | **Attacker triggers the Mailbox Interrupt.** |
| **T2** | **Firmware starts checking:**<br>`// Read #1: Check bounds`<br>`if (g_shared_req->requested_address < ALLOWED_BASE)` | `0x80005000` *(SAFE)* | *(Waiting)* |
| **T3** | `// 0x80005000 is allowed. Check passes!` | `0x80005000` *(SAFE)* | *(Waiting)* |
| **T4** | `// Firmware prepares to write to HW register...` | `0x80005000` *(SAFE)* | **Attacker swaps the address quickly:**<br>`g_shared_req->requested_address = SECURE_KEYS_ADDR;` |
| **T5** | *(Vulnerability Window is open)* | `0x00001000` *(MALICIOUS)* | *(Waiting)* |
| **T6** | **Firmware performs the "Use":**<br>`// Read #2: Write to hardware`<br>`hw_regs.address = g_shared_req->requested_address;` | `0x00001000` *(MALICIOUS)* | *(Waiting)* |
| **T7** | `hw_regs.command = CMD_START;`<br>*(Hardware starts transferring the secure keys!)* | `0x00001000` *(MALICIOUS)* | **Attacker extracts the keys** from the Mailbox SRAM buffer. |

---

### Why the Compiler doesn't save you
You might ask: *"Why doesn't the firmware compiler just keep the value in a CPU register after the first read?"*

Because the pointer is marked as `volatile` (or it is in a memory page marked as volatile/uncached). In embedded firmware, we must mark shared buffers as `volatile` so the compiler knows the hardware or another processor can update it. 

However, because it is `volatile`, the compiler is **forced** to generate a CPU `LOAD` instruction *every single time* the variable is referenced in the code:

```assembly
; ASSEMBLY GENERATED BY THE UNSECURE FIRMWARE (Simplified RISC-V)

; --- TIME OF CHECK ---
lw  t0, 0(a0)       ; LOAD #1: Read from shared memory into register t0
li  t1, ALLOWED_BASE
blt t0, t1, error   ; Branch if t0 < ALLOWED_BASE

; --- ATTACK WINDOW (Attacker overwrites memory here) ---

; --- TIME OF USE ---
lw  t0, 0(a0)       ; LOAD #2: Reads from shared memory AGAIN into t0! 
                    ; (Attacker's new value is loaded)
sw  t0, 8(a2)       ; Write loaded value to HW address register
```

By explicitly copying to a local variable `uint64_t local_addr = untrusted_req->requested_address;` at the very beginning, you force the CPU to only do **one single load** (`LOAD #1`). All subsequent checks and hardware writes use the register `t0` directly, making the attack impossible.