# Stage 5: High-Level Architecture & Lifecycle Flows

This stage details the 12 end-to-end device lifecycle state transitions, from factory manufacturing to decommissioning.

---

## 1. Lifecycle State Machine Overview

```text
Manufacture (OTP Fuse) ──> Measured Boot ──> Remote Attestation ──> Fleet Admission
                                 │                                       │
                                 ▼                                       ▼
                         Quarantine Mode <── Fault / Fail ── Context Creation & DMA
                                 │                                       │
                                 ▼                                       ▼
                         Authenticated Recovery <────────── Teardown / Zeroization
```

---

## 2. Detailed Lifecycle Sequence Flows

### Flow 1: Manufacturing & Identity Provisioning
1. Device powers up in `FACTORY` lifecycle mode.
2. Hardware RNG / PUF generates a Device Unique Secret (DUS) on-chip (never exported).
3. Device derives its public identity key; manufacturer signs the certificate chain.
4. Production Root Public Key Hash and initial version fuses are burned into OTP memory.
5. Lifecycle state irreversibly transitions to `PRODUCTION` (locking JTAG/UART debug ports).

### Flow 2: Verified Boot Chain
1. Reset vector starts execution inside the **Immutable Boot ROM**.
2. Boot ROM reads OTP version fuses and loads the First-Stage Bootloader.
3. Boot ROM verifies signature and checks version $\ge \text{OTP Minimum Floor}$.
4. First-Stage Bootloader loads and verifies Security Monitor and Firmware.
5. Execution transfers to Security Monitor; PCIe DMA remains disabled until initialization completes.

### Flow 3: Measured Boot & Remote Attestation
1. Each boot stage extends execution hashes into protected measurement registers.
2. Control plane requests attestation by sending a fresh **random nonce**.
3. Device signs an Attestation Report containing measurement registers, firmware version, lifecycle state, and nonce using its private attestation key.
4. Verifier validates signature, nonce freshness, and allows fleet admission.

### Flow 4: Management Session Establishment
1. Control plane and device establish an authenticated mTLS control channel over PCIe.
2. Management commands require: Target Device ID, Function Scope, Monotonic Counter, and Expiration Timestamp.
3. Device Security Monitor enforces local policy before executing management actions.

### Flow 5: Tenant Context Creation
1. Control plane issues a signed, short-lived Tenant Authorization Token.
2. Host passes token to device. Device Security Monitor verifies token signature and expiration.
3. Device allocates a unique **Opaque Context Handle**, queues, and memory quotas.
4. Device returns handle to host. (Host cannot modify context attributes).

### Flow 6: Buffer Registration & Context-Scoped DMA
1. Host requests registration of host DRAM pages for a valid context handle.
2. Device validates address ranges, lengths, and alignment to prevent integer overflows.
3. Device records context-scoped mapping internally; platform IOMMU configures host-side page tables.
4. Commands specify opaque buffer handles rather than raw physical host addresses.

### Flow 7: Normal Workload Execution
1. Host app enqueues workload descriptor referencing context handle and buffer handles.
2. Device fetches descriptor via DMA; Command Validator checks opcode validity and buffer ownership.
3. Scheduler dispatches work with context tags attached to execution pipelines.
4. Device MMU restricts reads/writes strictly to authorized onboard DRAM pages.

### Flow 8: Context Teardown & Zeroization
1. Context teardown triggered (completion, expiration, error, host request).
2. Device stops accepting commands for context handle; drains active compute pipelines.
3. DMA engine disabled for context mappings; context handle invalidated.
4. Hardware scrub engine overwrites allocated DRAM/caches OR cryptographic key is destroyed.

### Flow 9: Reset & Fault Recovery
1. Fault or reset signal triggered (watchdog timer, host reset, exception).
2. Hardware instantly disables PCIe DMA and revokes memory access.
3. Volatile key registers zeroized; device re-enters Verified Boot.
4. Re-attestation required before accepting new tenant workloads.

### Flow 10: Authenticated A/B Firmware Update
1. Control plane pushes signed update package to **inactive slot (Slot B)**.
2. Device verifies signature and version policy before writing.
3. Device reboots into Slot B; Boot ROM re-verifies Slot B signature.
4. Slot B runs self-tests and produces new attestation report.
5. Fleet control plane confirms health; minimum version fuse burned only after canary validation.

### Flow 11: Quarantine Mode
1. Triggered on boot signature mismatch, attestation failure, or unrecoverable memory fault.
2. Device disables tenant workload APIs and host DMA engine.
3. Accepts only signed recovery images and exports diagnostic security telemetry.

### Flow 12: Device Decommissioning
1. Control plane revokes device identity certificate in fleet registry.
2. Device executes cryptographic erase of nonvolatile flash and persistent keys.
3. On-chip persistent identity destroyed; card marked for physical disposal.
