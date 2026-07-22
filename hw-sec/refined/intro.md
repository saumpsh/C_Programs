# Hardware Security System Design: Master Framework & Core Primitives

This document serves as the **Single Source of Truth (SSOT)** for general hardware security system design interview principles, trust modeling, and technical primitives. Individual mock interview scenarios reference this document to avoid repeating foundational theory.

---

## 1. The 7-Step Interview Delivery Framework

When faced with an open-ended hardware security design problem (e.g., cloud accelerators, SmartNICs, HSMs, secure enclaves), organize the response into seven structured phases:

1. **Clarify Scope & Goals**: Functional scope, multi-tenancy model, performance constraints, host trust assumptions.
2. **Establish Security Invariants & Assets**: Map critical assets (keys, customer data, firmware) to core properties (Confidentiality, Integrity, Authenticity, Deletion).
3. **Define Threat Actors & Capabilities**: Model attackers from tenant workloads up to compromised host OS, physical technicians, and supply chain.
4. **Decompose Trust Boundaries**: Draw system layers, mark trusted vs. untrusted components, and establish enforcement points.
5. **Architect End-to-End Lifecycle Flows**: Design boot, attestation, session setup, tenant context creation, DMA transfers, zeroization, and updates.
6. **Walkthrough Concrete Attacks**: Validate the architecture against host DMA manipulation, replay attacks, key compromise, and side channels.
7. **Formulate Tradeoffs & Prioritize**: Distinguish hardware vs. firmware mechanisms, weigh isolation vs. utilization, and establish P0–P7 rollout stages.

---

## 2. Core Hardware Security Primitives

### A. Hardware Root of Trust (RoT) & Secure Boot
* **Immutable Boot ROM**: Small, unmodifiable ROM chip execution vector containing the Root Public Key Hash and basic verification logic.
* **Measured Boot Chain**: Each boot stage (ROM → 1st Bootloader → Security Monitor → Main Firmware) calculates a cryptographic hash (digest) of the next stage before execution and extends it into protected measurement registers (e.g., TPM PCRs or internal HW registers).
* **Anti-Rollback Protection**: Hardware-backed monotonic counters or One-Time Programmable (OTP) fuses prevent flashing older, vulnerability-ridden (yet validly signed) firmware binaries.

### B. Device Identity & Remote Attestation
* **Device Unique Secret (DUS)**: Non-exportable hardware secret generated on-chip via Physically Unclonable Functions (PUF) or injected during trusted manufacturing into OTP fuses.
* **Attestation Statement**: Cryptographically signed proof issued by the device containing:
  - Device Unique Public Key / Certificate Chain
  - Measured Boot digest registers
  - Security configuration & lifecycle state (`PRODUCTION` vs `DEBUG`)
  - Verifier-provided Nonce (prevents replay of prior attestation statements)

### C. Zero-Trust Host & PCIe / DMA Protection
* **Untrusted Host Model**: Assume the host OS, hypervisor, and PCIe drivers are compromised by malicious tenants or external attackers.
* **Context-Scoped DMA Handles**: Host applications never pass raw physical memory addresses to the device. Instead, memory is pre-registered to an authenticated context, issuing opaque buffer handles.
* **Dual-Layer Memory Enforcement**:
  - *Device MMU / HW Checks*: Internal hardware context tags enforce that tenant context $A$ cannot execute DMA into tenant context $B$'s onboard memory.
  - *Platform IOMMU*: Restricts PCIe device DMA access to explicit host memory pages as defense-in-depth.

### D. Multi-Tenant Isolation & Zeroization
* **Concurrent Isolation**: Hardware context tagging carried through compute pipelines, queues, and caches to prevent active tenant cross-talk.
* **Temporal Isolation**: Guaranteeing that data from tenant $A$ cannot be recovered by tenant $B$ when hardware resources are reassigned.
* **Zeroization**:
  - *Hardware Scrub Engine*: Overwriting memory/registers before re-allocating.
  - *Cryptographic Erasure*: Destroying ephemeral per-context memory encryption keys, instantly rendering ciphertext unrecoverable.

### E. Firmware Lifecycle & Update Security
* **A/B Firmware Slots**: Update packages written to an inactive slot ($B$) while running on active slot ($A$). Metadata committed atomically after verification.
* **Key Hierarchy Separation**: Offline Root Keys sign intermediate Release Keys. Production keys are strictly separated from Development/Debug keys.
* **Quarantine Mode**: Devices failing boot signature checks or attestation validation enter a restricted state allowing only diagnostic telemetry and signed recovery updates—never tenant workloads.

---

## 3. Recommended Whiteboard Layout & Communication Flow

```text
               +----------------------------------+
               |      FLEET MANAGEMENT PLANE      |
               | Attestation Verifier | Firmware  |
               +----------------+-----------------+
                                | mTLS / Signed Commands
======================= MANAGEMENT BOUNDARY =======================
UNTRUSTED HOST ZONE             |         TRUSTED DEVICE ZONE
+--------------------+          |      +--------------------------+
| Tenant Workloads   |          |      | Security Control Layer   |
| Host OS / Driver   | PCIe/DMA |      | (Command Parser / MMU)   |
| Host Shared DRAM   |----------+----->| Tenant Execution Layer   |
+--------------------+                 | (Compute & Device DRAM)  |
                                       +------------+-------------+
                                                    |
                                       +------------+-------------+
                                       | Hardware Root of Trust   |
                                       | Boot ROM / OTP Fuses     |
                                       +--------------------------+
```
