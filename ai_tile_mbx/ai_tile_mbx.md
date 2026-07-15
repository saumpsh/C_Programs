# The AI Tile Mailbox Controller: An Iterative Co-Design Guide

This guide is designed for preparing for hardware-software co-design, resource management, and asynchronous data movement interview questions (particularly relevant for AI accelerators and SoC uncore logic). It illustrates how to model hardware, build defensive firmware, and trace system execution over time.

---

## 1. The Scenario: "The AI Tile Mailbox Controller"

### The Setup
You are designing a firmware driver and a software model for a "Mailbox" hardware unit. This unit is the primary interface between a **Compute Tile** (which runs AI kernels) and the **Global Memory** (SoC Uncore).

### The Hardware (The "Constrained Mechanism")
* **Buffer:** The Mailbox has a limited internal SRAM buffer (64KB).
* **Command Register:** Writing a `START` bit triggers the HW to pull data from Global Memory into the SRAM.
* **Status Register:** A single register indicates if the HW is `IDLE`, `BUSY`, or `DATA_READY`.
* **Constraint:** The hardware cannot handle "Back-to-Back" requests. If you write to the Command Register while the status is `BUSY`, the hardware hangs and requires a reset.
* **Timing:** Moving data from Global Memory to SRAM takes a variable amount of time (latency depends on bus congestion).

### The Task
1. **Model the Hardware:** Write a basic software representation of this Mailbox that simulates the delay of data movement.
2. **Write the Firmware:** Implement a non-blocking function `request_data(uint64_t address)` that allows the AI Tile to request data safely without hanging the hardware.

```text
+---------------------------------------------------------------------------------------+
|                                     COMPUTE TILE                                      |
|                                                                                       |
|   +---------------------+                                                             |
|   |    AI Kernel / App  |                                                             |
|   +----------|----------+                                                             |
|              |                                                                        |
|              | 1. Request Data (Non-blocking)                                         |
|              v                                                                        |
|   +-------------------------------------------------------------------------------+   |
|   |                        FIRMWARE DRIVER (The Guardian)                         |   |
|   |                                                                               |   |
|   |   +----------------------+               +--------------------------------+   |   |
|   |   | Driver Software State|               |      Local Software Queue      |   |   |
|   |   | [INIT/READY/BUSY]    |<------------->| (Absorbs "Back-to-Back" bursts)|   |   |
|   |   +----------------------+               +--------------------------------+   |   |
|   +--------------|-------------------------------------------^--------------------+   |
|                  |                                           |                        |
|                  | 2. Safe MMIO Write                        | 4. Hardware Interrupt  |
|                  |    (Only if HW is IDLE)                   |    or Status Poll      |
+------------------|-------------------------------------------|------------------------+
                   |                                           |
                   v                                           |
+--------------------------------------------------------------|------------------------+
|                                    MAILBOX HARDWARE                                   |
|                                                                                      |
|   +--------------------+      +--------------------+      +-----------------------+   |
|   |  Command Register  |      |  Status Register   |      |  Internal SRAM Buffer |   |
|   |    [ START BIT ]   |      | [IDLE/BUSY/READY?] |      |        (64 KB)        |   |
|   +---------|----------+      +---------^----------+      +-----------^-----------+   |
|             |                           |                             |               |
|             | 3. Triggers HW Logic      | Updates State               | 3b. Pulls     |
|             +---------------------------+----------------+            |     Data      |
+----------------------------------------------------------|------------|---------------+
                                                           |            |
                                                           v            |
+-----------------------------------------------------------------------|---------------+
|                              GLOBAL MEMORY (Variable Latency Bus)     |               |
|                                                                       |               |
|   +-------------------------------------------------------------------|-----------+   |
|   | Data Array [0x0000 ... 0xFFFF]  ==================================+           |   |
+---------------------------------------------------------------------------------------+
```

---

## 2. Interview Strategy & Checklist

### Why a Progressive, Iterative Approach Works

A progressive, step-by-step journey mimics the exact flow of a successful 45-minute architectural interview:

1. **Decouples Architecture from Syntax:** By separating system topology and interface design from implementation details, the focus remains on high-level constraints, which represent 80% of the evaluation criteria.
2. **Mirror-Matches the Interview Timeline:** Interviewers want to see you establish invariants and design structures *before* writing execution logic.
3. **Builds a Reusable "Pattern Library":** Most hardware-modeling problems boil down to a common pattern: **a software state machine managing an asynchronous, non-deterministic hardware resource.**

### Mock Interview Checklist

| Guideline | What an "Expert" answer looks like |
| --- | --- |
| **Manage State** | You use an `enum` for the HW status and a separate `struct` for the FW's internal tracking. |
| **Reasoning over Time** | You explain: "The FW initiates the transfer at $T_0$, enters a non-blocking wait, and the HW model updates the status to 'Ready' at $T_{10}$." |
| **Multiple Pieces of Work** | You suggest a **Software Queue** to hold pending AI Tile requests while the Mailbox HW is busy. |
| **Hardware Abstraction** | You define a clear `read_reg()` and `write_reg()` API rather than using global variables directly. |

---

## 3. Stage 1: Hardware Abstraction & State Boundaries

To model a hardware mechanism in software, we must establish a clear contract by defining:
1. **The Hardware Registry/Interface:** How the software communicates with the hardware (MMIO registers).
2. **The Internal Hardware State:** Private simulator variables (e.g., progress tracking, clock cycles) that actual firmware cannot access.
3. **The Firmware State:** Internal driver tracking used to run the non-blocking lifecycle.

### Initial Interface Design
```cpp
#include <cstdint>

// 1. Hardware-Facing Interface (Registers)
// This is the only bridge between Firmware and Hardware.
struct MailboxRegisters {
    volatile uint32_t command; // Write 1 to START a transfer
    volatile uint32_t status;  // Reads: 0 = IDLE, 1 = BUSY, 2 = DATA_READY
    volatile uint64_t address; // Destination memory address
};

// Register Bitmasks / Magic Numbers
const uint32_t CMD_START        = 0x1;
const uint32_t STATUS_IDLE      = 0x0;
const uint32_t STATUS_BUSY      = 0x1;
const uint32_t STATUS_DATA_RDY  = 0x2;

// 2. Private Hardware Simulation State
// The firmware CANNOT see or access this struct. It belongs strictly to the simulator.
struct HardwareSimulator {
    MailboxRegisters regs;      // The exposed register map
    uint32_t cycles_remaining;  // Simulates the variable bus latency over time
};

// 3. Firmware Driver Internal State
// The firmware uses this to manage its own non-blocking lifecycle.
enum class DriverState {
    UNINITIALIZED,
    READY,
    TRANSFER_IN_PROGRESS,
    DATA_AVAILABLE,
    ERROR_HARDWARE_HUNG
};
```

> [!NOTE]
> **Interviewer's Perspective: Separation of Concerns**
> Keeping `HardwareSimulator` completely distinct from `DriverState` demonstrates an understanding of physical system boundaries. Real-world firmware cannot peek into hardware-internal variables like `cycles_remaining`; it can only read what is exposed via the `status` register.

> [!WARNING]
> **Catching the Trap**
> Since writing a command while `BUSY` hangs the hardware, transitioning the register status directly to `STATUS_ERROR` allows us to model, simulate, and observe this catastrophic error state.

---

## 4. Stage 2: Addressing Edge Cases & Error Recovery

To ensure the driver can handle real-world hardware failures and transitions, we must clarify three system behaviors:

1. **Immediate Busy Transition:** The status register must transition to `BUSY` immediately upon writing `START` to enforce system invariants.
2. **Observability of Hung States (`STATUS_ERROR`):** If the hardware hangs silently without mutating its registers, the software is blind. Adding a dedicated error status makes physical hangs visible to the software.
3. **Reset Mechanism:** Designing an explicit hardware reset command enables self-healing firmware.

### Updated Interface Design
```cpp
#include <cstdint>

// 1. Updated Hardware-Facing Interface (Registers)
struct MailboxRegisters {
    volatile uint32_t command; // Write 1 = START, Write 2 = RESET
    volatile uint32_t status;  // Reads: 0=IDLE, 1=BUSY, 2=DATA_READY, 3=ERROR
    volatile uint64_t address; 
};

// Updated Magic Numbers
const uint32_t CMD_START        = 0x1;
const uint32_t CMD_RESET        = 0x2; // Added reset command capability

const uint32_t STATUS_IDLE      = 0x0;
const uint32_t STATUS_BUSY      = 0x1;
const uint32_t STATUS_DATA_RDY  = 0x2;
const uint32_t STATUS_ERROR     = 0x3; // Added to make hang observable

// 2. Private Hardware Simulation State (Unchanged)
struct HardwareSimulator {
    MailboxRegisters regs;      
    uint32_t cycles_remaining;  
};

// 3. Updated Firmware Driver Internal State
enum class DriverState {
    UNINITIALIZED,
    READY,
    TRANSFER_IN_PROGRESS,
    DATA_AVAILABLE,
    RECOVERING_FROM_RESET, // Added to track the recovery lifecycle
    FATAL_ERROR
};
```

---

## 5. Stage 3: The Temporal Simulation Loop (The Hardware Clock)

The hardware simulator uses a clock tick function to decrement transfer counters and actively verify hardware rules.

```cpp
// This function simulates the hardware's internal clock/logic using a Switch-Case FSM.
// It is called by the test harness, NOT the firmware.
void hardware_tick(HardwareSimulator& hw) {
    // 1. Handle a Reset Command instantly (Global Asynchronous Event)
    if (hw.regs.command == CMD_RESET) {
        hw.regs.status = STATUS_IDLE;
        hw.regs.command = 0; // Clear command register
        hw.cycles_remaining = 0;
        return;
    }

    // 2. FSM State Transition & Action Logic
    switch (hw.regs.status) {
        case STATUS_IDLE:
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_BUSY;
                hw.regs.command = 0;
                hw.cycles_remaining = 5; // Latency countdown
            }
            break;

        case STATUS_BUSY:
            // TRAP CHECK: Write during busy state leads to hardware hang/error
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_ERROR;
                hw.regs.command = 0;
            } else {
                if (hw.cycles_remaining > 0) {
                    hw.cycles_remaining--;
                }
                if (hw.cycles_remaining == 0) {
                    hw.regs.status = STATUS_DATA_RDY;
                }
            }
            break;

        case STATUS_DATA_RDY:
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_BUSY;
                hw.regs.command = 0;
                hw.cycles_remaining = 5;
            }
            break;

        case STATUS_ERROR:
            // Remain in ERROR state until reset is issued
            break;
    }
}
```

> [!TIP]
> **Interviewer's Perspective: Temporal Progression & Safety Checks**
> Decrementing `cycles_remaining` by exactly 1 per tick models a cycle-accurate, time-domain simulation. The explicit check for a write command during the `STATUS_BUSY` state enforces the physical hardware constraint and tests the driver's robustness.

---

## 6. Stage 4: Non-Blocking Firmware Driver

The firmware driver consists of two main functions:
1. **`mailbox_request_transfer`**: Initiates a data transfer non-blockingly, returning a status code immediately to let the caller proceed.
2. **`mailbox_process_events`**: Periodically advances the internal driver state, processes hkardware register changes, and issues recovery sequences.

```cpp
// Internal driver state instance
static DriverState g_driver_state = DriverState::READY;

// 1. Kick off a transfer (Non-blocking)
bool mailbox_request_transfer(MailboxRegisters& regs, uint64_t target_address) {
    // Apply defensive checks based on driver state
    if (g_driver_state != DriverState::READY) {
        // Driver is busy, hung, resetting, or uninitialized
        return false; 
    }

    // DEFENSIVE CHECK: Software state machine believes the driver is READY, but the
    // physical hardware register reports BUSY. This mismatch indicates a race condition
    // or hardware glitch. Writing CMD_START now would trigger a fatal hardware hang.
    // Transition to FATAL_ERROR to lock down the driver and protect the system.
    if (regs.status == STATUS_BUSY) {
        // Recovery from FATAL_ERROR is non-autonomous. The driver remains locked in 
        // this state until a supervisor (OS / Hypervisor / System Controller) performs a hardware reset and re-initializes 
        // the driver via mailbox_init().
        // This is a deliberate safety and security design pattern in bare-metal and embedded systems. When software state and hardware state mismatch, the system's execution environment is considered **corrupted**. 
        // Letting the driver automatically try to recover could lead to further memory corruption, hardware damage, or security exploits (e.g., if a fault-injection attack is actively occurring).
        g_driver_state = DriverState::FATAL_ERROR;
        return false;
    }

    // Configure the hardware
    regs.address = target_address;
    regs.command = CMD_START; // Triggers immediate STATUS_BUSY on hardware side

    // Update internal software tracking
    g_driver_state = DriverState::TRANSFER_IN_PROGRESS;
    
    return true; // Successfully accepted
}

// 2. Periodic Housekeeping / State Machine Advancement (Non-blocking)
void mailbox_process_events(MailboxRegisters& regs) {
    switch (g_driver_state) {
        case DriverState::TRANSFER_IN_PROGRESS:
            if (regs.status == STATUS_DATA_RDY) {
                g_driver_state = DriverState::DATA_AVAILABLE;
            } else if (regs.status == STATUS_ERROR) {
                // Hardware hung. Initiate recovery.
                regs.command = CMD_RESET;
                g_driver_state = DriverState::RECOVERING_FROM_RESET;
            } else {
                // Do nothing... Wait for data to be ready
            }
            break;

        case DriverState::RECOVERING_FROM_RESET:
            // Wait for hardware to acknowledge the reset and return to IDLE
            if (regs.status == STATUS_IDLE) {
                g_driver_state = DriverState::READY; // Fully recovered
            }
            break;

        case DriverState::DATA_AVAILABLE:
            // Upstream software Workload consumes data, clears state back to ready
            std::cout << "  [FW] Data consumed by upstream software.\n";
            g_driver_state = DriverState::READY;
            break;

        default:
            // READY, or FATAL_ERROR require no autonomous action
            break;
    }
}
```

> [!NOTE]
> **Interviewer's Perspective: Defensive Co-design & Graceful Recovery**
> Checking both driver software state (`g_driver_state`) and hardware registers (`regs.status`) protects against multi-threaded race conditions or unexpected hardware glitch states. 
> Furthermore, handling recovery asynchronously via `RECOVERING_FROM_RESET` ensures that the driver does not block the entire CPU execution loop waiting on hardware registers.

---

## 7. Stage 5: System Integration & Execution Trace

To prove correctness, we run both components inside a step-by-step test loop:

```cpp
HardwareSimulator my_hw = { {0, STATUS_IDLE, 0}, 0 };

// Simulate a workload loop
for (int cycle = 0; cycle < 10; cycle++) {
    // Step A: Run Firmware housekeeping
    mailbox_process_events(my_hw.regs);

    // Step B: Try to request a transfer on cycle 1
    if (cycle == 1) {
        mailbox_request_transfer(my_hw.regs, 0x00FFBEEF);
    }

    // Step C: Step the physical hardware clock forward
    hardware_tick(my_hw);
}
```

### Trace Walkthrough
* **Cycle 0:** 
  * `mailbox_process_events()` runs: Driver is `READY`. Does nothing.
  * `hardware_tick()` runs: HW is `IDLE`. Does nothing.
* **Cycle 1 (The Request):** 
  * `mailbox_process_events()` runs: Driver is `READY`. Does nothing.
  * `mailbox_request_transfer()` is called: `regs.command` is set to `CMD_START`. Driver state becomes `TRANSFER_IN_PROGRESS`.
  * `hardware_tick()` runs: Sees `CMD_START`. Sets `regs.status = STATUS_BUSY`, sets `cycles_remaining = 5`, and clears `regs.command = 0`.
* **Cycle 2:** 
  * Driver is `TRANSFER_IN_PROGRESS`. HW status is `BUSY`. No state change.
  * `hardware_tick()` runs: Decrements `cycles_remaining` to 4.
* **Cycle 3:** `hardware_tick()` decrements `cycles_remaining` to 3.
* **Cycle 4:** `hardware_tick()` decrements `cycles_remaining` to 2.
* **Cycle 5:** `hardware_tick()` decrements `cycles_remaining` to 1.
* **Cycle 6:** 
  * Driver is `TRANSFER_IN_PROGRESS`. HW status is still `BUSY`.
  * `hardware_tick()` runs: Decrements `cycles_remaining` to 0. Since it reached 0, the hardware sets `regs.status = STATUS_DATA_RDY`.
* **Cycle 7 (The Detection):** 
  * `mailbox_process_events()` runs: Reads `regs.status` and detects `STATUS_DATA_RDY`.
  * Driver state transitions to `DriverState::DATA_AVAILABLE`.

> [!TIP]
> **Interviewer's Perspective: Temporal Latency**
> It takes exactly 6 cycles from request to software detection: 5 cycles of physical hardware bus latency + 1 cycle for the software event loop to sample the updated register. Showing this trace demonstrates you can calculate real-time system behavior under polling constraints.

---

### Complete, Unified C++ Implementation
Here is the self-contained simulation code:

```cpp
#include <iostream>
#include <cstdint>
#include <string>

// ============================================================================
// 1. HARDWARE INTERFACE DEFINITIONS (REGISTERS)
// ============================================================================
struct MailboxRegisters {
    volatile uint32_t command; // 1 = START, 2 = RESET
    volatile uint32_t status;  // 0 = IDLE, 1 = BUSY, 2 = DATA_READY, 3 = ERROR
    volatile uint64_t address; // Memory address target
};

const uint32_t CMD_START        = 0x1;
const uint32_t CMD_RESET        = 0x2;

const uint32_t STATUS_IDLE      = 0x0;
const uint32_t STATUS_BUSY      = 0x1;
const uint32_t STATUS_DATA_RDY  = 0x2;
const uint32_t STATUS_ERROR     = 0x3;

// ============================================================================
// 2. HARDWARE SIMULATOR (PRIVATE INTERNAL STATE)
// ============================================================================
struct HardwareSimulator {
    MailboxRegisters regs;      
    uint32_t cycles_remaining;  
};

void hardware_tick(HardwareSimulator& hw) {
    // 1. Handle a Reset Command instantly (Global Asynchronous Event)
    if (hw.regs.command == CMD_RESET) {
        hw.regs.status = STATUS_IDLE;
        hw.regs.command = 0; 
        hw.cycles_remaining = 0;
        std::cout << "  [HW] Reset executed. System IDLE.\n";
        return;
    }

    // 2. FSM State Transition & Action Logic
    switch (hw.regs.status) {
        case STATUS_IDLE:
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_BUSY;
                hw.regs.command = 0; 
                hw.cycles_remaining = 5; // 5 cycle latency
                std::cout << "  [HW] Transfer started. Latency set to 5 cycles.\n";
            }
            break;

        case STATUS_BUSY:
            // TRAP CHECK: Command issued while busy = HANG/ERROR!
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_ERROR;
                hw.regs.command = 0;
                std::cout << "  [HW] CRITICAL ERROR: Write occurred during BUSY state! Hardware HUNG.\n";
            } else {
                if (hw.cycles_remaining > 0) {
                    hw.cycles_remaining--;
                    std::cout << "  [HW] Processing... Cycles remaining: " << hw.cycles_remaining << "\n";
                }
                if (hw.cycles_remaining == 0) {
                    hw.regs.status = STATUS_DATA_RDY;
                    std::cout << "  [HW] Data transfer complete. Status: DATA_READY.\n";
                }
            }
            break;

        case STATUS_DATA_RDY:
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_BUSY;
                hw.regs.command = 0; 
                hw.cycles_remaining = 5;
                std::cout << "  [HW] Transfer started. Latency set to 5 cycles.\n";
            }
            break;

        case STATUS_ERROR:
            // Remain in ERROR state until reset
            break;
    }
}

// ============================================================================
// 3. FIRMWARE DRIVER
// ============================================================================
enum class DriverState {
    READY,
    TRANSFER_IN_PROGRESS,
    DATA_AVAILABLE,
    RECOVERING_FROM_RESET,
    FATAL_ERROR
};

// Hardware registers track the physical state of the bus/silicon, whereas Driver enums track the logical state of the software transaction. 
// Tying them 1-to-1 fails because software transactions involve preparation, consumption, and recovery phases that the physical hardware is completely blind to.
static DriverState g_driver_state = DriverState::READY;

std::string get_driver_state_string(DriverState state) {
    switch(state) {
        case DriverState::READY:                  return "READY";
        case DriverState::TRANSFER_IN_PROGRESS:   return "TRANSFER_IN_PROGRESS";
        case DriverState::DATA_AVAILABLE:         return "DATA_AVAILABLE";
        case DriverState::RECOVERING_FROM_RESET:  return "RECOVERING_FROM_RESET";
        case DriverState::FATAL_ERROR:            return "FATAL_ERROR";
        default:                                  return "UNKNOWN";
    }
}

bool mailbox_request_transfer(MailboxRegisters& regs, uint64_t target_address) {
    if (g_driver_state != DriverState::READY) {
        return false; 
    }

    // DEFENSIVE CHECK: Software state machine believes the driver is READY, but the
    // physical hardware register reports BUSY. This mismatch indicates a race condition
    // or hardware glitch. Writing CMD_START now would trigger a fatal hardware hang.
    // Transition to FATAL_ERROR to lock down the driver and protect the system.
    if (regs.status == STATUS_BUSY) {
        // Recovery from FATAL_ERROR is non-autonomous. The driver remains locked in 
        // this state until a supervisor performs a hardware reset and re-initializes 
        // the driver via mailbox_init().
        g_driver_state = DriverState::FATAL_ERROR;
        return false;
    }

    regs.address = target_address;
    regs.command = CMD_START;
    g_driver_state = DriverState::TRANSFER_IN_PROGRESS;
    return true; 
}

void mailbox_process_events(MailboxRegisters& regs) {
    switch (g_driver_state) {
        case DriverState::TRANSFER_IN_PROGRESS:
            if (regs.status == STATUS_DATA_RDY) {
                g_driver_state = DriverState::DATA_AVAILABLE;
            } else if (regs.status == STATUS_ERROR) {
                std::cout << "  [FW] Detected Hardware Error! Issuing Reset...\n";
                regs.command = CMD_RESET;
                g_driver_state = DriverState::RECOVERING_FROM_RESET;
            }
            break;

        case DriverState::RECOVERING_FROM_RESET:
            if (regs.status == STATUS_IDLE) {
                g_driver_state = DriverState::READY;
                std::cout << "  [FW] Hardware recovered successfully.\n";
            }
            break;

        case DriverState::DATA_AVAILABLE:
            // Workload consumes data, clears state back to ready
            std::cout << "  [FW] Data consumed by upstream software.\n";
            g_driver_state = DriverState::READY;
            break;

        default:
            break;
    }
}

// ============================================================================
// 4. TEST BENCH EXECUTION LOOP
// ============================================================================
int main() {
    HardwareSimulator my_hw = { {0, STATUS_IDLE, 0}, 0 };

    std::cout << "Starting Asynchronous System Simulation...\n\n";

    for (int cycle = 0; cycle < 10; cycle++) {
        std::cout << "=== CYCLE " << cycle << " ===\n";
        std::cout << "  [FW State] Before processing: " << get_driver_state_string(g_driver_state) << "\n";

        // Step A: Run Firmware Housekeeping
        mailbox_process_events(my_hw.regs);

        // Step B: Trigger an action at Cycle 1
        if (cycle == 1) {
            std::cout << "  [App] Requesting data transfer via Firmware...\n";
            bool success = mailbox_request_transfer(my_hw.regs, 0x00FFBEEF);
            std::cout << "  [App] Request status: " << (success ? "ACCEPTED" : "REJECTED") << "\n";
        }

        // Step C: Step the physical hardware clock forward
        hardware_tick(my_hw);

        std::cout << "  [FW State] After cycle: " << get_driver_state_string(g_driver_state) << "\n\n";
    }

    return 0;
}
```

---

## 8. Summary: Reusable Interview Blueprint

This structure scales easily to any hardware modeling question (such as a NIC ring buffer, DMA controller, or SPI peripheral). Always break your answer down into this 4-part layout:

1. **The Registry (`struct` of volatile primitives):** Represents the hardware-exposed registers (MMIO contract), decoupled from software structures.
2. **The Simulator State (`struct` + `tick()` function):** Represents physical hardware behavior over time (latency, internal status, errors).
3. **The Driver State Machine (`enum` + event loop):** Implements non-blocking, asynchronous behavior to ensure the CPU never wastes cycles busy-waiting.
4. **The Test Bench (`main` loop):** Traces system behavior line-by-line under simulated workloads to verify that all hardware constraints are respected.

---

## 9. Follow-up: Clearing the Hardware Status after Data Consumption

### The Question
After the Upstream software consumes the data, should we be transitioning the hardware register (`regs.status`) back to `STATUS_IDLE`?

### The Answer & Architecture Review
Yes. In a real-world system, once the software consumes the data, the hardware status register should transition back to `STATUS_IDLE` (or a similar empty state) to accurately reflect that the mailbox has been cleared and is ready for the next command. 

Leaving the hardware status at `STATUS_DATA_RDY` after consumption is a simplification of this mock model. In the current simulation, the status is only reset back to `STATUS_BUSY` when a new transaction is started (`CMD_START`), which is functional for a continuous stream of transactions but doesn't handle the intermediate idle state properly.

### Real Caliptra Behavior
In the actual Caliptra mailbox hardware:
- The receiver writes `CmdComplete` or `CmdFailure` to the mailbox `status` register.
- The sender reads this status and then writes `unlock = true` to the mailbox `unlock` register, which resets the mailbox FSM back to `MboxIdle` (as seen in [`MailboxSendTxn::drop`](file:///usr/local/google/home/saumilshah/repos/ironheart/caliptra-sw/drivers/src/mailbox.rs#L247-L257)).

### Refined Simulation Design (Acknowledge Command Pattern)
To model this transition properly, we can introduce a new acknowledgement command (`CMD_ACK`) from the software driver to release the hardware.

1. **Define the Ack Command:**
   ```cpp
   const uint32_t CMD_ACK = 0x3;
   ```

2. **Update the Hardware Simulator to transition back to IDLE on ACK:**
   ```cpp
   case STATUS_DATA_RDY:
       if (hw.regs.command == CMD_ACK) {
           hw.regs.status = STATUS_IDLE;
           hw.regs.command = 0;
           std::cout << "  [HW] Acknowledge received. System IDLE.\n";
       } else if (hw.regs.command == CMD_START) {
           hw.regs.status = STATUS_BUSY;
           hw.regs.command = 0; 
           hw.cycles_remaining = 5;
           std::cout << "  [HW] Transfer started. Latency set to 5 cycles.\n";
       }
       break;
   ```

3. **Update the Driver State Machine to send ACK after consumption:**
   ```cpp
   case DriverState::DATA_AVAILABLE:
       // Workload consumes data, notifies hardware, and clears state back to ready
       std::cout << "  [FW] Data consumed by upstream software. Sending ACK.\n";
       regs.command = CMD_ACK;
       g_driver_state = DriverState::READY;
       break;
   ```

