# Stage 3: Threat Actors & Capabilities

This stage defines the threat actors, their granted access levels, target assets, and explicitly out-of-scope capabilities based on [Stage 2 Invariants](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage2.md#2-the-6-core-security-invariants).

---

## 1. Threat Actor Breakdown

### A. Malicious Tenant Workload
* **Capabilities**: Submits arbitrary valid/malformed workload requests; observes outputs, timing, and latency; attempts resource exhaustion.
* **Targets**: Other tenants' models, payloads, and intermediate DRAM buffers (Violates *Invariant 1*).
* **Out of Scope**: Physical board access, direct management interface access.

### B. Compromised Host OS / Driver
* **Capabilities**: Full control of host kernel; arbitrary PCIe queue manipulation; forged DMA descriptors; power cycling card; inspects host memory.
* **Targets**: Device DRAM, cross-tenant DMA injection, device identity keys, unauthorized resets (Violates *Invariants 1 & 3*).
* **Key Enforcement**: The card must enforce context checks internally; **the host driver is untrusted**.

### C. Compromised Firmware Signing Authority
* **Capabilities**: Signs malicious binaries with valid production keys; controls release pipelines.
* **Targets**: Fleet-wide persistent compromise (Violates *Invariant 2*).
* **Mitigation**: Offline root trust anchors, intermediate scoped keys, multi-party approvals, and measurement allowlists.

### D. Physical Technician / Board-Level Attacker
* **Capabilities**: Card removal, tapping JTAG/UART headers, reading external flash chips, inter-card substitution.
* **Targets**: Extracted nonvolatile DRAM/Flash contents, device identity cloning (Violates *Invariants 2 & 3*).
* **Out of Scope**: Silicon decapping, electron microscopy, focused ion beam (FIB) attacks.

### E. Compromised Management Service
* **Capabilities**: Sends remote fleet management commands, updates, and context authorizations.
* **Targets**: Massive fleet-wide reset, unauthorized firmware deployment, tenant context spoofing (Violates *Invariant 5*).
* **Mitigation**: Device-local policy validation, short validity windows, role-separated management scopes.

---

## 2. Threat Actor vs. Target Asset Matrix

| Threat Actor | Granted Access | Target Assets | Core Mitigations |
| :--- | :--- | :--- | :--- |
| **Malicious Tenant** | Software API / SDK | Other Tenants' Models & Inputs | Context-tagged execution, HW queue limits |
| **Compromised Host** | Kernel / PCIe Bus | Onboard DRAM, DMA, Device State | Opaque buffer handles, Device MMU checks |
| **Compromised Signing Key** | Build / Release Pipeline | Entire Firmware Trust Chain | Offline Root Anchors, Scoped Sub-keys, Manifests |
| **Physical Attacker** | Board-level Access | Debug Ports, Flash Storage | Locked JTAG/UART, Encrypted Flash |
| **Management Attacker** | Control Plane Network | Fleet Configuration & Updates | Local Policy Engine, Short-lived Signed Leases |
| **Supply Chain Attacker** | Factory / Transport | Device Identity & Lifecycle Fuses | On-Chip PUF Key Generation, OTP Fuses |
| **DoS Attacker** | Network / Queue Traffic | Card Compute & Power Budget | Per-tenant quotas, Watchdog escalations |
