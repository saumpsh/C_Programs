# Stage 6: Attack Walkthroughs & Validations

This stage validates the architecture against 13 concrete attack vectors using the [Stage 2 Invariants](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage2.md#2-the-6-core-security-invariants).

---

## Attack Walkthrough Micro-Cards

### 1. Malicious Host Submits Cross-Tenant DMA Pointer
* **Invariant at Risk**: *Invariant 1 (Command Isolation)*
* **Prevention**: Commands specify opaque buffer handles, not raw addresses. Security Monitor resolves handles against internal context maps and checks integer-overflow bounds. Platform IOMMU restricts host page access as defense-in-depth.
* **Residual Risk**: Host-controlled IOMMU cannot be trusted alone; device-side checks remain primary.

### 2. Tenant Context Handle Guessing / Replay
* **Invariant at Risk**: *Invariant 1 (Command Isolation) & Invariant 4 (Zero-Leakage)*
* **Prevention**: Context handles are 128-bit high-entropy random values bound to specific queue IDs and generation numbers. Handle invalidation occurs instantly upon context teardown.
* **Residual Risk**: Entropy generation relies on robust hardware RNG.

### 3. Replay of Expired Tenant Authorization Token
* **Invariant at Risk**: *Invariant 5 (Management Freshness)*
* **Prevention**: Tokens contain short expiration timestamps, device binding, and unique token IDs tracked in a sliding sequence window.
* **Residual Risk**: Requires coarse clock synchronization between fleet manager and device.

### 4. Technician Board Tapping / Flash Extraction
* **Invariant at Risk**: *Invariant 3 (Secret Non-Exportability)*
* **Prevention**: External Flash storage encrypted; keys stored inside on-chip PUF/OTP vault. Production fuses permanently disable JTAG/UART headers.
* **Residual Risk**: Invasive silicon decapping attacks remain out of scope.

### 5. Attempted Firmware Downgrade Attack
* **Invariant at Risk**: *Invariant 2 (Code Authenticity)*
* **Prevention**: Boot ROM checks binary version against OTP monotonic counter fuses. Flash write rejected if version < minimum floor.
* **Residual Risk**: Emergency rollback windows must be tightly time-bounded.

### 6. Compromised Release Signing Key
* **Invariant at Risk**: *Invariant 2 (Code Authenticity)*
* **Prevention**: Boot ROM verifies against offline root keys. Intermediate release keys scoped by device family. Fleet verifier enforces measurement allowlists.
* **Residual Risk**: Staged rollouts required to contain compromised release binaries before fleet-wide deployment.

### 7. Workload Crash on Shared Compute Engine
* **Invariant at Risk**: *Invariant 4 (Zero-Leakage) & System Availability*
* **Prevention**: Hardware watchdog attributes fault to context ID. Resets specific execution pipeline, disables DMA, and zeroizes context memory before engine re-allocation.
* **Residual Risk**: Unrecoverable hardware state faults escalate to full device reset.

### 8. Power Loss Mid-Firmware Update
* **Invariant at Risk**: *Invariant 2 (Code Authenticity)*
* **Prevention**: Updates written strictly to inactive A/B slot. Active slot remains default until Slot B signature and self-tests pass atomically.
* **Residual Risk**: Dual slot failure drops device into Quarantine Mode.

### 9. Vulnerability in Validly Signed Firmware
* **Invariant at Risk**: *Invariant 1 & Invariant 2*
* **Prevention**: Security Monitor isolated from model execution firmware. Hardware MMU enforces context bounds independent of firmware correctness.
* **Residual Risk**: Firmware vulnerabilities within the Security Monitor itself require rapid patch rollout.

### 10. Control Plane Attestation Outage
* **Invariant at Risk**: Availability vs *Invariant 6 (Authentic Admission)*
* **Prevention**: Existing active tenant contexts continue running under bounded grace periods. New context creation and firmware updates fail closed.
* **Residual Risk**: Revocation delays during control plane connectivity loss.

### 11. Compromised Management Service Submits Resets
* **Invariant at Risk**: *Invariant 5 (Management Freshness)*
* **Prevention**: Role-separated credentials (scheduler vs. update service). Local policy engine blocks destructive commands without multi-party approval.
* **Residual Risk**: Single compromised scheduler credential limited to context manipulation.

### 12. Cross-Tenant Timing Side-Channel Inference
* **Invariant at Risk**: *Invariant 1 (Command Isolation)*
* **Prevention**: Partitioned execution modes, restricted global performance counters, per-tenant queue scheduling.
* **Residual Risk**: Bounded timing leakage acceptable in standard shared tier; dedicated cards offered for high-security workloads.

### 13. Output Routing Mix-Up Bug
* **Invariant at Risk**: *Invariant 1 (Command Isolation)*
* **Prevention**: Context tags attached to data headers throughout internal pipelines. Completion queue writes validated against context ID.
* **Residual Risk**: Software queue corruption caught by hardware tag mismatch.
