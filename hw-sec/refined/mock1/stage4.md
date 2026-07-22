# Stage 4: Trust Boundaries & System Architecture

This stage defines the system decomposition, primary architecture layers, and trust boundary enforcement points.

---

## 1. Master System Architecture Diagram

```text
                   CENTRAL FLEET CONTROL PLANE
        +-------------------------------------------------+
        | Fleet Manager | Attestation Verifier | FW Signer|
        +------------------------+------------------------+
                                 | mTLS / Signed Leases
======================== MANAGEMENT BOUNDARY ========================

 UNTRUSTED HOST ZONE                     TRUSTED ACCELERATOR CARD
+--------------------+          +-----------------------------------+
| Customer Workload  |          | DEVICE COMMAND INTERFACE          |
| Application / SDK  | PCIe/DMA | Security Monitor | Queue Manager  |
| Host Kernel / OS   |--------->| DMA Validator    | Policy Engine  |
| Host PCIe Driver   |          +-----------------+-----------------+
| Host DRAM Queues   |                            |
+--------------------+          +-----------------v-----------------+
                                | TENANT EXECUTION LAYER            |
                                | Compute Engines  | Device MMU     |
                                | Tenant Memory    | Scheduler      |
                                +-----------------+-----------------+
                                                  |
                                +-----------------v-----------------+
                                | HARDWARE ROOT OF TRUST            |
                                | Boot ROM        | Root Key Hash   |
                                | DUS Vault (PUF) | OTP Version Fuse|
                                +-----------------------------------+
```

---

## 2. Layer & Trust Level Definitions

* **Untrusted Layer (Host)**: Customer application, host OS, hypervisor, device driver, host DRAM queues. (Even though host drivers are operator-provided, they are treated as untrusted).
* **Trusted Security Control Layer (Device)**: Security Monitor, Command Validator, Context Manager, Policy Engine. Owns all context handles, DMA permissions, keys, and reset logic.
* **Tenant Execution Layer (Device)**: Compute engines, Device MMU, tenant DRAM partitions. Constrained by context tags enforced in hardware.
* **Hardware Root of Trust (Device)**: Immutable Boot ROM, OTP version fuses, hardware key vault (PUF/DUS). Immutable and minimal.

---

## 3. Trust Boundary Enforcement Matrix

| Trust Boundary | Intersecting Components | Primary Threat | Enforcement Point |
| :--- | :--- | :--- | :--- |
| **Boundary 1: Host to Card** | PCIe Bus / DMA | Malformed descriptors, arbitrary DMA injection | Device Security Monitor & DMA Validator |
| **Boundary 2: Tenant to Tenant** | Onboard DRAM / Compute | Cross-tenant memory access, queue spoofing | Device MMU & HW Context Tags |
| **Boundary 3: Control to Card** | Management Network | Forged management commands, replay | mTLS, Signed Tokens, Local Policy Engine |
| **Boundary 4: Firmware to RoT** | Boot ROM → Firmware | Firmware tampering, version rollback | Boot ROM Signature Check & Monotonic Fuses |
| **Boundary 5: Factory to Production** | Manufacturing → Fleet | Identity cloning, debug leakage | OTP Lifecycle Fuses, Locked JTAG/UART |

---

## 4. Control Plane vs. Data Plane Separation

* **Data Plane**: Handles workload commands, model weights, and inference execution over PCIe queues. Highly optimized for low-latency throughput; authenticated via device-issued **opaque handles**.
* **Control Plane**: Handles device admission, attestation, signed firmware updates, and context leases. Authenticated via asymmetric signatures and short validity windows.
