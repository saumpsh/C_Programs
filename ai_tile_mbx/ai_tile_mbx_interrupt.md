# The AI Tile Mailbox Controller: Interrupt-Driven Design Guide

This guide describes how to evolve the AI Tile Mailbox Controller from a polling-based design to an interrupt-driven architecture. This is a common progression in hardware-software co-design, demonstrating how to handle asynchronous hardware events efficiently without wasting CPU cycles.

---

## 1. Architectural Shift: Polling vs. Interrupts

In the polling design, the firmware driver must periodically check the `status` register (busy-waiting or polling loop). 

With interrupts, the hardware actively notifies the CPU when a state transition occurs (e.g., `DATA_READY` or `ERROR`).
1. **Interrupt Enable Register (`intr_enable`):** Allows firmware to mask/unmask interrupts.
2. **Interrupt Status Register (`intr_status`):** Indicates which event triggered the interrupt. Typically cleared by the firmware writing a `1` to the active bit (Write-1-to-Clear or W1C).
3. **Interrupt Service Routine (ISR):** An asynchronous handler invoked by the CPU vector table when the hardware asserts the interrupt line.

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
|   |                        FIRMWARE DRIVER (Interrupt Safe)                       |   |
|   |                                                                               |   |
|   |   +--------------------------+       +------------------------------------+   |   |
|   |   | Driver State (volatile)  |       | Interrupt Service Routine (ISR)    |   |   |
|   |   | [INIT/READY/BUSY/READY]  |       | (Handles async HW notifications)   |   |   |
|   |   +------------^-------------+       +------------------^-----------------+   |   |
|   +----------------|----------------------------------------|---------------------+   |
|                    |                                        |                         |
|                    | 2. MMIO Write                          | 4. Hardware IRQ         |
|                    |    (Start transfer)                    |    (Asynchronous)       |
|                    v                                        |                         |
+--------------------|----------------------------------------|-------------------------+
                     |                                        |
                     v                                        |
+-------------------------------------------------------------|-------------------------+
|                                    MAILBOX HARDWARE         |                         |
|                                                             |                         |
|   +--------------------+  +--------------------+  +---------|----------+  +-------+   |
|   |  Command Register  |  |   Status Register  |  | Interrupt Registers |  | SRAM  |   |
|   |    [ START BIT ]   |  | [IDLE/BUSY/READY?] |  | [ ENABLE / STATUS ] |  | (64K) |   |
|   +--------------------+  +--------------------+  +---------------------+  +-------+   |
+---------------------------------------------------------------------------------------+
```

---

## 2. Hardware Interface & Register Map

We extend the hardware MMIO interface to expose interrupt configuration and status.

```cpp
#include <cstdint>

// Interrupt status/enable bitmasks
const uint32_t INTR_READY_BIT = (1 << 0); // Bit 0: Data transfer complete (DATA_READY)
const uint32_t INTR_ERROR_BIT = (1 << 1); // Bit 1: Protocol error / Hardware hang (ERROR)

// Updated Hardware-Facing Interface (Registers)
struct MailboxRegisters {
    volatile uint32_t command;     // Write 1 = START, Write 2 = RESET
    volatile uint32_t status;      // Reads: 0=IDLE, 1=BUSY, 2=DATA_READY, 3=ERROR
    volatile uint64_t address;     // Memory address target
    
    // --- NEW INTERRUPT REGISTERS ---
    volatile uint32_t intr_enable; // RW: Enable/disable interrupts (bitmask)
    volatile uint32_t intr_status; // RW1C: Reads active interrupts. Write 1 to clear.
};

const uint32_t CMD_START        = 0x1;
const uint32_t CMD_RESET        = 0x2;

const uint32_t STATUS_IDLE      = 0x0;
const uint32_t STATUS_BUSY      = 0x1;
const uint32_t STATUS_DATA_RDY  = 0x2;
const uint32_t STATUS_ERROR     = 0x3;
```

---

## 3. Hardware Simulator Updates

In the hardware simulator, we model the physical interrupt line. When the status transitions, if the corresponding bit in `intr_enable` is unmasked, we set the bit in `intr_status` and trigger the CPU interrupt signal.

```cpp
struct HardwareSimulator {
    MailboxRegisters regs;      
    uint32_t cycles_remaining;  
    bool irq_line; // Simulates the physical hardware IRQ pin (true = high/asserted)
};

// Forward declaration of the CPU interrupt handler trigger
void trigger_cpu_interrupt();

void hardware_tick(HardwareSimulator& hw) {
    if (hw.regs.command == CMD_RESET) {
        hw.regs.status = STATUS_IDLE;
        hw.regs.command = 0; 
        hw.cycles_remaining = 0;
        hw.regs.intr_status = 0; // Clear outstanding interrupts
        hw.irq_line = false;
        return;
    }

    switch (hw.regs.status) {
        case STATUS_IDLE:
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_BUSY;
                hw.regs.command = 0; 
                hw.cycles_remaining = 5;
            }
            break;

        case STATUS_BUSY:
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_ERROR;
                hw.regs.command = 0;
                
                // Assert ERROR interrupt if enabled
                if (hw.regs.intr_enable & INTR_ERROR_BIT) {
                    hw.regs.intr_status |= INTR_ERROR_BIT;
                    hw.irq_line = true;
                    trigger_cpu_interrupt();
                }
            } else {
                if (hw.cycles_remaining > 0) {
                    hw.cycles_remaining--;
                }
                if (hw.cycles_remaining == 0) {
                    hw.regs.status = STATUS_DATA_RDY;
                    
                    // Assert READY interrupt if enabled
                    if (hw.regs.intr_enable & INTR_READY_BIT) {
                        hw.regs.intr_status |= INTR_READY_BIT;
                        hw.irq_line = true;
                        trigger_cpu_interrupt();
                    }
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
            break;
    }
}
```

---

## 4. Firmware Driver & Interrupt Service Routine (ISR)

Since the ISR runs asynchronously, the global driver state must be marked `volatile` (or accessed atomically) to prevent compiler optimizations from caching the state.

```cpp
enum class DriverState {
    READY,
    TRANSFER_IN_PROGRESS,
    DATA_AVAILABLE,
    RECOVERING_FROM_RESET,
    FATAL_ERROR
};

// Must be volatile since the ISR updates it asynchronously
static volatile DriverState g_driver_state = DriverState::READY;

// 1. Initialization: Enable interrupts in the Mailbox controller
void mailbox_init(MailboxRegisters& regs) {
    // Enable interrupts for both READY and ERROR states
    regs.intr_enable = (INTR_READY_BIT | INTR_ERROR_BIT);
    g_driver_state = DriverState::READY;
}

// 2. Request Transfer (Non-blocking)
bool mailbox_request_transfer(MailboxRegisters& regs, uint64_t target_address) {
    if (g_driver_state != DriverState::READY) {
        return false; 
    }

    if (regs.status == STATUS_BUSY) {
        g_driver_state = DriverState::FATAL_ERROR;
        return false;
    }

    regs.address = target_address;
    regs.command = CMD_START;
    g_driver_state = DriverState::TRANSFER_IN_PROGRESS;
    return true; 
}

// 3. The Interrupt Service Routine (ISR)
// Called asynchronously by the CPU when the hardware asserts the interrupt line.
void mailbox_isr(MailboxRegisters& regs) {
    uint32_t active_interrupts = regs.intr_status;

    // Handle Transfer Ready
    if (active_interrupts & INTR_READY_BIT) {
        g_driver_state = DriverState::DATA_AVAILABLE;
        
        // Write 1 to clear (acknowledging the interrupt)
        regs.intr_status = INTR_READY_BIT; 
    }

    // Handle Hardware Error / Hang
    if (active_interrupts & INTR_ERROR_BIT) {
        regs.command = CMD_RESET;
        g_driver_state = DriverState::RECOVERING_FROM_RESET;
        regs.intr_status = INTR_ERROR_BIT; 
    }
}
```

---

## 5. Temporal Integration Trace (Interrupt Execution)

Let's examine how the system behaves without polling:

* **Cycle 1 (The Request):**
  * Application requests transfer using `mailbox_request_transfer()`.
  * Driver state changes to `TRANSFER_IN_PROGRESS`.
  * Hardware transitions to `STATUS_BUSY`.
* **Cycles 2-5:**
  * The CPU performs unrelated tasks (e.g., preparing the next AI kernel).
  * Hardware ticks down.
* **Cycle 6 (The Completion & Interrupt):**
  * Hardware completes transfer. Status transitions to `STATUS_DATA_RDY`.
  * Hardware checks `intr_enable`, sets `intr_status = INTR_READY_BIT`, and asserts the IRQ line.
  * The CPU halts normal execution, jumps to `mailbox_isr()`.
  * ISR reads `regs.intr_status`, sets `g_driver_state = DriverState::DATA_AVAILABLE`, writes `1` to clear `regs.intr_status`, and returns.
  * The CPU resumes its normal workload.

### Visual Comparison: Polling vs. Interrupt CPU Utilization

**Polling:**
```text
CPU Timeline: [ FW Request ] -> [ Poll ] -> [ Poll ] -> [ Poll ] -> [ Poll ] -> [ Data Ready! ]
```

**Interrupt:**
```text
CPU Timeline: [ FW Request ] -> [ Run AI App ] -> [ Run AI App ] -> [ Run AI App ] -IRQ-> [ ISR: Data Ready! ]
```

---

## 6. Complete Simulation Code (C++)

This complete code models the interrupt mechanism using a synchronous function callback to represent the hardware IRQ line.

```cpp
#include <iostream>
#include <cstdint>
#include <string>

// ============================================================================
// Register Definitions
// ============================================================================
const uint32_t INTR_READY_BIT = (1 << 0);
const uint32_t INTR_ERROR_BIT = (1 << 1);

struct MailboxRegisters {
    volatile uint32_t command;     
    volatile uint32_t status;      
    volatile uint64_t address;     
    volatile uint32_t intr_enable; 
    volatile uint32_t intr_status; 
};

const uint32_t CMD_START        = 0x1;
const uint32_t CMD_RESET        = 0x2;

const uint32_t STATUS_IDLE      = 0x0;
const uint32_t STATUS_BUSY      = 0x1;
const uint32_t STATUS_DATA_RDY  = 0x2;
const uint32_t STATUS_ERROR     = 0x3;

// ============================================================================
// Driver State and Declarations
// ============================================================================
enum class DriverState {
    READY,
    TRANSFER_IN_PROGRESS,
    DATA_AVAILABLE,
    RECOVERING_FROM_RESET,
    FATAL_ERROR
};

static volatile DriverState g_driver_state = DriverState::READY;
static MailboxRegisters g_regs = {0, STATUS_IDLE, 0, 0, 0};

void mailbox_isr();

// Simulates CPU jump to ISR vector
void trigger_cpu_interrupt() {
    std::cout << "  [CPU] <<< INTERRUPT TRIGGERED! Jumping to ISR >>>\n";
    mailbox_isr();
}

// ============================================================================
// Hardware Simulator
// ============================================================================
struct HardwareSimulator {
    MailboxRegisters& regs;      
    uint32_t cycles_remaining;  
    bool irq_line;
};

void hardware_tick(HardwareSimulator& hw) {
    if (hw.regs.command == CMD_RESET) {
        hw.regs.status = STATUS_IDLE;
        hw.regs.command = 0; 
        hw.cycles_remaining = 0;
        hw.regs.intr_status = 0;
        hw.irq_line = false;
        std::cout << "  [HW] Reset executed. System IDLE.\n";
        return;
    }

    switch (hw.regs.status) {
        case STATUS_IDLE:
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_BUSY;
                hw.regs.command = 0; 
                hw.cycles_remaining = 3; // 3-cycle latency for demo
                std::cout << "  [HW] Transfer started. Latency: 3 cycles.\n";
            }
            break;

        case STATUS_BUSY:
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_ERROR;
                hw.regs.command = 0;
                std::cout << "  [HW] Error: Command written during BUSY!\n";
                if (hw.regs.intr_enable & INTR_ERROR_BIT) {
                    hw.regs.intr_status |= INTR_ERROR_BIT;
                    hw.irq_line = true;
                    trigger_cpu_interrupt();
                }
            } else {
                if (hw.cycles_remaining > 0) {
                    hw.cycles_remaining--;
                    std::cout << "  [HW] Processing... Cycles left: " << hw.cycles_remaining << "\n";
                }
                if (hw.cycles_remaining == 0) {
                    hw.regs.status = STATUS_DATA_RDY;
                    std::cout << "  [HW] Data transfer complete. Status set to DATA_READY.\n";
                    if (hw.regs.intr_enable & INTR_READY_BIT) {
                        hw.regs.intr_status |= INTR_READY_BIT;
                        hw.irq_line = true;
                        trigger_cpu_interrupt();
                    }
                }
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// Firmware Implementation
// ============================================================================
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

void mailbox_init() {
    g_regs.intr_enable = (INTR_READY_BIT | INTR_ERROR_BIT);
    g_driver_state = DriverState::READY;
}

bool mailbox_request_transfer(uint64_t target_address) {
    if (g_driver_state != DriverState::READY) {
        return false; 
    }
    if (g_regs.status == STATUS_BUSY) {
        g_driver_state = DriverState::FATAL_ERROR;
        return false;
    }

    g_regs.address = target_address;
    g_regs.command = CMD_START;
    g_driver_state = DriverState::TRANSFER_IN_PROGRESS;
    return true; 
}

void mailbox_isr() {
    uint32_t active = g_regs.intr_status;

    if (active & INTR_READY_BIT) {
        std::cout << "  [ISR] Servicing READY interrupt.\n";
        g_driver_state = DriverState::DATA_AVAILABLE;
        g_regs.intr_status = INTR_READY_BIT; // W1C
    }

    if (active & INTR_ERROR_BIT) {
        std::cout << "  [ISR] Servicing ERROR interrupt. Initiating recovery.\n";
        g_regs.command = CMD_RESET;
        g_driver_state = DriverState::RECOVERING_FROM_RESET;
        // The CPU writes 1 to the bit index of the READY interrupt. The physical hardware register logic receives the write data.
        // Because the register is designed as Write-1-to-Clear (W1C), the hardware computes:
        // New State = 1 (Current State) AND NOT 1 (CPU Write) -> 0
        // The hardware flip-flop is immediately reset to 0. The hardware drops the physical IRQ line back to LOW.
        g_regs.intr_status = INTR_ERROR_BIT; // W1C
    }
}

// Simulates the consumer consuming the data and clearing the state
void mailbox_consume_data() {
    if (g_driver_state == DriverState::DATA_AVAILABLE) {
        std::cout << "  [FW] Data consumed by upstream workload.\n";
        g_driver_state = DriverState::READY;
    }
}

// ============================================================================
// Execution
// ============================================================================
int main() {
    HardwareSimulator my_hw = { g_regs, 0, false };
    
    std::cout << "Initializing System...\n";
    mailbox_init();

    for (int cycle = 0; cycle < 8; cycle++) {
        std::cout << "=== CYCLE " << cycle << " ===\n";
        std::cout << "  [FW State] " << get_driver_state_string(g_driver_state) << "\n";

        // Request a transfer on cycle 1
        if (cycle == 1) {
            std::cout << "  [App] Requesting data transfer...\n";
            mailbox_request_transfer(0x00FFBEEF);
        }

        // Consume data on cycle 6 if available
        if (cycle == 6) {
            mailbox_consume_data();
        }

        hardware_tick(my_hw);
        std::cout << "\n";
    }

    return 0;
}
```
