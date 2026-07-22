# Stage 1: Clarifying Questions & Assumptions

> **Opening Strategy**: Begin by categorizing clarifying questions into system function, multi-tenancy, trust model, threat boundaries, and operational constraints.

---

## 1. System Function & Workloads
* **Candidate Question**: What workload does the accelerator run, and does it persist data across operations?
* **Interviewer Response**: Performs machine-learning inference. Customers submit proprietary model weights and input payloads. Data is temporarily processed in onboard DRAM; no customer data is intended to be persisted permanently.

## 2. Multi-Tenancy Execution Model
* **Candidate Question**: Are workloads from different customers executed concurrently or strictly time-sliced?
* **Interviewer Response**: Both. The card supports concurrent execution contexts across multiple tenants and can be reassigned to different host servers over its operational lifecycle.

## 3. Host Trust Boundary
* **Candidate Question**: Is the host operating system, hypervisor, and device driver considered trusted?
* **Interviewer Response**: **No.** Assume tenant workloads may compromise the host OS. A compromised host must not be able to extract another tenant's data or alter device firmware.

## 4. Physical Threat Scope
* **Candidate Question**: What level of physical attack should we defend against in the data center?
* **Interviewer Response**: Protect against physical card swapping, tapping exposed debug ports (JTAG/UART), and flash chip extraction. Invasive silicon decapping or focused ion beam (FIB) attacks are out of scope.

## 5. Security Objectives & Fail Posture
* **Candidate Question**: How do we prioritize Confidentiality, Integrity, and Availability?
* **Interviewer Response**: **Confidentiality & Integrity > Availability.** Cross-tenant data exposure is unacceptable. Taking a suspicious card out of service (quarantine) is preferable to risking data leakage.

## 6. Firmware & Control Plane Management
* **Candidate Question**: Who updates device firmware and how is the fleet managed?
* **Interviewer Response**: Central data-center management services perform remote updates over tens of thousands of cards. Customers cannot update firmware. Staged rollouts and automatic recovery are required.

## 7. Remote Attestation
* **Candidate Question**: Must the management service cryptographically verify device state before workload assignment?
* **Interviewer Response**: **Yes.** The control plane requires cryptographic proof of genuine hardware identity and authentic firmware measurements before assigning customer workloads.

## 8. Communication Interface & DMA
* **Candidate Question**: How do host workloads communicate with the card, and is DMA used?
* **Interviewer Response**: Host drivers create PCIe command queues in host DRAM. The card reads/writes commands and workload buffers via DMA. Management traffic shares the physical PCIe bus.

## 9. Performance & Cost Limits
* **Candidate Question**: What latency or cost trade-offs apply to security controls?
* **Interviewer Response**: Asymmetric public-key operations on every individual execution command are too slow. Symmetric mechanisms or hardware address checks must be used for per-command data paths.

## 10. Reset & Recovery Expectations
* **Candidate Question**: What happens after a reset, crash, or update failure?
* **Interviewer Response**: Failed updates must revert via A/B slots without bricking. Resets must sanitize memory. Operational cards may run on bounded offline leases if the control plane drops temporarily.

## 11. Supply Chain Scope
* **Candidate Question**: Do we defend against counterfeit hardware or supply-chain substitution?
* **Interviewer Response**: Yes. Ensure only provisioned cards join the fleet. Assume trusted silicon design and primary manufacturing facilities are non-malicious.

## 12. Logging & Audit Requirements
* **Candidate Question**: What telemetry is required, and what privacy restrictions apply?
* **Interviewer Response**: Audit trails required for resets, updates, attestation, and errors. Telemetry **must not** contain customer payloads, model weights, or keys.

---

## Consolidated Assumption Summary

> *"We are designing a PCIe machine-learning inference accelerator serving multi-tenant workloads both concurrently and temporally. Customer models, inputs, and intermediate states are confidential. The host OS and drivers are untrusted, requiring the card to validate all commands and DMA ranges internally. Central control services manage attestation and firmware updates. Physical board-level attacks and device swapping are in scope, while invasive chip attacks are out of scope. Confidentiality and integrity take absolute priority over availability."*
