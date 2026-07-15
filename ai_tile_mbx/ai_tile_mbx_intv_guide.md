# Focus items

Given a tight 45-minute window and a candidate who takes time to internalize requirements, time management and prioritization are critical. Focusing on the wrong things will definitely act as a negative signal, especially if the interviewer feels the candidate is missing the "big picture" of embedded constraints.

Here is a breakdown of exactly where a candidate should focus, what should be written in real code, what should be pseudo-coded, and what traps to avoid.

### 1. What to Focus On (The "Must-Haves")
The interviewer is looking for **safety, state management, and an understanding of physical boundaries**. The candidate should spend their mental energy on:
*   **Separation of Concerns:** Clearly separating what the Hardware "sees" (Registers + Simulation State) from what the Firmware "sees" (Registers + Driver State). 
*   **Non-Blocking Logic:** Ensuring the CPU never gets stuck in a `while()` loop waiting for the hardware.
*   **Defensive Checks:** Proving they understand the constraint: *"If I write while it's BUSY, it hangs. Therefore, my driver must check state before writing."*

### 2. What to Write Actual Code For (C/C++)
The candidate should write concrete code for the structures and the control flow, because this proves they understand how to represent state in a low-level environment.
*   **Structs and Enums (Crucial):** Write actual C-style `struct` definitions for the Registers, the Simulator State, and an `enum` for the Driver State. This is fast to write and immediately proves you understand memory boundaries.
*   **The FW Driver Guard Clause:** Write the actual logic for `request_data()`. It should be brief: check FW state, check HW register, write `START`, return success/fail. 
*   **The State Machine (`switch` statement):** Write the actual `switch` or `if/else` block for how the Firmware transitions from `BUSY` -> `READY` -> `ERROR`.

### 3. What to Write Pseudo-Code For (Save Time Here)
The interviewer already knows the candidate can write a `for` loop. Do not waste time on boilerplate.
*   **The Hardware Simulator (`tick` function):** You can write pseudo-code here: `if (command == START) { cycles = 5; status = BUSY; }`. 
*   **Queues/Buffers:** If you propose a software queue to absorb back-to-back requests, **do not implement the ring buffer.** Just write `queue.push(request)` and say, *"Assuming a standard FIFO implementation here."*
*   **The Test Bench / Execution Trace:** Do not write a `main()` function with `std::cout` print statements. Instead, use verbal tracing or comment blocks: *"Cycle 1: FW writes START. Cycle 2-6: HW decrements. Cycle 7: FW reads READY."*

### 4. Will focusing on the incorrect things negatively impact the candidate?
**Yes, heavily.** In embedded and HW/SW co-design interviews, focusing on the wrong abstraction level is a massive red flag. 

Here are the specific "incorrect things" that will negatively impact the score:
*   **Negative Signal 1: Writing a blocking loop.** If the candidate writes `while(regs.status == BUSY) {}` when the prompt asked for "non-blocking", they fail the core requirement of the question. 
*   **Negative Signal 2: Over-engineering C++ features.** If a candidate spends 10 minutes writing template classes, smart pointers, or virtual interfaces for the Mailbox instead of solving the temporal logic, the interviewer will assume they don't understand low-level C and memory-mapped IO.
*   **Negative Signal 3: Mixing HW and SW state.** If the candidate creates one massive `class Mailbox` that contains both the `cycles_remaining` (HW reality) and the `driver_status` (FW tracking), they will be heavily penalized for failing to understand physical boundaries. Firmware cannot read the hardware's internal clock cycles.
*   **Negative Signal 4: Perfecting syntax over logic.** If the candidate gets stuck trying to remember the exact syntax for a `volatile uint32_t*` pointer cast, they are wasting time. They should just say *"I'm assuming `regs` maps to the MMIO address"* and move on.

### Summary Strategy for the Candidate
1. **Spend the first 5-8 minutes talking:** Draw the boxes. Ask, *"Can I assume I have a periodic timer interrupt to check the status?"* Confirm the constraints.
2. **Write the Structs/Enums (5 mins):** Define the boundaries.
3. **Write the core Firmware logic (15 mins):** Write the non-blocking state machine.
4. **Pseudo-code the rest (10 mins):** Sketch the HW simulator and talk through the execution trace.

# CPU simuation

### How to handle the "CPU" in the interview instead
In a 45-minute interview, **the CPU is just your laptop.** 

You do not need to simulate a RISC-V or ARM core. The "firmware" is just standard C/C++ code executing sequentially in your `main()` function.

If you look at **Stage 5** of the `ai_tile_mbx.md` blueprint, you'll see the perfect example of how to handle the CPU in an interview:

```cpp
// 4. TEST BENCH EXECUTION LOOP
int main() {
    HardwareSimulator my_hw = { ... };

    for (int cycle = 0; cycle < 10; cycle++) {
        // Step A: The "CPU" runs the firmware loop
        mailbox_process_events(my_hw.regs);

        // Step B: The "CPU" tries to request data
        if (cycle == 1) {
             mailbox_request_transfer(my_hw.regs, 0x00FFBEEF);
        }

        // Step C: The "Hardware" clock ticks forward
        hardware_tick(my_hw);
    }
}
```

In an interview, this simple `for`-loop acts as your ISS. It represents the passage of time, alternately giving execution time to the "Firmware" (your driver code) and the "Hardware" (your simulated hardware tick). 

**The takeaway:** Never worry about writing an ISS or memory-mapped routing buses in a 45-minute interview unless explicitly directed to. Keep everything flat, use a simple `for` loop to represent time, and focus entirely on the logic of the peripheral and the state machine of the driver.