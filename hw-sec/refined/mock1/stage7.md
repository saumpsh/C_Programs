# Stage 7: Architectural Tradeoffs & Prioritization

This stage evaluates core engineering tradeoffs, identifies residual risks, and outlines the P0–P7 implementation roadmap.

---

## 1. Core Architectural Tradeoffs

### Tradeoff 1: Hardware Enforcement vs. Firmware Flexibility
* **Hardware Mechanisms**: Context tags, Device MMU, key non-exportability, Boot ROM. (Immutable, fast, tamper-proof, but impossible to update post-silicon).
* **Firmware Mechanisms**: Command parsing, queue scheduling, attestation formats, update policies. (Flexible, patchable, but larger attack surface).
* **Decision**: Implement core invariant enforcement in hardware; keep policy logic in updateable firmware.

### Tradeoff 2: Isolation Depth vs. Hardware Utilization
* **Shared Multi-Tenancy**: Concurrent context execution maximizes compute utilization and reduces cost, but introduces timing side-channel risks.
* **Dedicated Execution**: Single tenant per physical card eliminates side channels, but increases hardware costs.
* **Decision**: Offer tiered service levels—standard shared tier for typical workloads; dedicated/partitioned cards for high-security tenants.

### Tradeoff 3: Zeroization Latency vs. Performance
* **Full DRAM Overwrite**: Thorough, but introduces significant delay during tenant context switching.
* **Cryptographic Erasure**: Instant key destruction renders ciphertext unrecoverable; requires per-context memory encryption overhead.
* **Decision**: Use cryptographic erasure for external DRAM partitions; use hardware scrub engines for internal registers/caches.

### Tradeoff 4: Reset Domain Granularity
* **Per-Context Pipeline Reset**: Preserves card availability for other tenants, but complex state-machine cleanup required.
* **Full Device Reset**: Simple and guaranteed zero-leakage, but impacts all active workloads.
* **Decision**: Attempt per-context reset first; escalate to full device reset if state cleanup verification fails.

---

## 2. P0–P7 Implementation Prioritization Roadmap

| Priority Level | Architectural Component | Rationale | Retrofit Difficulty |
| :--- | :--- | :--- | :--- |
| **P0 (Foundation)** | Threat Model & Invariant Specs | Define trust boundaries before writing code/HDL | N/A |
| **P1 (Silicon RoT)** | Boot ROM, PUF Key Vault, OTP Fuses | Fundamental hardware trust anchors | Impossible post-silicon |
| **P2 (HW Isolation)** | Device MMU, Context Tagging, DMA Bounds | Core cross-tenant protection (*Invariant 1*) | High (requires HW) |
| **P3 (Sanitization)** | Hardware Scrub Engine & Key Destruction | Prevents temporal data leakage (*Invariant 4*) | High |
| **P4 (Firmware Update)**| Signed A/B Slots & Version Fuses | Essential for patching security flaws (*Invariant 2*) | High |
| **P5 (Attestation)** | Measured Boot & Control Leases | Fleet admission and health monitoring | Medium |
| **P6 (Observability)** | Telemetry & Audit Event Logging | Incident response and anomaly detection | Low (Software) |
| **P7 (Advanced)** | Cache Partitioning & PQC Signatures | Niche side-channel & post-quantum defense | High |

---

## 3. Explicit Residual Risks

1. **Shared Resource Side Channels**: Timing, cache contention, and power consumption may leak model characteristics in shared multi-tenant mode.
2. **Unseen Hardware MMU Defects**: Logic flaws in hardware address translation cannot be patched over-the-air.
3. **Vulnerable Authorized Firmware**: Secure Boot guarantees binary authorization, not vulnerability-free execution.
4. **Signing Root Key Compromise**: Staged rollouts mitigate, but fleet-wide authority key loss is catastrophic.
