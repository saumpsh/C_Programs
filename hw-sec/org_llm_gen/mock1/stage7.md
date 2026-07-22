# Stage 7: Tradeoffs, residual risks, and prioritization

A strong transition is:

> “The design now satisfies the main security invariants, but several controls introduce cost, complexity, or availability tradeoffs. I’ll make those tradeoffs explicit, identify the risks that remain, and prioritize what I would build first.”

This stage shows engineering judgment. The interviewer is not looking for “maximum security everywhere.” They want to see whether you can choose appropriate protections under realistic constraints.

## 1. Hardware enforcement versus firmware flexibility

### Hardware enforcement

Security checks implemented directly in hardware are:

* Harder for compromised firmware to bypass
* Faster for operations performed on every command
* More predictable
* Suitable for memory bounds, context tags, key access, and DMA restrictions

However, hardware is:

* Expensive to change
* Difficult to patch after deployment
* Slower to develop and verify
* Vulnerable to permanent design mistakes

### Firmware enforcement

Firmware is:

* Easier to update
* Better for complex policy
* More flexible across device generations
* Useful for protocol parsing, orchestration, and recovery logic

But compromised firmware may bypass firmware-only controls.

### Design decision

Put small, stable security invariants in hardware:

* Context ownership checks
* Memory isolation
* Privilege separation
* Key non-exportability
* Boot-root enforcement
* DMA enable and disable controls

Keep evolving policy in firmware or the control plane:

* Supported command versions
* Scheduling policy
* Attestation formats
* Update rollout rules
* Operational health policy

A strong interview statement:

> “I would implement mechanisms in hardware and policy in firmware. Hardware enforces that a context cannot access another context’s memory, while firmware decides how much memory each context may receive.”

### Residual risk

A hardware bug in isolation logic may require device replacement or disabling functionality across the fleet.

Mitigations include:

* Simple hardware interfaces
* Formal verification of critical blocks
* Redundant range checks
* Kill switches for optional features
* Conservative initial feature scope

---

## 2. Isolation versus utilization

Sharing the card improves:

* Hardware utilization
* Cost efficiency
* Scheduling flexibility

But shared resources introduce:

* Timing side channels
* Cache and bandwidth contention
* Larger fault domains
* More complicated cleanup
* Denial-of-service risks

### Possible isolation levels

**Logical isolation**

* Shared engines and caches
* Context tagging and address checks
* Highest utilization
* Greater side-channel exposure

**Partitioned isolation**

* Dedicated memory regions
* Partitioned cache or compute resources
* Lower leakage
* Lower scheduling flexibility

**Dedicated-device isolation**

* One tenant per physical card
* Strongest practical isolation
* Highest cost and lowest utilization

### Design decision

Use multiple service tiers:

* Shared mode for standard workloads
* Partitioned mode for higher-sensitivity workloads
* Dedicated cards for customers requiring strong side-channel separation

This avoids imposing the most expensive isolation model on every workload.

### Residual risk

In shared mode, some timing or contention leakage may remain even when direct reads and writes are prevented.

State this explicitly:

> “I would guarantee strong architectural isolation against unauthorized memory access, but not claim complete timing-channel elimination in the shared service tier.”

---

## 3. Memory encryption versus latency and complexity

Memory encryption helps protect against:

* Physical extraction of external memory
* Cold-boot-style recovery
* Board-level probing within the defined threat model

But it introduces:

* Latency
* Area and power overhead
* Key-management complexity
* Integrity-metadata overhead
* Recovery complexity

Encryption without integrity also does not prevent an attacker from modifying ciphertext.

### Design options

* Encrypt only persistent storage
* Encrypt external device DRAM
* Use a per-boot memory key
* Use per-tenant keys
* Add integrity trees or authentication tags

### Tradeoff

A per-boot key is simpler and protects against offline extraction, but does not cryptographically separate tenants.

Per-tenant keys improve erasure and tenant separation but require:

* More key slots
* Key selection on memory access
* More metadata
* More complex context switching

### Design decision

For this system:

* Always encrypt sensitive nonvolatile storage.
* Encrypt external device memory if physical chip extraction is in scope.
* Use per-context or per-allocation keys where cryptographic erasure meaningfully reduces scrub latency.
* Continue to enforce memory isolation independently of encryption.

A good line:

> “Encryption protects data at rest or against physical observation; it does not replace address isolation.”

### Residual risk

Data may also exist in:

* Compute registers
* Caches
* scratchpads
* Queues
* Error buffers

Encrypting external memory alone does not sanitize these locations.

---

## 4. Secure deletion versus performance

Full memory overwrite provides a clear deletion model but can be expensive for large accelerator memories.

### Options

**Synchronous overwrite**

* Strong and easy to reason about
* Delays resource reuse

**Background scrubbing**

* Better performance
* Unsafe if memory is reallocated before scrubbing completes

**Cryptographic erasure**

* Destroy the encryption key
* Very fast
* Requires all copies to be encrypted under that key

### Design decision

Use:

* Cryptographic erasure for encrypted external tenant memory
* Hardware scrubbing for caches, scratchpads, and unencrypted state
* A resource state machine that prevents reuse until sanitization is complete

### Residual risk

Cryptographic erasure fails if:

* Keys are backed up unexpectedly
* Plaintext copies exist elsewhere
* The encryption implementation is incorrect
* Shared buffers use a broader key domain

---

## 5. Fine-grained reset versus implementation complexity

Per-context or per-engine reset improves availability because one tenant does not take down the entire card.

But safe fine-grained reset requires proving that:

* All in-flight operations are stopped
* DMA is disabled
* Shared queues are consistent
* Memory and caches are sanitized
* No stale completion remains
* Shared firmware state is trustworthy

### Design decision

Support a reset hierarchy:

1. Command cancellation
2. Tenant-context reset
3. Compute-engine reset
4. Full-device reset

Escalate when cleanup cannot be proven.

### Residual risk

Fine-grained recovery logic itself becomes security-critical and may contain state-machine bugs.

A mature answer:

> “I would initially prefer a simpler and broader reset boundary if the hardware cannot reliably prove per-context cleanup. Availability optimization should not weaken isolation.”

---

## 6. Fail closed versus service availability

Fail-closed behavior is appropriate when:

* Firmware verification fails
* Device identity is revoked
* Debug state is unexpected
* Tenant ownership cannot be determined
* Memory sanitization fails

But failing closed during every control-plane outage can cause a large service disruption.

### Design decision

Differentiate operations by risk.

**Fail closed**

* New device admission
* Firmware update
* Debug authorization
* New privileged configuration
* New tenant admission after authorization expiry

**Bounded continuity**

* Existing workloads on an already attested device
* Existing management sessions with valid leases
* Read-only telemetry

Use short-lived admission leases and a grace period.

### Tradeoff

Long leases improve availability but delay revocation.

Short leases improve responsiveness to compromise but increase dependency on the control plane.

### Residual risk

A compromised device may continue operating until its lease expires.

---

## 7. Anti-rollback versus recoverability

Strong rollback prevention blocks known-vulnerable firmware, but irreversible counters can make recovery difficult after a faulty release.

### Poor design

Immediately burn a fuse when a new version is installed.

If the new version fails, the previous working version may no longer boot.

### Better design

Maintain two concepts:

* **Preferred version:** operational policy can change quickly
* **Minimum secure version:** irreversible or strongly protected security floor

Advance the security floor only after:

* Successful boot
* Attestation
* Canary validation
* Operational stability
* Confirmation that older versions are no longer needed for recovery

### Residual risk

Keeping an older version temporarily available creates a rollback window.

That window should be:

* Time-bounded
* Policy-controlled
* Audited
* Limited to approved recovery versions

---

## 8. Centralized management versus blast radius

Centralized fleet control simplifies:

* Updates
* Attestation
* Inventory
* Revocation
* Policy consistency

But a central compromise may affect tens of thousands of devices.

### Mitigations

* Separate orchestration from signing
* Use credentials scoped by operation
* Segment the fleet
* Limit update velocity
* Require multi-party approval
* Use device-local safety policy
* Support rapid credential revocation
* Use canary deployments

### Example separation

```text
Scheduler:
  may create tenant contexts

Update orchestrator:
  may request installation of approved releases

Signing service:
  may sign release manifests

Security authority:
  may change minimum-version policy

No single service:
  may perform every operation
```

### Residual risk

Some fleet-wide authorities still exist, particularly:

* Root signing keys
* Root certificate authorities
* Global policy services

These require the strongest operational controls.

---

## 9. Rich telemetry versus customer privacy

Detailed telemetry improves:

* Incident response
* Debugging
* Detection of malformed requests
* Fleet reliability

But it may leak:

* Model characteristics
* Tenant workload volume
* Timing patterns
* Buffer sizes
* Customer identifiers
* Failure details

### Design decision

Use structured security events containing:

* Device ID
* Context pseudonym
* Error category
* Firmware version
* Sequence number
* Resource counters

Avoid:

* Customer payloads
* Model contents
* Raw memory
* Plaintext keys
* Full command bodies

Use access controls and retention limits for tenant-associated metadata.

### Residual risk

Even metadata may reveal workload behavior.

Sensitive customers may require reduced telemetry or stronger pseudonymization.

---

## 10. Strong command authentication versus data-plane performance

Authenticating every command with public-key cryptography would be costly.

But trusting unauthenticated host-provided tenant identity is unsafe.

### Design decision

Use expensive cryptography during:

* Attestation
* Session establishment
* Tenant-context authorization
* Firmware updates

Then use efficient runtime mechanisms:

* Device-issued opaque handles
* Context-bound queues
* Symmetric authentication where needed
* Hardware ownership checks
* Monotonic queue sequence numbers

This amortizes cryptographic cost across many commands.

### Residual risk

A compromised host may still:

* Drop commands
* Delay them
* Reorder permitted operations
* Deny service

It should not be able to cross tenant boundaries or gain privilege.

---

## 11. Debuggability versus production security

Debug interfaces help diagnose hardware failures but can expose:

* Memory
* Registers
* Keys
* Firmware control
* Boot policy

### Design options

* Permanently disable debug in production
* Permit authenticated debug
* Require physical presence
* Use a separate return-to-manufacturer lifecycle state

### Design decision

For high-security production devices:

* Disable unrestricted JTAG and UART
* Permit only limited diagnostic telemetry
* Require an irreversible or tightly controlled lifecycle transition for deep debugging
* Sanitize customer state before service access
* Record every debug authorization

### Residual risk

Permanently disabling debug can make hardware failures difficult to diagnose and increase replacement costs.

---

# Residual risks to state explicitly

A strong candidate does not claim the system is perfectly secure.

For this design, important residual risks include:

## 1. Side channels

Timing, cache contention, memory bandwidth, thermal behavior, and power usage may leak information between tenants.

Complete prevention may require dedicated hardware.

## 2. Unknown hardware flaws

A defect in:

* Device MMU
* Context-tag propagation
* Key vault
* Secure boot ROM
* Memory-encryption engine

could undermine core guarantees and may be difficult to patch.

## 3. Approved but vulnerable firmware

Secure boot allows authorized firmware, including firmware with unknown vulnerabilities.

Mitigation reduces blast radius but cannot eliminate this risk.

## 4. Fleet-root compromise

Compromise of an offline root key, certificate authority, or recovery authority may affect the entire fleet.

## 5. Denial of service by the host

A compromised host may always be able to:

* Withhold work
* Power-cycle the device
* Disconnect PCIe
* Corrupt host-side buffers

The card can contain the impact but cannot guarantee service through a hostile physical host platform.

## 6. Advanced physical attacks

Chip decapping, fault injection, sophisticated probing, and invasive extraction remain outside the threat model.

## 7. Availability during revocation

Short-lived leases create a bounded window during which a device or authorization may remain valid after compromise.

## 8. Operational mistakes

Incorrect policy, key rotation, rollout configuration, or inventory state can defeat otherwise sound technical controls.

---

# Prioritization

The interviewer may ask:

> “You cannot build all of this in version one. What do you prioritize?”

Use risk and dependency rather than choosing convenient features.

## Priority 0: Define the threat model and invariants

Before implementation:

* Confirm whether the host is untrusted
* Define tenant isolation guarantees
* Define physical attack scope
* Define recovery and availability requirements

A mistaken trust assumption can invalidate the architecture.

## Priority 1: Hardware root of trust and secure lifecycle

Build first:

* Immutable boot verification
* Device identity
* Protected key storage
* Production lifecycle state
* Debug restrictions
* Secure recovery foundation

These are difficult to retrofit after silicon ships.

## Priority 2: Runtime tenant isolation

Build:

* Device-side command validation
* Context-bound queues
* Device MMU
* DMA ownership checks
* Privileged memory separation
* Context tagging through the execution pipeline

This protects the most important customer assets.

## Priority 3: Safe teardown and reset

Build:

* DMA shutdown
* Handle invalidation
* Key destruction
* Memory sanitization
* Reset hierarchy
* Power-loss-safe state transitions

These controls are necessary before supporting multi-tenancy safely.

## Priority 4: Secure firmware update and rollback control

Build:

* Signed updates
* A/B slots
* Version enforcement
* Atomic metadata
* Recovery image
* Signing-key separation

A secure boot design without safe updates will become insecure when vulnerabilities are discovered.

## Priority 5: Attestation and fleet admission

Build:

* Measured boot
* Device certificates
* Verification policy
* Admission leases
* Revocation
* Quarantine mode

This lets the operator enforce fleet-wide security posture.

## Priority 6: Operational hardening

Add:

* Fine-grained RBAC
* Multi-party approvals
* Fleet segmentation
* Canary rollout
* Incident-response tooling
* Tamper-evident logs

## Priority 7: Advanced protections

Depending on risk and customer demand:

* External memory encryption
* Cache partitioning
* Strong timing isolation
* Per-tenant cryptographic memory domains
* Formal verification of more components
* Advanced tamper resistance

---

# A useful prioritization framework

You can evaluate each control across four dimensions:

| Dimension           | Question                                              |
| ------------------- | ----------------------------------------------------- |
| Security impact     | What attack does this prevent?                        |
| Blast radius        | One tenant, one host, one card, or the entire fleet?  |
| Retrofit difficulty | Can this be added after silicon deployment?           |
| Operational cost    | What latency, utilization, or complexity does it add? |

Controls that are high-impact, fleet-critical, and hard to retrofit should come first.

For example:

| Control                   |    Impact | Retrofit difficulty |   Priority |
| ------------------------- | --------: | ------------------: | ---------: |
| Hardware memory isolation | Very high |           Very high |    Highest |
| Secure boot root          | Very high |           Very high |    Highest |
| Context zeroization       | Very high |                High |    Highest |
| A/B firmware update       |      High |                High |       High |
| Attestation               |      High |              Medium |       High |
| Detailed telemetry        |    Medium |                 Low |      Later |
| Timing-channel mitigation |    Varies |                High | Risk-based |

---

# How to answer “What would you cut?”

A good answer is:

> “I would not cut secure boot, device-side tenant isolation, protected keys, reset-time sanitization, or authenticated firmware updates because they support the primary security invariants and are difficult to retrofit. For an initial version, I might defer strong timing-side-channel isolation, per-tenant external-memory encryption, and very fine-grained engine resets. I would clearly document those residual risks and offer dedicated-device deployment for highly sensitive workloads.”

This shows prioritization without compromising the foundation.

---

# Polished interview response

> “The main architectural tradeoff is deciding which controls belong in fixed hardware versus updateable firmware. I would put stable invariants—memory ownership, context tagging, DMA limits, key non-exportability, and boot-root enforcement—in hardware. More changeable policy remains in firmware and the control plane.
>
> Multi-tenancy also trades utilization against isolation. Shared execution gives better efficiency, but timing and contention leakage may remain. I would strongly guarantee direct memory and command isolation, while using partitioned or dedicated devices for customers requiring stronger side-channel protection.
>
> Memory encryption protects against board-level physical attacks but adds latency and key-management complexity. I would prioritize encryption for nonvolatile and externally accessible memory while keeping address isolation as the primary runtime control.
>
> For availability, I would support a reset hierarchy, but only use fine-grained recovery when cleanup can be proven complete. Otherwise, a full reset is safer. Similarly, already admitted devices may continue under bounded leases during a control-plane outage, but newly booted or updated devices fail closed.
>
> The largest residual risks are shared-resource side channels, unknown hardware defects, validly signed but vulnerable firmware, fleet-root compromise, and denial of service by a hostile host. Advanced invasive physical attacks remain out of scope.
>
> In prioritization, I would first build the hardware root of trust, device-side tenant isolation, key protection, secure reset and sanitization, and safe signed updates. Attestation and fleet operations come next. Advanced timing isolation and sophisticated physical protections would be driven by customer requirements and cost.”

# Final interview stage

The final stage is **closing synthesis and interviewer follow-ups**.

You should be ready to:

* Summarize the architecture in two minutes
* Name the top three risks
* State the most important assumptions
* Explain what you would build first
* Identify what remains unsolved
* Respond to changes such as:

  * “Now assume the host is trusted”
  * “Now remove the central control plane”
  * “Now support offline operation”
  * “Now reduce cost by 30%”
  * “Now protect against malicious firmware”
  * “Now make the card single-tenant”
