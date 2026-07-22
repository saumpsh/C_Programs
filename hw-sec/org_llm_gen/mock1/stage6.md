# Stage 6: Attack walkthroughs and mitigation validation

A strong transition is:

> “Now I’ll validate the architecture against concrete attacks. For each attack, I’ll explain the attacker’s capability, the expected control, the failure behavior, and any residual risk.”

This stage is where you prove the design is not just a collection of security features.

A useful structure for every attack is:

```text
Attack
→ Security invariant at risk
→ Prevention
→ Detection
→ Recovery
→ Residual risk or tradeoff
```

---

## 1. Malicious host submits a DMA pointer into another tenant’s memory

### Attack

The compromised host constructs a command for tenant A but supplies a DMA address belonging to tenant B.

It may also try:

* Integer overflow in address-plus-length
* Overlapping buffers
* Out-of-range offsets
* Remapping memory after validation
* Reusing a stale buffer descriptor

### Invariant at risk

> A command may only access buffers belonging to its own tenant context.

### Prevention

The device should not trust raw addresses from commands.

Instead:

1. Host memory is registered to a specific context.
2. The device creates an internal buffer handle.
3. Commands reference the handle, not an arbitrary address.
4. The device resolves the handle through protected context state.
5. Hardware checks ensure:

   * buffer context matches command context
   * offset and length are within bounds
   * requested direction is allowed
   * arithmetic does not overflow
6. The IOMMU limits device access to registered host pages as defense in depth.

### Detection

Log:

* Invalid buffer handle
* Context mismatch
* Out-of-bounds access
* Repeated malformed descriptors

Repeated violations may indicate a compromised host.

### Recovery

* Reject the command.
* Fault only the offending context where possible.
* Rate-limit repeated violations.
* Quarantine the host or card if violations continue.

### Residual risk

If the compromised host controls the IOMMU, host-side isolation cannot be fully trusted. Device-side ownership checks remain essential.

A strong conclusion:

> “The IOMMU protects host memory from accidental or malicious device access, but device-side context checks protect tenant isolation from a malicious host.”

---

## 2. Tenant guesses or reuses another tenant’s context handle

### Attack

Tenant A observes or guesses tenant B’s context identifier and submits commands using it.

It may also replay a handle from a previously destroyed context.

### Invariant at risk

> Only authorized commands may operate within a tenant context, and stale contexts must remain invalid.

### Prevention

Use opaque context handles that are:

* High entropy
* Device-generated
* Bound to internal protected state
* Bound to a queue or authenticated session
* Invalidated on teardown
* Associated with a generation number

The device should verify more than possession of the handle:

```text
queue owner
= context owner
= authorization domain
= command context
```

A random handle alone should not act as a bearer credential.

### Detection

Log:

* Unknown handles
* Stale generation numbers
* Queue-context mismatches
* Excessive context-probing attempts

### Recovery

Reject the request without revealing whether the target context currently exists.

This avoids creating a context-enumeration oracle.

### Residual risk

Opaque handles reduce guessing, but isolation should rely primarily on protected ownership metadata, not secrecy of identifiers.

---

## 3. Attacker replays an old tenant authorization

### Attack

The host captures a previously valid authorization token and reuses it after:

* The tenant was revoked
* The authorization expired
* The context was destroyed
* The card was reassigned
* The permissions were reduced

### Invariant at risk

> Privileged and tenant-authorizing commands must be fresh and current.

### Prevention

The authorization should include:

* Device identity
* Tenant security domain
* Unique token ID
* Issue time
* Expiration time
* Allowed operations
* Resource limits
* Policy version

The device should also maintain:

* Monotonic sequence or replay state
* Revocation epoch
* Context generation
* Recently consumed one-time authorization IDs, where appropriate

Binding the token to a specific device prevents use on another card.

### Detection

Log:

* Expired token
* Duplicate authorization ID
* Stale policy version
* Device-binding mismatch
* Revoked authorization epoch

### Recovery

* Reject the token.
* Destroy associated stale context state if it still exists.
* Require fresh authorization from the control plane.

### Tradeoff

Maintaining exact replay state for every token may be expensive at fleet scale.

Possible compromise:

* Short token lifetimes
* Per-device or per-tenant revocation epochs
* Sequence windows rather than storing every token forever

---

## 4. Technician removes the card and reads external memory

### Attack

A technician powers down the server, removes the accelerator, and attempts to read:

* External DRAM
* Flash storage
* Debug output
* Configuration storage

### Invariant at risk

> Customer data and device secrets must not be recoverable through ordinary physical access.

### Prevention

For nonvolatile storage:

* Encrypt all sensitive contents.
* Store encryption keys in protected on-chip storage.
* Do not store device root secrets externally.

For external DRAM:

* Use memory encryption if cold-boot or chip removal is in scope.
* Prefer per-context or per-boot keys.
* Destroy keys on reset or context teardown.

For debug paths:

* Disable JTAG and UART in production.
* Require cryptographic authorization for approved service mode.
* Ensure service mode does not expose raw tenant memory or keys.

### Detection

Possible controls include:

* Inventory mismatch
* Device disappearance
* Unexpected re-enrollment
* Lifecycle-state changes
* Physical tamper evidence

### Recovery

* Revoke or quarantine the device identity.
* Require full re-attestation after reinstall.
* Sanitize before returning to service.

### Residual risk

Memory encryption protects against board-level extraction but may not protect against advanced invasive chip attacks, which are outside scope.

---

## 5. Attacker attempts firmware rollback

### Attack

The attacker installs an older, correctly signed firmware version containing a known vulnerability.

### Invariant at risk

> Only currently approved firmware may execute.

### Prevention

The boot ROM verifies both:

* Signature validity
* Version policy

Use protected rollback state such as:

* One-time programmable version fuses
* Secure monotonic counter
* Signed minimum-version policy protected by hardware

The update package must also match:

* Device family
* Hardware revision
* Lifecycle state
* Required bootloader version

### Detection

Log:

* Attempted downgrade
* Version-policy mismatch
* Repeated recovery-slot selection
* Unexpected firmware measurement

Attestation should expose the running version and security floor.

### Recovery

Boot only:

* A current approved image
* Or a specifically authorized recovery image above the minimum security floor

### Tradeoff

Irreversible rollback counters can make recovery harder.

A good answer:

> “I would separate the currently preferred version from the irreversible minimum secure version. That allows controlled rollback during a faulty deployment without permitting rollback to known-vulnerable releases.”

---

## 6. Firmware-signing key is compromised

### Attack

The attacker signs malicious firmware with a valid production key.

### Invariant at risk

> Secure boot must admit only software authorized under current security policy.

### Important observation

Basic secure boot does not stop this attack.

A valid signature proves that a trusted key signed the image, not that the image is safe.

### Mitigation

Reduce signing-key blast radius through:

* Offline root keys
* Shorter-lived intermediate signing keys
* Separate keys by:

  * environment
  * device family
  * firmware component
  * release channel
* Multi-party approval
* Independent artifact review
* Signed release manifests
* Key revocation
* Emergency key rotation
* Staged rollout

The device or verifier may also require:

* Approved image hash
* Approved release ID
* Valid signing key
* Minimum policy epoch

This allows policy to reject a maliciously signed image after compromise is discovered.

### Detection

* Compare attestation measurements against an allowlist.
* Detect unexpected release hashes.
* Monitor canary health.
* Alert on unusual signing activity.
* Audit all release approvals.

### Recovery

1. Revoke the compromised signing key.
2. Publish a new trust-policy epoch.
3. Prevent affected images from being admitted.
4. Update devices using a separate recovery trust path.
5. Re-attest the fleet.

### Residual risk

If the compromised key can sign both normal and recovery firmware, recovery may be difficult.

Therefore:

> “The recovery trust path should not depend entirely on the same online key used for routine releases.”

---

## 7. Malicious workload crashes a shared compute engine

### Attack

A tenant submits a legal or malformed workload that causes:

* Compute-engine hang
* Firmware exception
* Infinite execution
* Shared scheduler corruption
* Watchdog reset

### Invariant at risk

> One tenant should not unnecessarily affect other tenants, and failures must not expose stale state.

### Prevention

Use:

* Command validation
* Maximum execution-time limits
* Resource quotas
* Per-context scheduling
* Hardware watchdogs
* Restricted tenant-programmable functionality
* Fault containment between execution engines

### Detection

Attribute faults to:

* Tenant context
* Queue
* Command
* Compute engine
* Firmware component

Avoid logging raw customer data.

### Recovery

Escalate gradually:

1. Cancel the offending command.
2. Reset the tenant context.
3. Reset the affected compute engine.
4. Reset the entire card only if shared state cannot be trusted.

Before reuse:

* Disable DMA
* Destroy keys
* Invalidate handles
* Sanitize affected memory and shared state

### Tradeoff

Fine-grained resets improve availability but require more complex hardware isolation and state cleanup.

A strong statement:

> “I would support the smallest reset domain whose cleanup can be proven complete. Otherwise, I would prefer a broader reset over risking cross-tenant leakage.”

---

## 8. Power is lost halfway through a firmware update

### Attack or failure

Power fails after part of the new image or metadata is written.

### Invariant at risk

> The device must never boot partially written or unauthenticated firmware.

### Prevention

Use A/B slots:

* Active slot remains unchanged.
* Update is written only to inactive slot.
* Metadata is committed atomically.
* Each image is fully verified after writing.
* Boot ROM independently verifies the selected image.

Store slot metadata with:

* Integrity protection
* Version
* Boot-attempt count
* Commit state
* Image hash

### Detection

At boot, detect:

* Incomplete update
* Invalid metadata
* Failed signature
* Repeated boot failures
* Self-test failure

### Recovery

* Fall back to the last approved slot.
* Enter restricted recovery if both slots fail.
* Do not lower the anti-rollback floor prematurely.

### Residual risk

If both slots share a buggy bootloader or corrupted shared metadata, recovery may still fail.

A minimal ROM-level recovery path may be necessary.

---

## 9. Firmware is validly signed but contains a vulnerability

### Attack

An attacker exploits a parser or memory-safety flaw in approved firmware.

### Invariant at risk

Potentially all runtime invariants, depending on firmware privilege.

### Prevention

Secure boot alone is insufficient.

Architectural mitigations include:

* Small privileged firmware
* Hardware-enforced memory isolation
* Separate firmware privilege levels
* Memory-safe implementation where practical
* Minimal command formats
* Fuzzing and negative testing
* Formal verification for small critical modules
* No direct firmware access to raw root keys
* Hardware-enforced DMA and context checks

### Detection

* Runtime fault monitors
* Isolation-violation counters
* Unexpected resets
* Attestation plus firmware version inventory
* Behavioral anomaly detection
* Crash telemetry

### Recovery

* Quarantine affected versions
* Revoke or block their measurements
* Deploy patched firmware
* Re-attest before readmission
* Rotate tenant session keys if compromise is suspected

### Residual risk

Approved firmware remains part of the trusted computing base.

The design should limit what one firmware bug can compromise.

---

## 10. Attestation service is unavailable

### Failure

The control plane cannot verify device state.

### Invariant at risk

There is tension between:

* Availability
* Preventing unverified devices from serving workloads

### Possible policy

For already admitted devices:

* Allow existing workloads to continue for a limited grace period.
* Do not permit firmware updates or security-policy changes.
* Possibly allow existing contexts but block new tenant admission after token expiry.

For newly booted or changed devices:

* Fail closed.
* Keep them in quarantine until attestation succeeds.

### Prevention and resilience

* Redundant verifier instances
* Cached policy
* Short-lived admission leases
* Multiple regional verification endpoints
* Grace periods based on risk class

### Detection

Log:

* Lease expiration
* Verification-service failures
* Extended offline operation
* Policy cache age

### Tradeoff

Long grace periods improve availability but increase the window in which revocation cannot take effect.

A good line:

> “I would distinguish continuity of already approved operation from admission of new or changed state. Existing devices may receive a bounded grace period, while newly booted or updated devices fail closed.”

---

## 11. Compromised management service sends destructive commands

### Attack

An attacker controls one orchestration service and issues commands to:

* Reset the fleet
* Install firmware
* Disable logging
* Enable debug
* Reassign tenants
* Sanitize devices

### Invariant at risk

> Privileged operations must be authorized according to role, scope, and device lifecycle state.

### Prevention

Do not give one service universal authority.

Use:

* Separate credentials by function
* Signed command scopes
* Multi-party approval for high-risk actions
* Device-local policy
* Rate limits
* Fleet segmentation
* Short command validity
* Per-device targeting
* Emergency circuit breakers

Examples:

* Scheduler can create tenant contexts but not update firmware.
* Update service can stage an approved release but not enable debug.
* Debug enablement may be impossible in production or require physical lifecycle transition.

### Detection

* Alert on unusually broad commands
* Monitor update velocity
* Require audit trails
* Compare command intent against change-management records

### Recovery

* Revoke the compromised service credential
* Freeze privileged operations
* Quarantine affected devices
* Re-attest and inventory the fleet

---

## 12. Tenant tries to infer another tenant through timing

### Attack

Tenant A observes:

* Execution latency
* Cache contention
* Memory bandwidth
* Scheduler delay
* Power or thermal throttling
* Performance counters

It tries to infer whether tenant B is active or learn characteristics of B’s model.

### Invariant at risk

> Cross-tenant confidentiality includes more than direct memory reads.

### Mitigation options

Depending on requirements:

* Partition shared resources
* Use per-tenant scheduling slices
* Flush selected state
* Restrict global counters
* Add timing noise
* Dedicate devices for highly sensitive tenants
* Accept bounded leakage for performance reasons

### Tradeoff

Strong side-channel isolation may significantly reduce utilization and throughput.

A mature answer is:

> “I would explicitly separate direct isolation guarantees from side-channel guarantees. I can strongly prevent unauthorized reads and writes, but timing isolation may require partitioning or dedicated hardware depending on the customer’s threat model.”

Do not claim complete side-channel elimination unless the design supports it.

---

## 13. Device returns an output to the wrong tenant

### Attack or bug

A completion entry or output buffer is associated with the wrong tenant because of:

* Queue confusion
* Reused context ID
* Stale completion
* Scheduler bug
* Incorrect buffer binding

### Invariant at risk

> Results must be delivered only to the context that initiated the work.

### Prevention

Carry a protected context tag through the entire execution pipeline.

Bind together:

* Input command
* Execution context
* Output buffer
* Completion queue
* Encryption or integrity domain

Use generation numbers so stale completions cannot match newly allocated contexts.

### Detection

* Completion-context mismatch
* Invalid generation
* Queue-owner mismatch
* Output after context teardown

### Recovery

* Drop the completion.
* Fault the affected engine or context.
* Sanitize potentially exposed buffers.
* Escalate if shared state integrity is uncertain.

---

# Validate the original invariants

After the walkthroughs, explicitly return to the invariants.

## Invariant 1

**A command accesses only its tenant’s resources.**

Validated by:

* Context-scoped queues
* Opaque buffer handles
* Device MMU
* DMA ownership checks
* Context tags through execution

## Invariant 2

**Only approved code and configuration execute.**

Validated by:

* Secure boot
* Anti-rollback
* Approved measurement policy
* Key revocation
* Restricted recovery

## Invariant 3

**Root secrets and tenant keys are not exposed.**

Validated by:

* Non-exportable key storage
* Hardware key operations
* Privilege separation
* Debug restrictions
* Key destruction on teardown

## Invariant 4

**No tenant state survives reuse.**

Validated by:

* Ordered teardown
* DMA disablement
* Handle invalidation
* Memory scrub or cryptographic erasure
* Reset-time cleanup

## Invariant 5

**Privileged commands are authentic, authorized, and fresh.**

Validated by:

* Mutual authentication
* Scoped roles
* Signed commands
* Sequence numbers
* Expiry
* Device-local policy

## Invariant 6

**Only compliant devices join the fleet.**

Validated by:

* Hardware identity
* Measured boot
* Attestation
* Policy allowlists
* Revocation
* Admission leases

---

# How to present this efficiently in an interview

You probably will not have time for twelve attacks.

Choose the highest-value ones:

1. Cross-tenant DMA attack
2. Firmware rollback
3. Signing-key compromise
4. Reset or crash leaving residual data
5. Management-plane compromise
6. Attestation outage

A concise interview transition could be:

> “I’ll validate the design against the highest-risk attacks: a malicious host attempting cross-tenant DMA, replay or guessing of tenant contexts, firmware downgrade, compromise of the signing path, reset-time residual data, and management-plane abuse.”

For each one, spend roughly:

* 20 seconds describing the attack
* 30–40 seconds on prevention
* 10–20 seconds on recovery and tradeoff

---

# Polished mock-interview response

> “I’ll validate the architecture against several representative attacks.
>
> First, suppose a compromised host supplies a DMA pointer into another tenant’s memory. Commands do not carry trusted raw addresses; they refer to device-issued, context-bound buffer handles. The device validates ownership, direction, bounds, and generation, while the IOMMU provides defense in depth. A mismatch faults the requesting context rather than the whole card.
>
> Second, suppose an attacker replays an old tenant authorization. Tokens are short-lived, bound to a specific device and policy epoch, and include a unique identifier or sequence. The device rejects expired, duplicate, or revoked authorizations.
>
> Third, suppose an attacker installs old but correctly signed firmware. The ROM verifies not only the signature but also a protected minimum-version policy. Recovery may use only explicitly approved images above the security floor.
>
> Fourth, if a firmware-signing key is compromised, secure boot alone is insufficient. I would use scoped intermediate keys, multi-party approval, measurement allowlists, revocation, staged rollout, and a separate recovery trust path.
>
> Fifth, if a tenant crashes an execution engine, I would first reset and sanitize the tenant context, then the individual engine, and only reset the full card if shared state cannot be trusted. DMA is disabled and keys and memory are cleared before reuse.
>
> Finally, if attestation is unavailable, newly booted or updated devices fail closed. Already admitted devices may continue under short-lived leases for a bounded grace period, balancing availability against revocation latency.
>
> These walkthroughs validate the core invariants: tenant ownership is enforced on-device, only approved firmware executes, privileged commands are fresh and scoped, and no sensitive state survives context reuse or reset.”

# Next stage

The next stage is **tradeoffs, residual risks, and prioritization**.

That is where you explain:

* Hardware enforcement versus firmware flexibility
* Isolation versus utilization
* Encryption versus latency
* Fine-grained reset versus complexity
* Fail-closed behavior versus availability
* Strong anti-rollback versus recoverability
* Rich telemetry versus customer privacy
* Centralized control versus blast radius
