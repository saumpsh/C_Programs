# The AI Tile Mailbox Controller: Security Engineering Co-Design

This guide expands upon the foundational "AI Tile Mailbox Controller" interview scenario, pivoting it from a functional firmware exercise into a rigorous **Hardware Security Engineering** interview. It evaluates a candidate's ability to apply a "Zero Trust" mindset to hardware-software interfaces.

---

## 1. The Scenario Pivot: "The Untrusted Tenant"

### The Setup
You are writing the Firmware Driver for the Mailbox hardware unit. This driver runs on the highly trusted **Platform Security Processor (Root of Trust)**. 

The **Compute Tile (AI Kernel)** is running third-party, multi-tenant workloads. **It is completely untrusted and potentially malicious.**

### The Hardware Updates
*   **Shared Memory Interface:** Instead of direct function calls, the AI Tile submits requests via an untrusted Shared Memory Ring Buffer.
*   **Transfer Size:** The hardware can now transfer variable amounts of data. The AI tile requests an `address` and a `size`.
*   **SRAM Buffer:** The hardware still has the 64KB internal SRAM buffer, which is shared sequentially among different AI jobs/tenants.
*   **Allowed Memory Region:** The firmware must enforce that the AI Tile only reads from a specific region: `ALLOWED_BASE` to `ALLOWED_BASE + ALLOWED_SIZE`.

### The Task
Identify the security vulnerabilities in a naive implementation and write the robust, secure firmware handler `process_mailbox_request()` that protects the SoC from a malicious AI Tile.

---

## 2. The Vulnerabilities (What to Evaluate)

An interviewer should look for the candidate to independently identify and mitigate the following four adversarial threat vectors:

### A. Time-of-Check to Time-of-Use (TOCTOU)
*   **The Trap:** Reading the `address` and `size` directly from the untrusted shared memory, validating them, and then writing those shared memory pointers to the hardware registers.
*   **The Exploit:** A malicious tile has a concurrent thread running. It waits for the firmware to validate the benign `address`, and then overwrites it in shared memory with a malicious address (e.g., pointing to RoT private keys) *before* the hardware initiates the transfer.
*   **The Fix:** **Copy-Before-Validate.** The firmware must copy the request parameters into its own private, secure memory/stack *first*, validate the local copy, and use only the local copy for the hardware transaction.

### B. Integer Overflow in Bounds Checking (MPU Bypass)
*   **The Trap:** Implementing bounds checking like this: `if (address + size > ALLOWED_BASE + ALLOWED_SIZE) { return ERROR; }`
*   **The Exploit:** A malicious tile passes a valid `address` but a massive `size` (e.g., `0xFFFFFFFFFFFFFFFF`). The addition wraps around (integer overflow) to a very small number, bypassing the check and tricking the hardware into reading restricted global memory into the SRAM.
*   **The Fix:** Safe arithmetic checks. E.g., check `if (size > MAX_ALLOWED)` and `if (address > ALLOWED_BASE + MAX_ALLOWED - size)`.

### C. Data Remanence (Information Disclosure)
*   **The Trap:** Completing a transfer or resetting the hardware after an error, and immediately handing the mailbox over to the next tenant.
*   **The Exploit:** Tenant A transfers highly sensitive cryptographic material. Tenant A finishes. Tenant B requests a 1-byte transfer but reads the rest of the 64KB SRAM to steal Tenant A's leftover data.
*   **The Fix:** **SRAM Scrubbing.** The firmware state machine must explicitly zeroize or cryptographically wipe the 64KB SRAM buffer before signaling `READY` for the next tenant or recovering from a `STATUS_ERROR`.

### D. Denial of Service (DoS) via Reset Storms
*   **The Trap:** If the hardware hangs (e.g., due to an invalid command), the firmware gracefully issues a `CMD_RESET` to fix it and moves on.
*   **The Exploit:** A malicious tile intentionally triggers hardware hangs repeatedly. The firmware gets stuck in a loop constantly resetting the hardware, making the Mailbox unavailable to all other benign tiles on the SoC.
*   **The Fix:** **Rate Limiting & Quarantine.** Implement error counters. If a specific tenant/tile causes too many hardware faults, the firmware transitions that tile to a "quarantined" state and refuses to service its requests.

---

## 3. The Secure Implementation

Below is a detailed C++ implementation demonstrating how a candidate should securely process a request from the untrusted shared memory interface.

```cpp
#include <cstdint>
#include <iostream>

// ============================================================================
// SYSTEM DEFINITIONS
// ============================================================================

// Memory region the AI Tile is allowed to access
const uint64_t ALLOWED_BASE = 0x80000000;
const uint64_t ALLOWED_SIZE = 0x01000000; // 16 MB limit

// Hardware Registers
struct MailboxRegisters {
    volatile uint32_t command; 
    volatile uint32_t status;  
    volatile uint64_t address; 
    volatile uint32_t size;    // Hardware now accepts a size
};

const uint32_t CMD_START = 0x1;
const uint32_t STATUS_BUSY = 0x1;

// Untrusted Shared Memory Interface (Modified by AI Tile)
struct SharedMemoryRequest {
    volatile uint64_t requested_address;
    volatile uint32_t requested_size;
};

// ============================================================================
// SECURE FIRMWARE IMPLEMENTATION
// ============================================================================

// Helper: Hardware SRAM Scrubbing
void secure_zeroize_sram() {
    // In a real system, this might involve writing to a special 
    // hardware register that triggers a hardware-accelerated zeroize, 
    // or doing a secure memset on the mapped SRAM region.
    std::cout << "  [SEC] Zeroizing 64KB SRAM buffer to prevent data remanence.\n";
}

// Global DoS tracking (simplified for one tile)
static uint32_t g_consecutive_faults = 0;
const uint32_t MAX_FAULTS_BEFORE_QUARANTINE = 3;
static bool g_tile_quarantined = false;

bool process_mailbox_request(MailboxRegisters& hw_regs, SharedMemoryRequest* untrusted_req) {
    
    // 1. Availability / Anti-DoS Check
    if (g_tile_quarantined) {
        std::cout << "  [SEC] Request rejected. Tile is quarantined due to malicious activity.\n";
        return false;
    }

    // 2. TOCTOU Defense: Copy parameters to secure local memory FIRST
    // Using 'volatile' on the struct fields ensures the compiler actually reads from memory here.
    uint64_t local_address = untrusted_req->requested_address;
    uint32_t local_size = untrusted_req->requested_size;

    // 3. Input Validation (Bounds Checking & Integer Overflow Protection)
    
    // Check A: Is the size itself absurd?
    if (local_size == 0 || local_size > ALLOWED_SIZE) {
        std::cout << "  [SEC] Request rejected. Size out of bounds.\n";
        return false;
    }

    // Check B: Does the start address fall below the allowed base?
    if (local_address < ALLOWED_BASE) {
        std::cout << "  [SEC] Request rejected. Address below allowed base.\n";
        return false;
    }

    // Check C: Does the transfer exceed the upper bound? (Integer Overflow Safe)
    // BAD: if (local_address + local_size > ALLOWED_BASE + ALLOWED_SIZE) -> Can overflow!
    // GOOD: Rearrange the algebra to subtract, preventing overflow:
    uint64_t max_allowed_addr = ALLOWED_BASE + ALLOWED_SIZE;
    if (max_allowed_addr - local_address < local_size) {
        std::cout << "  [SEC] Request rejected. Transfer exceeds allowed region boundaries.\n";
        return false;
    }

    // 4. State Verification
    if (hw_regs.status == STATUS_BUSY) {
        std::cout << "  [SEC] Request rejected. Hardware is busy.\n";
        
        // Track potential DoS attempts
        g_consecutive_faults++;
        if (g_consecutive_faults >= MAX_FAULTS_BEFORE_QUARANTINE) {
            g_tile_quarantined = true;
            std::cout << "  [SEC] ALERT: Tile quarantined after consecutive HW faults!\n";
        }
        return false;
    }

    // Reset fault counter on a successful, valid request initiation
    g_consecutive_faults = 0;

    // 5. Secure Hardware Interfacing
    // WE ONLY USE THE LOCAL, VALIDATED COPIES (local_address, local_size)!
    hw_regs.address = local_address;
    hw_regs.size = local_size;
    
    // 6. Data Isolation
    // Before starting a new transfer, ensure the SRAM is clean from the previous tenant
    secure_zeroize_sram();

    // 7. Initiate Transfer
    hw_regs.command = CMD_START;
    
    std::cout << "  [FW] Transfer successfully initiated.\n";
    return true;
}
```

## Summary for Interviewers
When evaluating a candidate's response to this scenario:
1. **Did they trust the pointer?** If they operated directly on `untrusted_req->requested_address`, they failed the TOCTOU check.
2. **Did they check for overflow?** If their math allows wrapping `address + size` around zero, they failed the MPU bypass check.
3. **Did they clean up?** If they didn't mention clearing the SRAM, they missed the data remanence vulnerability. 
4. **Did they protect availability?** If they allowed infinite failures to trigger infinite resets, they failed the DoS check.
