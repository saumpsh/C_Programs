# Stage 8: Closing Synthesis & Follow-Up Pivots

This stage provides concise summary scripts and structured responses for common interviewer follow-up pivots.

---

## 1. Executive Summaries

### A. 2-Minute Master Synthesis
> *"We designed a secure multi-tenant PCIe accelerator for cloud ML inference under an untrusted host threat model. We separated the architecture into an untrusted host layer, a trusted device control layer, a context-isolated execution engine, and a hardware Root of Trust.
> 
> The Boot ROM authenticates firmware, enforces monotonic version fuses, and records boot digests. The control plane attests device state using nonces and hardware certificates before admitting cards to the fleet.
> 
> Workloads execute inside context-isolated domains created via signed control-plane leases. The card issues opaque context handles, eliminating raw host-provided address trust. Device MMUs and hardware context tags enforce memory isolation, with platform IOMMUs providing defense-in-depth.
> 
> On teardown or reset, DMA is disabled, handles are revoked, and tenant memory is sanitized via cryptographic erasure or hardware scrub engines. Signed A/B firmware updates provide safe recovery. The principal residual risks are timing side channels, unpatchable hardware defects, and compromised release infrastructure."*

### B. 30-Second Executive Summary
> *"The accelerator treats the host OS and drivers as untrusted. A hardware Root of Trust verifies firmware and protects device keys. The control plane admits cards via remote attestation and issues short-lived context leases. Internal hardware context tags and Device MMUs isolate tenant memory. Resets zeroize keys and scrub state before re-allocation, while signed A/B slots ensure safe firmware updates."*

---

## 2. Top 3 System Risks

1. **Cross-Tenant Compromise over PCIe/DMA**: Untrusted host drivers manipulating memory offsets. *Mitigated by opaque handles, context-scoped DMA limits, and Device MMUs.*
2. **Firmware Release Infrastructure Compromise**: Malicious software signed by valid production keys. *Mitigated by offline root keys, intermediate scoped credentials, measurement allowlists, and staged rollouts.*
3. **Data Leakage During State Transitions**: Residual state surviving resets or context teardowns. *Mitigated by hardware-enforced DMA block, key destruction, and scrub-before-reuse state machines.*

---

## 3. Interviewer Follow-Up Pivot Scenarios

| Scenario Pivot | Required Architectural Shift | What Remains Unchanged |
| :--- | :--- | :--- |
| **1. "Host is now trusted"** | Host can pass raw DMA pointers; simpler context handles; reduced control-plane lease checks. | Secure Boot, firmware updates, hardware key vault, bounds checking. |
| **2. "Single-tenant card"** | Remove concurrent context scheduling, cache tags, and multi-queue maps. | Temporal isolation, zeroization on reassignment, Boot ROM, attestation. |
| **3. "No central control plane"** | Accept offline-signed capability tokens; maintain local replay/revocation state on-card. | Hardware RoT, Device MMU isolation, key vault, zeroization. |
| **4. "Operate offline for 30 days"** | Issue 30-day context leases; maintain local revocation epoch counters; accept delayed revocation. | Hardware isolation, secure boot, DMA limits, sanitization. |
| **5. "Reduce HW cost by 30%"** | Remove external DRAM encryption, cache partitioning, and fine-grained resets. Preserve core MMU. | Boot ROM, PUF keys, context tags, zeroization, A/B updates. |
| **6. "Firmware is malicious"** | Push ALL isolation checks below firmware into fixed hardware logic; deny firmware access to DUS key. | Boot ROM, OTP fuses, hardware key vault. |
| **7. "Control plane compromised"** | Scope management credentials by function; require multi-party signed manifests for updates. | Device-local policy engine, Boot ROM anti-rollback. |
| **8. "Runtime compromise after attestation"** | Issue short-lived admission leases; require periodic health telemetry; re-attest on reset. | Measured boot, attestation signing keys. |
| **9. "How do you test this?"** | Formal verification of HW MMU tags; parser fuzzing; power-fault injection during updates. | Complete security specification. |
| **10. "How do you measure security?"** | Track isolation fault metrics, fleet attestation compliance %, time-to-revoke keys, and patch rollout speed. | Underlying security architecture. |
| **11. "Weakest link in design?"** | Shared-resource timing side channels and complex reset-recovery state machines. | Direct DRAM address isolation. |
| **12. "What to investigate next?"** | Benchmarking external memory encryption latency, DMA interface fuzzing, and formal MMU verification. | Core design invariants. |
