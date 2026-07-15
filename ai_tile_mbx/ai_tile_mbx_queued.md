# The Queued AI Tile Mailbox Controller: Co-Design with Software Buffering

This guide extends the foundational Mailbox Controller scenario to include **asynchronous software command queueing**. It is designed to model real-world scenarios where compute tiles emit bursts of requests faster than hardware memory interfaces can process them, without causing hardware lockups, registry busy-waits, or dropping commands.

---

## 1. The Scenario: "The Queued Mailbox"

### The Setup
To decouple the speed of the **Compute Tile** (which issues memory request bursts) from the speed of the **Global Memory Bus** (which has variable, high latency), we insert a software queue within the firmware driver layer.

```text
+---------------------------------------------------------------------------------------+
|                                     COMPUTE TILE                                      |
|                                                                                       |
|   +---------------------+                                                             |
|   |    AI Kernel / App  |                                                             |
|   +----------|----------+                                                             |
|              |                                                                        |
|              | 1. Request Data Burst (A, B, C)                                        |
|              v                                                                        |
|   +-------------------------------------------------------------------------------+   |
|   |                        FIRMWARE DRIVER (The Guardian)                         |   |
|   |                                                                               |   |
|   |   +----------------------+               +--------------------------------+   |   |
|   |   | Driver State Machine |<------------->|  Static Ring Buffer Queue      |   |   |
|   |   | [READY/TRANSFER/...] |               |  [ Addr A | Addr B | Addr C ]  |   |   |
|   |   +----------------------+               +--------------------------------+   |   |
|   +--------------|-------------------------------------------^--------------------+   |
|                  |                                           |                        |
|                  | 2. Safe MMIO Write (Only if HW is IDLE)   | 4. HW Interrupt        |
|                  |    Pop from Queue when ready              |    or Status Poll      |
| +----------------|-------------------------------------------|----------------------+ |
                   |                                           |
                   v                                           |
+--------------------------------------------------------------|------------------------+
|                                    MAILBOX HARDWARE                                   |
|                                                                                       |
|   +--------------------+      +--------------------+      +-----------------------+   |
|   |  Command Register  |      |  Status Register   |      |  Internal SRAM Buffer |   |
|   |    [ START BIT ]   |      | [IDLE/BUSY/READY?] |      |        (64 KB)        |   |
|   +---------|----------+      +---------^----------+      +-----------^-----------+   |
|             |                           |                             |               |
|             | 3. Triggers HW Logic      | Updates State               | 3b. Pulls     |
|             +---------------------------+----------------+            |     Data      |
+----------------------------------------------------------|------------|---------------+
                                                           v            |
+-----------------------------------------------------------------------|---------------+
|                              GLOBAL MEMORY (Variable Latency Bus)     |               |
|                                                                       |               |
|   +-------------------------------------------------------------------|-----------+   |
|   | Data Array [0x0000 ... 0xFFFF]  ==================================+           |   |
+---------------------------------------------------------------------------------------+
```

### Key System Invariants
1. **Back-to-Back Prevention:** If we write `CMD_START` to the mailbox while the status is `BUSY`, the hardware locks up permanently. The software queue holds pending transactions and only dispatches the next when the hardware register returns to `STATUS_IDLE`.
2. **Transaction Safety:** We do not dequeue a command until we confirm the hardware is not `BUSY`. This prevents transaction loss during unexpected hardware-software state mismatches.
3. **Graceful Escalation:** Transient busy states are handled by waiting and retrying (`break`), while permanent lockups transition the system to a `FATAL_ERROR` for supervisor escalation.

---

## 2. Interface and Data Structure Definitions

To avoid dynamic heap allocations in resource-constrained driver code, we define a static template ring buffer.

```cpp
#include <cstdint>
#include <cstddef>

// 1. Hardware Interface Registers
struct MailboxRegisters {
    volatile uint32_t command; // Write 1 = START, Write 2 = RESET
    volatile uint32_t status;  // Reads: 0=IDLE, 1=BUSY, 2=DATA_READY, 3=ERROR
    volatile uint64_t address; // Dest memory address
};

const uint32_t CMD_START        = 0x1;
const uint32_t CMD_RESET        = 0x2;

const uint32_t STATUS_IDLE      = 0x0;
const uint32_t STATUS_BUSY      = 0x1;
const uint32_t STATUS_DATA_RDY  = 0x2;
const uint32_t STATUS_ERROR     = 0x3;

// 2. Static Circular Queue for Embedded Driver Environment
template <typename T, size_t Capacity>
class StaticQueue {
private:
    T buffer_[Capacity];
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t size_ = 0;

public:
    bool enqueue(const T& item) {
        if (size_ == Capacity) return false; // Full
        buffer_[tail_] = item;
        tail_ = (tail_ + 1) % Capacity;
        size_++;
        return true;
    }

    bool dequeue(T& item) {
        if (size_ == 0) return false; // Empty
        item = buffer_[head_];
        head_ = (head_ + 1) % Capacity;
        size_--;
        return true;
    }

    bool empty() const { return size_ == 0; }
    size_t size() const { return size_; }
    void clear() { head_ = 0; tail_ = 0; size_ = 0; }
};
```

---

## 3. Firmwares State Machine & Event Loop

The driver processes the queue sequentially:

```cpp
enum class DriverState {
    READY,
    TRANSFER_IN_PROGRESS,
    DATA_AVAILABLE,
    RECOVERING_FROM_RESET,
    FATAL_ERROR
};

constexpr size_t SW_QUEUE_CAPACITY = 4;
static StaticQueue<uint64_t, SW_QUEUE_CAPACITY> g_sw_queue;
static uint64_t g_active_address = 0;
static DriverState g_driver_state = DriverState::READY;

// Request non-blockingly (Inserts into software queue)
bool mailbox_request_transfer(MailboxRegisters& regs, uint64_t target_address) {
    if (g_driver_state == DriverState::FATAL_ERROR) {
        return false; 
    }
    return g_sw_queue.enqueue(target_address);
}

// Periodic driver event processor
void mailbox_process_events(MailboxRegisters& regs) {
    switch (g_driver_state) {
        case DriverState::READY:
            if (!g_sw_queue.empty()) {
                // Check if physical hardware is busy before dequeuing
                if (regs.status == STATUS_BUSY) {
                    g_driver_state = DriverState::FATAL_ERROR;
                    break; // Wait and try again next tick
                }

                uint64_t next_address;
                g_sw_queue.dequeue(next_address); // Safe to pop now

                regs.address = next_address;
                regs.command = CMD_START;
                g_active_address = next_address;
                g_driver_state = DriverState::TRANSFER_IN_PROGRESS;
            }
            break;

        case DriverState::TRANSFER_IN_PROGRESS:
            if (regs.status == STATUS_DATA_RDY) {
                g_driver_state = DriverState::DATA_AVAILABLE;
            } else if (regs.status == STATUS_ERROR) {
                regs.command = CMD_RESET;
                g_driver_state = DriverState::RECOVERING_FROM_RESET;
            }
            break;

        case DriverState::RECOVERING_FROM_RESET:
            if (regs.status == STATUS_IDLE) {
                g_driver_state = DriverState::READY;
                g_sw_queue.enqueue(g_active_address); // Re-queue the failed transfer
            }
            break;

        case DriverState::DATA_AVAILABLE:
            // Consumed by app context
            g_driver_state = DriverState::READY;
            break;

        default:
            break;
    }
}
```

---

## 4. Execution Trace: Handling a Burst

Let's walk through what happens when **3 commands** are requested in a burst at Cycle 1.

### Step-by-Step Simulation Code
```cpp
#include <iostream>
#include <string>

struct HardwareSimulator {
    MailboxRegisters regs;      
    uint32_t cycles_remaining;  
};

void hardware_tick(HardwareSimulator& hw) {
    if (hw.regs.command == CMD_RESET) {
        hw.regs.status = STATUS_IDLE;
        hw.regs.command = 0; 
        hw.cycles_remaining = 0;
        std::cout << "  [HW] Reset executed. System IDLE.\n";
        return;
    }

    switch (hw.regs.status) {
        case STATUS_IDLE:
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_BUSY;
                hw.regs.command = 0; 
                hw.cycles_remaining = 2; // Shortened latency for trace readability
                std::cout << "  [HW] Transfer started. Latency: 2 cycles.\n";
            }
            break;

        case STATUS_BUSY:
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_ERROR;
                hw.regs.command = 0;
                std::cout << "  [HW] CRITICAL ERROR: Hardware HUNG.\n";
            } else {
                if (hw.cycles_remaining > 0) {
                    hw.cycles_remaining--;
                    std::cout << "  [HW] Processing... Cycles remaining: " << hw.cycles_remaining << "\n";
                }
                if (hw.cycles_remaining == 0) {
                    hw.regs.status = STATUS_DATA_RDY;
                    std::cout << "  [HW] Data transfer complete. STATUS_DATA_RDY.\n";
                }
            }
            break;

        case STATUS_DATA_RDY:
            if (hw.regs.command == CMD_START) {
                hw.regs.status = STATUS_BUSY;
                hw.regs.command = 0; 
                hw.cycles_remaining = 2;
                std::cout << "  [HW] Transfer started. Latency: 2 cycles.\n";
            }
            break;

        case STATUS_ERROR:
            // Remain in ERROR state until reset
            break;
    }
}

int main() {
    HardwareSimulator my_hw = { {0, STATUS_IDLE, 0}, 0 };

    std::cout << "Starting Queued Mailbox Controller Simulation...\n\n";

    for (int cycle = 0; cycle < 15; cycle++) {
        std::cout << "=== CYCLE " << cycle << " ===\n";

        // Step A: Run Firmware events
        mailbox_process_events(my_hw.regs);

        // Step B: Trigger burst requests at cycle 1
        if (cycle == 1) {
            std::cout << "  [App] Requesting burst: 0xAAAA, 0xBBBB, 0xCCCC\n";
            mailbox_request_transfer(my_hw.regs, 0xAAAA);
            mailbox_request_transfer(my_hw.regs, 0xBBBB);
            mailbox_request_transfer(my_hw.regs, 0xCCCC);
            std::cout << "  [App] Queue size: " << g_sw_queue.size() << "\n";
        }

        // Step C: Advance Hardware Simulator clock
        hardware_tick(my_hw);
    }
    return 0;
}
```

### Trace Walkthrough Logs
* **Cycle 1:**
  * Application requests transfer for `0xAAAA`, `0xBBBB`, and `0xCCCC`.
  * All three are enqueued. `g_sw_queue.size()` is 3.
  * `hardware_tick()` runs and remains `IDLE`.
* **Cycle 2:**
  * `mailbox_process_events()` reads `STATUS_IDLE`. It pops `0xAAAA` from the queue and issues `CMD_START` to register.
  * `hardware_tick()` sees `CMD_START`. Transitions register to `STATUS_BUSY` and starts latency count.
* **Cycle 3:**
  * Driver is `TRANSFER_IN_PROGRESS`. HW register is `STATUS_BUSY`.
  * `hardware_tick()` decrements cycles remaining. Data transfer finishes at the end of the tick. Register transitions to `STATUS_DATA_RDY`.
* **Cycle 4:**
  * `mailbox_process_events()` reads `STATUS_DATA_RDY`. Driver transitions to `DATA_AVAILABLE`.
  * Upstream AI Tile reads the data and clears the driver state to `READY`.
* **Cycle 5:**
  * Driver is `READY` again. It pops `0xBBBB` from the queue and issues `CMD_START` to register.
  * `hardware_tick()` transitions register to `STATUS_BUSY`.
* *The process repeats for `0xBBBB` and `0xCCCC` sequentially, executing the whole workload without lockups or missing requests.*

---

## 5. Architectural blueprint for system recovery

If a device reaches `DriverState::FATAL_ERROR`, it drops register interaction and isolates itself. The following out-of-band **Watchdog Task** or **System Supervisor** handles recovery:

```cpp
void system_supervisor_task(HardwareSimulator& hw) {
    if (g_driver_state == DriverState::FATAL_ERROR) {
        std::cout << "[Supervisor] Alert: Driver FATAL_ERROR. Resetting IP block...\n";

        // 1. Assert physical reset line
        hw.regs.command = CMD_RESET;
        hardware_tick(hw);

        // 2. Clean driver registers & discard pending queue entries
        g_sw_queue.clear();
        g_active_address = 0;
        g_busy_retry_count = 0;

        // 3. Bring driver back online
        g_driver_state = DriverState::READY;
        std::cout << "[Supervisor] System restored to READY.\n";
    }
}
```
