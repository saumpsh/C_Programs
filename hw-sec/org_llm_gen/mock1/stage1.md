Below is a realistic first phase of the mock interview. The goal is not to ask every possible question, but to clarify the assumptions that materially change the security architecture.

## Mock question

> Design a secure hardware accelerator card for a multi-tenant data center. The card connects to a server over PCIe, has onboard memory and firmware, and may be shared by workloads belonging to different customers.

You could begin with:

> “Before proposing an architecture, I’d like to clarify the functionality, deployment model, security objectives, and trust assumptions. I’ll prioritize questions whose answers would materially change the design.”

---

# Stage 1: Clarifying questions

## 1. What operations does the accelerator perform?

**Candidate question**

> “What kind of computation does the card perform, and does it process or retain sensitive customer data?”

**Interviewer answer**

The card performs machine-learning inference. Customers submit models and input data. The card temporarily stores model weights and intermediate results in onboard memory, but it is not intended to persist customer data after a workload finishes.

**What this tells us**

The important assets include:

* Customer input data
* Model weights, which may be proprietary
* Intermediate computation results
* Device firmware and configuration
* Credentials or cryptographic keys

Temporary storage still creates confidentiality risks. We will need memory isolation and reliable zeroization between customers.

---

## 2. How is the card shared between customers?

**Candidate question**

> “Can workloads from multiple customers run on the card concurrently, or is the card reassigned between customers over time?”

**Interviewer answer**

Both are possible. The accelerator supports multiple execution contexts, and workloads from several customers may run concurrently. It may also be reassigned to different servers during its lifetime.

**What this tells us**

We need two forms of isolation:

* **Concurrent isolation:** preventing one active tenant from accessing another tenant’s resources.
* **Temporal isolation:** ensuring that a new tenant cannot recover data belonging to a previous tenant.

The design must cover memory, execution contexts, queues, caches, performance counters, and reset behavior.

---

## 3. Should we trust the host server?

**Candidate question**

> “Should the host operating system, hypervisor, and device driver be considered trusted, or should the accelerator remain secure if the host is compromised?”

**Interviewer answer**

Assume customer workloads may compromise the host operating system. The data-center control plane is more trusted, but we do not want a compromised host to extract another customer’s data or alter device firmware.

**What this tells us**

This is one of the most important answers.

The PCIe boundary becomes a major trust boundary. The card must not depend entirely on the host driver for security.

We should assume a compromised host can:

* Send malformed commands
* Submit arbitrary DMA addresses
* Repeatedly reset the device
* Attempt unauthorized firmware updates
* Replay requests
* Inspect or modify host memory
* Impersonate workloads running on that host

The device must validate requests and enforce isolation itself.

---

## 4. What level of physical attacker is in scope?

**Candidate question**

> “Should we protect against ordinary data-center physical access, or also against sophisticated invasive attacks against the chip or board?”

**Interviewer answer**

Protect against technicians removing or replacing cards, connecting to exposed debug interfaces, or accessing storage chips. Advanced laboratory attacks such as chip decapping and electron microscopy are out of scope.

**What this tells us**

We need reasonable physical protections:

* Disable or authenticate JTAG, UART, and test interfaces
* Encrypt sensitive data in nonvolatile storage
* Detect device substitution through device identity
* Securely erase devices before repair or disposal
* Protect manufacturing and provisioning interfaces

We do not need to claim resistance to advanced invasive attacks. That would substantially increase cost and complexity.

---

## 5. What are the main security priorities?

**Candidate question**

> “How should we prioritize confidentiality, integrity, and availability? Are there any especially important security properties?”

**Interviewer answer**

Customer data and models must remain confidential and unmodified. Cross-tenant access is unacceptable. Availability is important because the cards are expensive shared resources, but taking a malfunctioning card out of service is preferable to risking data exposure.

**What this tells us**

Our default failure posture should prioritize confidentiality and integrity over availability.

For example:

* Failed verification should prevent the device from entering normal service.
* A tenant context with corrupted state should be terminated.
* A device with an untrusted firmware measurement should be quarantined.
* We should still avoid allowing one tenant to unnecessarily take down the entire card.

---

## 6. Who controls firmware and device configuration?

**Candidate question**

> “Who is authorized to update firmware or change security-sensitive configuration, and must updates occur remotely?”

**Interviewer answer**

A central data-center management service performs remote firmware updates. Customers must never update firmware. The fleet may contain tens of thousands of cards, so updates must support staged rollout and recovery.

**What this tells us**

Firmware update security is a core design requirement.

We will need:

* Cryptographically signed firmware
* Device-side verification
* Anti-rollback protection
* Separation between update authorization and ordinary host commands
* Staged deployment and canarying
* A/B firmware slots or equivalent recovery
* Audit logs
* Key rotation and revocation
* Protection against a compromised management service

Scale also matters: manual provisioning or recovery is not acceptable.

---

## 7. Is remote attestation required?

**Candidate question**

> “Does the control plane need cryptographic evidence of the device identity and the firmware currently running before assigning customer workloads?”

**Interviewer answer**

Yes. Before putting a card into service, the control plane must verify that it is a genuine enrolled device running an approved firmware version and configuration.

**What this tells us**

The card needs:

* A hardware-rooted device identity
* A non-exportable identity or attestation key
* Measured boot
* Freshness protection, such as verifier-provided nonces
* An enrollment and certificate lifecycle
* A way to report firmware and security configuration measurements

Attestation will become part of workload admission.

---

## 8. How does communication with the card work?

**Candidate question**

> “How do workloads submit commands and data to the accelerator? Does the card use DMA into host memory, and are there separate management and workload interfaces?”

**Interviewer answer**

The host driver creates command queues in host memory. The card reads commands and workload buffers using DMA. There is also a logically separate management interface, but both ultimately travel over PCIe.

**What this tells us**

DMA is a major attack surface.

We must consider:

* IOMMU configuration
* Device-side validation of DMA addresses
* Registered or pinned memory ranges
* Per-tenant DMA mappings
* Descriptor bounds, lengths, alignment, and ownership
* Time-of-check/time-of-use issues
* Isolation between management commands and workload commands

A logically separate interface is helpful, but sharing the same physical PCIe transport means separation must also be cryptographically and architecturally enforced.

---

## 9. What are the performance and cost constraints?

**Candidate question**

> “What latency, throughput, power, and cost constraints should the security design respect?”

**Interviewer answer**

Inference latency and throughput are important. Per-command public-key operations or encrypting all internal device memory would be too expensive. Moderate additional hardware for security is acceptable, but the card must remain commercially practical.

**What this tells us**

We cannot simply apply the strongest control everywhere.

Likely tradeoffs include:

* Use public-key cryptography during boot, attestation, session establishment, and firmware update—not on every command.
* Use symmetric message authentication where request authentication is needed.
* Enforce memory isolation through hardware address checks rather than encrypting every small internal buffer.
* Encrypt external memory if physical extraction is in scope.
* Keep the trusted computing base small enough to audit.

Security mechanisms must fit the performance model.

---

## 10. What should happen after failures or resets?

**Candidate question**

> “What recovery behavior is expected after a crash, reset, failed firmware update, or loss of connectivity to the management service?”

**Interviewer answer**

A device should recover automatically when it can do so safely. A failed update must not permanently brick the card. Existing workloads may be interrupted after a reset, but their data must not become accessible to later workloads. If the control plane is temporarily unavailable, already approved devices may continue operating for a limited period.

**What this tells us**

The architecture needs explicit recovery behavior:

* A/B firmware slots
* A minimal trusted recovery image
* Watchdogs
* Memory and key zeroization on reset
* Tenant-context invalidation
* Limited-duration cached authorization
* Quarantine after repeated verification failures
* Protection against attacker-induced reboot loops

The answer also reveals a deliberate fail-open/fail-closed distinction:

* Existing approved operation may temporarily continue.
* New privileged changes or admissions may fail closed.

---

## 11. Are supply-chain attacks in scope?

**Candidate question**

> “Should the design account for malicious or counterfeit components, compromised manufacturing, or device substitution?”

**Interviewer answer**

Yes, within reasonable limits. The operator wants to detect counterfeit or substituted cards and ensure that only properly provisioned production devices join the fleet. Assume the chip design itself and the trusted manufacturing facility are not malicious.

**What this tells us**

We should cover:

* Unique device identity
* Secure provisioning
* Production versus development lifecycle states
* Signed manufacturing firmware
* Certificate enrollment
* Inventory verification
* Disabling factory test capabilities
* Prevention of cloned device credentials
* Chain-of-custody and device replacement procedures

The trusted manufacturing facility is an explicit trust assumption. We should state it rather than pretending supply-chain risk is fully eliminated.

---

## 12. What compliance and observability are required?

**Candidate question**

> “What security events must be logged or audited, and are there restrictions on what telemetry may contain?”

**Interviewer answer**

Operators need an audit trail for firmware changes, device admission, administrative commands, resets, and security violations. Logs must not contain customer models, input data, keys, or sensitive intermediate values.

**What this tells us**

We need security telemetry that is:

* Authenticated
* Ordered or sequence-numbered
* Exported off-device
* Associated with the device identity
* Protected against tampering and replay
* Carefully scrubbed of customer data

Observability is part of the security architecture, not an afterthought.

---

# Consolidate the assumptions

Before proceeding, summarize the interviewer’s answers. This is important because it confirms that you and the interviewer are solving the same problem.

A strong candidate summary would sound like this:

> “Let me summarize the assumptions before I design the system. We are building a PCIe machine-learning inference accelerator that may concurrently serve multiple tenants. Customer inputs, model weights, and intermediate results are sensitive. The host OS and driver may be compromised, so the card must enforce its own request validation and tenant isolation. The central control plane manages firmware and workload admission, and it requires device identity and remote attestation. Ordinary physical access, device substitution, debug access, and storage extraction are in scope, but sophisticated invasive chip attacks are not. We prioritize confidentiality and integrity over availability, while still supporting fleet-scale updates, safe recovery, and high inference performance.”

That summary is often more valuable than asking additional low-priority questions.

# The next stage: assets and security objectives

Do **not** jump directly to the architecture yet.

The next systematic step is:

## Stage 2A: Identify assets

We will list exactly what must be protected, such as:

* Customer model weights
* Customer input and output data
* Intermediate computations
* Tenant identities and authorization state
* Device firmware
* Security configuration
* Device identity keys
* Firmware-signing trust anchors
* Attestation keys
* Management credentials
* Audit records
* Accelerator availability and capacity

## Stage 2B: Define security objectives

For each asset, determine whether we require:

* Confidentiality
* Integrity
* Authenticity
* Freshness
* Isolation
* Availability
* Secure deletion
* Auditability

## Stage 2C: Rank the objectives

We will identify the top non-negotiable properties, likely:

1. No cross-tenant data access
2. Only authenticated firmware may execute
3. A compromised host cannot take control of the card
4. Only authorized devices may join the fleet
5. Sensitive state must not survive tenant reassignment or reset

This gives us a concrete basis for identifying threat actors and drawing trust boundaries.
