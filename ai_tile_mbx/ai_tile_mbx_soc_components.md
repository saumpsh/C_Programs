# SoC components

In modern AI accelerator and System-on-Chip (SoC) architectures (such as Google TPUs, GPUs, or high-performance NPUs), hardware is highly modular. Here is a breakdown of these components, how they interact, and how they relate to security boundaries.

---

### 1. What is an "AI Tile"?
An **AI Tile** (sometimes called a core, compute engine, or processing element) is a physical, self-contained silicon block on the chip dedicated to executing machine learning math. 

*   **Structure:** A typical tile contains matrix multiplication units (MXUs), vector processing units (VPUs), a local high-bandwidth scratchpad memory (SRAM), and a minimal instruction sequencer.
*   **Scaling:** Instead of building one giant processor, chip designers scale AI accelerators by tiling dozens or hundreds of these identical blocks across a mesh/grid interconnect.

### 2. What is an "AI Kernel"?
In this context, a **Kernel** is not an Operating System kernel (like Linux). Instead, it refers to a **compute kernel**—a small, highly optimized software program that runs bare-metal on the AI Tile to perform a specific mathematical operation (e.g., a Matrix Multiplication, a Convolution, or an Activation function like ReLU/GeLU).
*   In multi-tenant cloud environments, these kernels are often compiled from user-submitted models (e.g., TensorFlow, PyTorch), meaning the code running on the AI Tile is **completely untrusted**.

---

### 3. How do they relate to the Security Processor?
The **Security Processor** (often a dedicated on-chip microcontroller like **Caliptra**, acting as the Root of Trust) is the security anchor of the entire SoC. It is isolated from the heavy data compute paths to minimize its attack surface.

*   **The Privilege Boundary:** The AI Tile operates at a low privilege level and is untrusted. The Security Processor operates at the highest privilege level.
*   **The Mailbox Bridge:** If the AI Tile needs to fetch new weights or input data from Global Memory, it cannot be allowed to program the system DMA (Direct Memory Access) engines directly, because a compromised AI Kernel could point the DMA to read the Security Processor's private cryptographic keys or another user's data.
*   **The Gatekeeper:** Instead, the AI Tile writes a request to a **Mailbox**. The privileged Security Processor (or secure firmware authorized by it) reads the request, validates the addresses and sizes (the MPU/bounds checks), configures the hardware safely, and monitors the transfer. The AI Tile only sees the final data in its isolated SRAM buffer.

---

### 4. Is there also a Control and Management Processor?
Yes. Real-world AI accelerators use a hierarchy of processors to split duties between compute, scheduling, management, and security:

```
+-------------------------------------------------------------------------------+
|                                    SoC / BOARD                                |
|                                                                               |
|  +--------------------+   +-----------------------+   +--------------------+  |
|  | Management (BMC)   |-->| Host CPU (x86/ARM)    |-->| Device Controller  |  |
|  | Out-of-band, Power |   | Orchestrates Workload |   | (Control Plane CPU)|  |
|  +--------------------+   +-----------------------+   +---------|----------+  |
|                                                                 | Schedules   |
|  +--------------------+                                         v Workloads   |
|  | Security Processor |<==================================[ AI Tile Grid ]    |
|  | (Root of Trust)    | Monitors & Verifies Firmware      | [Tile]  [Tile]   |  |
|  +--------------------+                                   | [Tile]  [Tile]   |  |
+-----------------------------------------------------------+-------------------+
```

1.  **Management Processor (BMC - Baseboard Management Controller):** 
    *   *Role:* An external chip on the motherboard. It handles out-of-band management, monitoring board thermals, power sequencing, and orchestrating physical resets or bare-metal provisioning.
2.  **Host CPU (e.g., Host x86 or ARM CPU running Linux):**
    *   *Role:* Orchestrates the overall application. It compiles the AI models, loads the driver, handles network traffic, and sends high-level command queues to the accelerator.
3.  **Control/Device Processor (SoC Controller / Sequencer):**
    *   *Role:* An on-chip, firmware-driven processor (often a high-performance RISC-V or ARM core). It acts as the "control plane" for the accelerator. It parses the command queues sent by the Host CPU, manages the memory controllers, configures the global DMA, and schedules the **AI Kernels** onto the individual **AI Tiles**.
4.  **Security Processor (Root of Trust - RoT):**
    *   *Role:* Focuses purely on security. It verifies the cryptographic signatures of the firmware running on the Host CPU, BMC, and Control Processor before they are allowed to boot. During runtime, it manages cryptographic keys, handles identity attestation, and monitors the hardware firewalls/mailboxes to ensure the untrusted processors do not access secure physical memory.

---

# Architecture 1

## The `mailbox_request_transfer` API call

The `mailbox_request_transfer` API is called by the **Application Layer** (often referred to as the **Host Driver**, **Kernel Orchestrator**, or **Runtime Scheduler**) running on the main CPU (or the control-plane processor).

Here is the exact call hierarchy and why this separation is designed:

---

### 1. The System Call Hierarchy
```text
  +-----------------------------------------------------------+
  |              UPSTREAM APPLICATION / RUNTIME               |
  | (e.g., PyTorch Executor, TPU Scheduler, or Host App)      |
  +-----------------------------|-----------------------------+
                                |
                                | 1. Calls mailbox_request_transfer(0x00FFBEEF)
                                v
  +-----------------------------------------------------------+
  |                   FIRMWARE DRIVER (us)                    |
  |  - Validates software state machine (READY?)              |
  |  - Validates physical registers (not BUSY?)               |
  |  - Writes CMD_START to registers                          |
  +-----------------------------|-----------------------------+
                                |
                                | 2. Safe MMIO Write
                                v
  +-----------------------------------------------------------+
  |                     MAILBOX HARDWARE                      |
  |  - Pulls data from Global Memory to local SRAM.           |
  +-----------------------------------------------------------+
```

---

### 2. When is it Called?
It is called by the upstream runtime whenever the AI Tile needs a new dataset to process:
* **Weight Loading:** Loading a new layer's weights/biases for a neural network.
* **Activation Loading:** Loading a batch of user input data to execute an inference pass.
* **Double Buffering Pipeline:** In high-performance systems, the scheduler will call `mailbox_request_transfer` for *Buffer B* while the AI Tile is still calculating math on *Buffer A*, hiding memory transfer latency.

---

### 3. Why the App is Forbidden from Writing Directly to Registers
In our simulation, we represent the App calling the driver in the test bench loop:
```cpp
// Step B: Trigger an action at Cycle 1
if (cycle == 1) {
    std::cout << "  [App] Requesting data transfer via Firmware...\n";
    bool success = mailbox_request_transfer(my_hw.regs, 0x00FFBEEF);
    std::cout << "  [App] Request status: " << (success ? "ACCEPTED" : "REJECTED") << "\n";
}
```

The App does not have direct access to the `MailboxRegisters` structure. 
1. **Security:** If the untrusted App wrote directly to `regs.address`, a malicious user model could point the hardware to read secure keys from private physical memory. The driver API acts as a gateway to perform **Bounds Checking** (MPU protection) on the requested memory address.
2. **Hardware Protection:** If the App wrote to `regs.command` directly without checking if the hardware is busy, it would crash/hang the mailbox. The driver API wraps this call with defensive checks.

---

# Architecture 2

Created toctou.md
Viewed ai_tile_mbx_security.md:57-127

You have hit on a fundamental concept in hardware architecture. Your understanding of the control processor orchestrating the tiles is correct, but we need to look at **Data Flow vs. Control Flow** and **latency hiding** to see why tiles make requests.

Here is how real AI accelerators (like TPUs or GPUs) handle this, and how the AI Tile "calls" the privileged processor.

---

### Part 1: Why does an AI Tile request data? (Data Flow & Latency Hiding)

While it's true that the Control Processor tells the AI Tile *what* program to run (Control Flow), the AI Tile must manage its own data ingestion during execution (Data Flow) to stay fast.

AI models are massive (gigabytes), but an AI Tile's local SRAM is very small (often 64KB to 1MB). A tile cannot hold a whole model; it can only hold a tiny slice of weights and activations at one time. 

If the tile had to stop computing, wait for the Control Processor to notice, wait for the Control Processor to schedule a DMA transfer, and then resume computing, the AI execution would be extremely slow.

To hide this memory transfer latency, AI Tiles use **Double-Buffering**:
1.  The tile is actively computing on **Buffer A** (e.g., performing matrix multiplication on one set of weights).
2.  At the same time, the tile *autonomously* requests the hardware to load the next set of weights into **Buffer B**.
3.  Once the tile finishes computing on A, and B's data transfer is complete, it immediately swaps: it computes on B, and requests the *next* data slice into A.

To make this double-buffering work, the AI Tile's internal code (the kernel) must be able to fire-and-forget data transfer requests. This is why the **AI Tile is the initiator of the mailbox request**.

---

### Part 2: How does the AI Tile "call" the firmware function?

You are 100% correct that the AI Tile **cannot directly call a C++ function** like `process_mailbox_request` on the Security Processor. They are physically different CPUs, running different instruction sets, on different parts of the silicon.

Instead, the "function call" is done through **Hardware Inter-Process Communication (IPC)**. 

Here is how that looks in physical hardware:

```text
  AI COMPUTE TILE (Untrusted)                  SECURITY PROCESSOR (Privileged)
+-------------------------------+             +--------------------------------+
|  Runs Vector Instructions     |             |  Runs Security Firmware (C++)  |
|                               |             |                                |
|  1. Writes request parameters  |             |  4. Hardware interrupt triggers|
|     to Shared Memory (RAM)    |             |     the ISR (Interrupt Service |
|                               |             |     Routine)                   |
|  2. Writes to "Doorbell" reg   |             |                                |
|     (Hardware Register)       |             |  5. Firmware executes:         |
|             |                 |             |     process_mailbox_request()  |
+-------------|-----------------+             +----------------^---------------+
              |                                                |
              | Network-on-Chip (NoC) transaction              |
              +------------------------------------------------+
```

1.  **Writing to Shared RAM:** The AI Tile writes the source address and size to a predetermined location in shared RAM (e.g., `0x50000000`).
2.  **Ringing the "Doorbell":** The AI Tile writes a `1` to a specific memory-mapped hardware register called a **Doorbell Register**.
3.  **Physical Interrupt:** The hardware of the Doorbell Register is physically wired to the interrupt controller of the Security/Control Processor. Writing to it immediately asserts an interrupt signal (a physical electrical high/low state) on the Security Processor.
4.  **Interrupt Handling:** The Security Processor stops what it is doing and jumps to its Interrupt Service Routine (ISR).
5.  **Running the Code:** The ISR checks which doorbell rang, extracts the pointer to the shared RAM, and passes it to the function:
    ```cpp
    // Inside Security Processor's Interrupt Handler
    void mailbox_interrupt_handler() {
        SharedMemoryRequest* req = (SharedMemoryRequest*)0x50000000;
        process_mailbox_request(g_mailbox_regs, req);
    }
    ```

### Summary
*   **The Control Processor** orchestrates the *high-level* program (e.g., "Run Layer 5 convolution on Tile 0").
*   **The AI Tile** orchestrates the *low-level* loop (e.g., "I am computing step $i$; grab the weights for step $i+1$ asynchronously").
*   **The Security Processor** acts as the *secure driver/firewall*, intercepting the AI Tile's hardware requests (via interrupts/doorbells) to validate them before the physical DMA hardware acts on them.

---

# Compare Architectures

Viewed ai_tile_mbx_soc_components.md:1-113

Yes, there is an apparent contradiction, but it highlights the difference between a **simplified functional interview model** and a **security-hardened architecture**. 

Here is how the two models compare and why we shifted the design for the security-focused version.

---

### The Contrast

| Feature | The Original Functional Model (`ai_tile_mbx_soc_components.md`) | The Security-Hardened Model (`ai_tile_mbx_security.md`) |
| :--- | :--- | :--- |
| **Who initiates the transfer?** | **Host CPU / Control Processor** (trusted) | **AI Compute Tile** (untrusted) |
| **Data Flow Direction** | **"Push" Model:** The Host Driver decides when the Tile needs data, fetches it, and passes control to the Tile. | **"Pull" Model:** The Tile is executing code, realizes it needs more data, and requests it asynchronously. |
| **The Calling Interface** | Direct C++ API call: `mailbox_request_transfer()` | Hardware IPC: Writing to Shared Memory + Ringing a Doorbell |
| **The Security Boundary** | Minimal. The caller (Host App) is trusted; we are only protecting the hardware from accidental hangs. | Strict. The caller (AI Tile) is **untrusted**; we are protecting the SoC from malicious exploits. |

---

### Why the Security Model Shshifted (and why both exist in the real world)

#### 1. In a simple functional interview (The "Push" Model)
In the original `ai_tile_mbx.md` prompt, the focus was purely on asynchronous state machines. The test bench (`main()`) represents the Host Application calling `mailbox_request_transfer()`. The Host CPU acts as the orchestrator, and the AI Tile is a passive slave that just does math when told. 

This is common in simpler accelerators (like early VPUs) where the host CPU does all the scheduling.

#### 2. In a security-focused interview (The "Pull" Model)
To test a candidate on **TOCTOU, MPU bypass, and DoS**, we *had* to introduce a strict security boundary. 

If the trusted Host CPU is the only one calling the driver, the boundary is clean: we trust the Host CPU not to perform TOCTOU attacks on itself. 

By shifting the model to the **AI Tile initiating requests via Shared Memory** (as detailed in `ai_tile_mbx_security.md`):
*   We establish the AI Tile as the **untrusted initiator**.
*   We create the shared memory space that is vulnerable to **TOCTOU**.
*   We allow the AI Tile to pass arbitrary addresses, necessitating the **MPU check**.
*   We allow the AI Tile to trigger errors, enabling a **DoS attack** check.

### Real-World Context
In a complex modern accelerator (like a Google TPU or an NVIDIA GPU):
*   **Both models exist.** 
*   The **Host CPU** orchestrates the high-level graph execution (Push model).
*   However, during execution, the **AI Tiles** autonomously run loops that fetch their own weights and inputs from memory using local DMA/Mailboxes to hide latency (Pull model). 

Because the code running on the AI Tiles is often compiled from user-space ML models (e.g., PyTorch), the Security Processor *must* treat the AI Tiles' hardware requests with the "Zero Trust" pull-model checks we mapped out.