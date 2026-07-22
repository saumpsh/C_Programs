# Stage 2: Assets & Security Objectives

Based on the clarified scope in [Stage 1](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage1.md), this stage maps system assets to required security properties and establishes the core security invariants.

---

## 1. Asset & Security Property Mapping

| Asset Category | Specific Asset | Required Security Properties |
| :--- | :--- | :--- |
| **Workload Assets** | Proprietary Model Weights | Confidentiality, Integrity, Tenant Binding, Secure Deletion |
| | Input & Output Payloads | Confidentiality, Integrity, Correct Output Routing |
| | Intermediate State (Caches/DRAM) | Cross-tenant Isolation, Hardware Zeroization on Teardown |
| **Authorization State** | Tenant Contexts & Queue Maps | Authenticity, Freshness, Integrity, Non-replayability |
| **Device Software** | Main Firmware & Microcode | Authenticity, Integrity, Anti-rollback, Auditability |
| | Security Configuration / Fuses | Modification Protection, Persistence, Debug Locking |
| **Cryptographic Assets** | Device Root Secret (DUS) | Non-exportability, On-chip Generation, Uniqueness |
| | Attestation Private Key | Non-exportability, Hardware Isolation |
| | Firmware Signing Root Keys | Fleet-wide Isolation, Key Revocation, Offline Storage |
| **Management Plane** | Fleet Commands & Policy | Authenticity, Authorization, Short Validity Windows |
| **Hardware Capacity** | Compute Engines & DRAM | Quotas, DoS Resistance, Fault Containment |
| **Observability** | Security Telemetry & Audit Logs | Integrity, Sequence Ordering, Payload Sanitization |

---

## 2. The 6 Core Security Invariants

These six invariants form the non-negotiable benchmark used to validate every architecture decision:

* **Invariant 1 (Command Isolation)**: A command can only access memory, queues, and compute resources belonging to its authenticated tenant context.
* **Invariant 2 (Code Authenticity)**: No executable code, microcode, or security configuration runs unless cryptographically authorized by secure boot or update policy.
* **Invariant 3 (Secret Non-Exportability)**: Device-root secrets and tenant encryption keys are never exposed to host-visible memory or software interfaces.
* **Invariant 4 (Zero-Leakage State Transition)**: A reset, context teardown, or device reassignment sanitizes all prior tenant state before resources are reallocated.
* **Invariant 5 (Management Freshness)**: Privileged management commands require authentic, authorized, and fresh cryptographically signed tokens.
* **Invariant 6 (Authentic Fleet Admission)**: The control plane only admits devices whose hardware identity, measurement registers, and lifecycle state satisfy current policy.
