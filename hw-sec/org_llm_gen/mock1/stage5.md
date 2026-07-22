# Stage 5: High-level security architecture and lifecycle flows

A strong transition is:

> “Now that the trust boundaries are clear, I’ll propose the high-level architecture and walk through the device lifecycle from manufacturing to decommissioning. I’ll focus first on the security-critical flows, then return to tradeoffs.”

The lifecycle view is useful because many hardware vulnerabilities occur during transitions: boot, update, reset, reassignment, and recovery.

---

# 1. High-level architecture

Organize the device into four security layers.

```text
+------------------------------------------------------+
| Untrusted host                                       |
| Tenant workload | Runtime | OS | Driver | Host memory|
+---------------------------+--------------------------+
                            |
                         PCIe / DMA
                            |
+---------------------------v--------------------------+
| Device interface layer                               |
| Command parser | Queue manager | DMA validation      |
+---------------------------+--------------------------+
                            |
+---------------------------v--------------------------+
| Security control layer                               |
| Security monitor | Context manager | Key manager     |
| Policy engine | Attestation | Update manager         |
+---------------------------+--------------------------+
                            |
+---------------------------v--------------------------+
| Tenant execution layer                               |
| Compute engines | Device MMU | Tenant memory         |
| Scheduler | Caches | External device memory          |
+---------------------------+--------------------------+
                            |
+---------------------------v--------------------------+
| Hardware root of trust                               |
| Boot ROM | Root-key hash | Device secret             |
| Lifecycle state | Anti-rollback state                |
+------------------------------------------------------+
```

## The key architectural idea

The security control layer owns all privileged state:

* Tenant-context creation
* Queue ownership
* DMA authorization
* Key derivation
* Firmware update
* Attestation
* Reset and zeroization
* Debug policy

The performance-oriented compute layer should not have unrestricted access to device identity keys, firmware trust anchors, or management authority.

A strong statement is:

> “I would separate high-throughput workload execution from the smaller security-control path, so a bug in model execution does not automatically compromise device identity or fleet-management authority.”

---

# 2. Lifecycle flow 1: Manufacturing and provisioning

This is where the device obtains its identity and enters a trusted lifecycle state.

## Possible flow

1. The device powers on in a manufacturing lifecycle state.
2. The boot ROM authenticates factory provisioning software.
3. A unique device root secret is generated:

   * Preferably on-device using a hardware random-number generator
   * Or securely injected in a trusted facility
4. The secret is stored in protected hardware:

   * One-time programmable memory
   * Secure element
   * Key vault
   * Physically unclonable function-backed derivation
5. The device generates an identity or attestation key pair.
6. The public identity is certified by the manufacturer or operator.
7. Production root keys and lifecycle policy are provisioned.
8. Factory debug privileges are disabled.
9. The lifecycle state is irreversibly changed to production.
10. The device is enrolled into fleet inventory.

## Security properties

* Every device has a unique identity.
* The root secret is non-exportable.
* Development and production trust domains are separate.
* Factory firmware cannot continue running in production.
* Duplicate or cloned identities can be detected.
* Provisioning actions are logged.

## Important design question

Should the private device key be injected or generated on the device?

### On-device generation

Advantages:

* The private key never exists outside the device.
* Reduces exposure during manufacturing.
* Makes cloning more difficult.

Disadvantages:

* Requires trustworthy hardware randomness.
* Certificate enrollment becomes more involved.

### Key injection

Advantages:

* Simpler centralized provisioning.
* Easier to precompute certificates.

Disadvantages:

* Keys may exist in manufacturing systems.
* Larger compromise and cloning risk.

A reasonable answer is:

> “I would prefer on-device key generation and certify only the public key, assuming the device has a reliable hardware random-number generator.”

---

# 3. Lifecycle flow 2: Secure boot

The purpose of secure boot is to prevent unauthorized code from gaining device privilege.

## Example boot chain

```text
Immutable boot ROM
      verifies
First-stage bootloader
      verifies
Security monitor and main firmware
      verifies
Microcode / FPGA image / security configuration
```

## Detailed flow

1. The boot ROM starts from a hardware-defined reset vector.
2. It reads:

   * Hardware lifecycle state
   * Root public-key hash
   * Minimum permitted firmware version
3. It loads the next boot stage.
4. It verifies:

   * Digital signature
   * Image hash
   * Device model compatibility
   * Lifecycle compatibility
   * Version policy
5. If verification succeeds, execution transfers to the next stage.
6. Each stage verifies the next stage.
7. Measurements are extended into protected measurement registers.
8. Security-sensitive hardware is initialized:

   * Debug disabled
   * DMA blocked
   * Host commands not yet accepted
   * Key access restricted
9. Only after the security monitor is ready does the device expose its normal interface.

## Failure behavior

On verification failure, the device should:

* Not enter normal workload mode
* Record a minimal security event
* Enter a restricted recovery mode
* Accept only authenticated recovery images
* Avoid exposing tenant data or keys

A useful line:

> “Secure boot failure should not mean unrestricted recovery mode. Recovery must be a smaller, equally authenticated path.”

---

# 4. Lifecycle flow 3: Measured boot and attestation

Secure boot protects the device locally. Attestation allows the control plane to decide whether it trusts the device.

## Flow

1. The control plane sends a fresh nonce.
2. The device produces an attestation report containing:

   * Device identity
   * Boot measurements
   * Firmware version
   * Security configuration
   * Lifecycle state
   * Debug state
   * Anti-rollback state
   * Device health status
   * Verifier nonce
3. The report is signed by a hardware-protected attestation key.
4. The verifier checks:

   * Certificate chain
   * Device enrollment status
   * Signature
   * Nonce freshness
   * Approved firmware measurements
   * Minimum version
   * Required configuration
   * Revocation status
5. The device is either:

   * Admitted
   * Quarantined
   * Restricted to update/recovery operations

## Important distinction

Attestation should not merely answer:

> “Is the signature valid?”

It should answer:

> “Is this known device running a state currently allowed by policy?”

## Admission output

The verifier may issue a short-lived authorization token stating:

* Device identity
* Approved operating state
* Allowed service role
* Expiration time
* Policy version

The device can require this token before accepting new tenant contexts.

---

# 5. Lifecycle flow 4: Establishing a secure management session

Before privileged commands are accepted, the device and control plane establish an authenticated session.

## Flow

1. The device authenticates the management service.
2. The management service authenticates the device.
3. They establish session keys.
4. Privileged commands include:

   * Sequence number
   * Expiration time
   * Command scope
   * Device identity
   * Authorization context
5. The device verifies:

   * Authentication
   * Authorization
   * Freshness
   * Current lifecycle state
   * Whether the requested action is permitted locally

## Why local policy matters

A compromised orchestration service should not automatically be able to issue every possible command.

For example:

* A workload scheduler may create tenant contexts.
* An update service may install approved firmware.
* Neither should be able to extract device identity keys.
* A general operator should not be able to re-enable production debug mode.

A good statement is:

> “Authentication answers who sent the command; authorization and device-local policy determine whether that sender may perform this action on this device in its current state.”

---

# 6. Lifecycle flow 5: Tenant-context creation

This is how a customer receives an isolated execution environment.

## The central problem

The host cannot simply provide a tenant ID because the host may be compromised.

## Safer flow

1. The control plane authenticates and authorizes the tenant workload.
2. It issues a signed, short-lived context authorization containing:

   * Tenant or security-domain identifier
   * Device identity
   * Permitted operations
   * Resource limits
   * Expiration
   * Unique authorization ID
3. The host forwards the authorization to the device.
4. The device verifies the authorization independently.
5. The device allocates:

   * Internal context ID
   * Command queues
   * Device-memory region
   * DMA mappings
   * Resource quota
   * Optional tenant session keys
6. The device returns an opaque context handle.
7. Future workload commands must reference that handle.

## Why use an opaque handle?

The handle should be:

* Unpredictable
* Bound to the device
* Bound to the context
* Invalid after teardown
* Insufficient by itself to bypass authorization

This reduces context-ID guessing and stale-context reuse.

## Context state

The device should internally bind:

```text
Context handle
    -> tenant security domain
    -> queue ownership
    -> allowed operations
    -> DMA windows
    -> device memory ranges
    -> keys
    -> quotas
    -> expiration
```

The host may carry the handle, but it cannot modify those bindings.

---

# 7. Lifecycle flow 6: Buffer registration and DMA setup

PCIe DMA is one of the most security-sensitive paths.

## Flow

1. The host requests registration of a host-memory range.
2. The request references a valid tenant context.
3. The device verifies:

   * Context ownership
   * Address and length
   * Alignment
   * Integer-overflow safety
   * Maximum permitted size
   * Access direction: read, write, or both
   * Whether the range overlaps prohibited areas
4. The platform IOMMU restricts the device to approved host pages.
5. The device also records a context-scoped DMA mapping.
6. Workload commands reference opaque buffer handles rather than arbitrary physical addresses.

## Why both IOMMU and device-side checks?

The IOMMU protects host memory from the device.

Device-side checks protect tenant separation from:

* Malicious descriptors
* Incorrect context binding
* A compromised host
* Device firmware bugs

A useful line is:

> “The IOMMU is useful defense in depth, but because the host may control it, the device must also enforce context-scoped DMA authorization.”

## TOCTOU concern

If a host can remap memory after validation, the device may access different data than intended.

Possible responses include:

* Pin mappings for the lifetime of an operation
* Bind mappings to immutable translation identifiers
* Use trusted platform support for protected mappings
* Copy highly sensitive input into protected onboard memory before processing

---

# 8. Lifecycle flow 7: Normal workload execution

Now walk through a request from submission to completion.

## Flow

1. The tenant runtime places a command in a host queue.
2. The device fetches the descriptor using DMA.
3. The command parser validates:

   * Opcode
   * Version
   * Length
   * Reserved fields
   * Context handle
   * Buffer handles
   * Numeric ranges
   * Dependency count
4. The security monitor resolves the context.
5. It verifies:

   * Context is active
   * Authorization has not expired
   * Requested operation is permitted
   * Buffers belong to the same context
   * Resource usage is within quota
6. The scheduler dispatches work to a tenant-scoped execution context.
7. Hardware address checks constrain all memory accesses.
8. Results are written only to authorized output buffers.
9. Completion status is returned through the tenant’s queue.

## Critical runtime invariants

For every access:

```text
command context
    == queue owner
    == memory owner
    == DMA mapping owner
    == key domain
```

An identifier should not be trusted merely because it appears in a command. The device should derive ownership from protected internal state.

---

# 9. Tenant isolation inside the device

Isolation must cover more than device DRAM.

## Device memory

Use:

* Per-context address spaces
* Device MMU or memory-protection tables
* Hardware range checks
* Privileged regions inaccessible to tenant engines

## Queues

Use:

* Dedicated or securely multiplexed queues
* Hardware-bound queue ownership
* Per-context completion rings
* Queue-depth limits

## Compute engines

Use:

* Context tags carried through the pipeline
* Context checks before memory access
* Per-context fault attribution
* Per-context reset where possible

## Caches and scratchpads

Options include:

* Partitioning
* Tagging by context
* Flushing on context switch
* Accepting some side-channel risk where performance dominates

## Performance counters

Counters can leak information about neighboring workloads.

Possible policies:

* Tenant-specific counters only
* Coarse-grained reporting
* Disable sensitive global counters
* Restrict operator-only metrics

---

# 10. Lifecycle flow 8: Context teardown and zeroization

This flow is essential for temporal isolation.

## Trigger conditions

A context may end because of:

* Normal workload completion
* Authorization expiration
* Tenant cancellation
* Policy revocation
* Fault
* Host reset
* Device reset
* Device reassignment

## Teardown order

1. Stop accepting new commands.
2. Cancel or drain in-flight operations.
3. Disable DMA mappings.
4. Invalidate queues and handles.
5. Destroy tenant session keys.
6. Clear device memory.
7. Clear scratchpads, caches, and execution registers as required.
8. Clear pending completions and error records containing tenant data.
9. Mark the context identifier unusable.
10. Return resources to the allocator only after sanitization succeeds.

## Important rule

> “Memory should be sanitized before reallocation, not asynchronously afterward.”

## Zeroization methods

* Explicit overwrite
* Cryptographic erasure by discarding a per-context memory-encryption key
* Hardware scrub engine
* Full reset for components that cannot be independently cleared

Cryptographic erasure can be faster, but it only works if all relevant data was encrypted under the destroyed key and no plaintext copies remain elsewhere.

---

# 11. Lifecycle flow 9: Reset and crash recovery

Resets are security-sensitive because they interrupt normal cleanup.

## Threats

* Residual tenant data after reset
* Reusing stale handles
* DMA continuing after firmware crash
* Partial key destruction
* Booting into an insecure fallback
* Attacker-triggered reset loops

## Safer reset sequence

Hardware reset logic should immediately:

1. Block PCIe command processing.
2. Disable DMA.
3. Revoke access to tenant memory.
4. Clear volatile key registers.
5. Reset execution engines.
6. Re-enter verified boot.
7. Scrub or cryptographically invalidate tenant memory.
8. Require re-attestation before new tenant admission.

## Partial resets

For availability, the architecture may support:

* Queue reset
* Tenant-context reset
* Compute-engine reset
* Full device reset

Prefer the smallest safe fault-containment domain.

A good tradeoff statement:

> “Per-context reset improves availability, but only if the hardware can prove that shared state has been sanitized. Otherwise I would escalate to a full-device reset.”

---

# 12. Lifecycle flow 10: Firmware update

This is a privileged state transition.

## Update package

The package may contain:

* Firmware image
* Version
* Device model
* Minimum bootloader version
* Security metadata
* Hashes
* Signing-key identifier
* Rollout policy
* Recovery compatibility information

## Flow

1. The management service sends an update authorization.
2. The device verifies the management command.
3. The device validates the firmware package:

   * Signature
   * Hash
   * Device compatibility
   * Version policy
   * Key authorization
4. The image is written to the inactive slot.
5. The device verifies the written image.
6. The next boot attempts the new slot.
7. The boot ROM verifies the new image again.
8. The new firmware runs self-tests.
9. The device produces a new attestation.
10. The control plane marks the update successful.
11. The previous slot remains available as an approved recovery image.

## Anti-rollback nuance

Do not advance the irreversible minimum version too early.

A safe sequence might be:

* Install version N+1
* Boot and attest successfully
* Confirm fleet health
* Then advance minimum permitted version

Otherwise, a faulty image may leave no recoverable version.

## Recovery limitation

Rollback should only be allowed to:

* A specifically approved recovery image
* A version not below the security floor
* An image signed by an authorized recovery key

---

# 13. Lifecycle flow 11: Quarantine and incident response

A device should enter quarantine when:

* Attestation fails
* Firmware verification fails
* Repeated memory-isolation violations occur
* Device identity is revoked
* Debug state is unexpected
* Hardware integrity errors exceed thresholds
* Update health checks fail

## Quarantine mode

The device may permit only:

* Attestation
* Diagnostics
* Authenticated firmware recovery
* Secure sanitization
* Inventory identification

It should not accept customer workloads.

## Incident evidence

Export:

* Device identity
* Firmware measurements
* Boot failure reason
* Security fault counters
* Reset history
* Update history
* Relevant sequence numbers

Avoid exporting sensitive tenant payloads.

---

# 14. Lifecycle flow 12: Reassignment and decommissioning

## Reassignment to another server

Before reassignment:

1. Destroy all tenant contexts.
2. Disable DMA.
3. Clear volatile and persistent tenant data.
4. Rotate or invalidate server-specific credentials.
5. Reboot into verified state.
6. Re-attest from the new host environment.
7. Update fleet inventory.

## Final decommissioning

1. Remove device from admission allowlists.
2. Revoke device certificates.
3. Destroy or invalidate device identity secrets where supported.
4. Sanitize nonvolatile storage.
5. Record disposal status.
6. Physically destroy sensitive components if sanitization cannot be verified.

---

# 15. The end-to-end lifecycle diagram

A compact interview diagram could be:

```text
Manufacture
    |
Provision identity and production trust
    |
Secure boot
    |
Measured boot
    |
Remote attestation
    |
Fleet admission
    |
Secure management session
    |
Tenant authorization
    |
Context creation
    |
DMA registration
    |
Workload execution
    |
Context teardown and zeroization
    |
Reset / update / reassignment
    |
Re-attestation
    |
Decommission and revoke identity
```

At every transition, ask:

* Who authorizes this transition?
* What state is authenticated?
* What stale state must be destroyed?
* Can it be replayed?
* What happens if power is lost halfway through?
* How is recovery constrained?

That is a systematic way to reason through lifecycle security.

---

# A polished interview answer

> “At a high level, I would divide the card into an untrusted PCIe interface, a small security-control layer, tenant-isolated execution resources, and a hardware root of trust. The security-control layer owns context creation, DMA authorization, key management, updates, attestation, and reset handling.
>
> During manufacturing, each device receives or generates a unique non-exportable identity and transitions irreversibly into production lifecycle state. At boot, immutable ROM verifies each firmware stage, enforces rollback policy, measures the complete executable configuration, and does not enable DMA or workload commands until the trusted security monitor is initialized.
>
> The device then performs remote attestation. The fleet verifier checks device identity, firmware measurements, lifecycle state, debug status, and freshness before admitting the card.
>
> For tenant execution, the control plane provides a signed, short-lived context authorization. The device independently verifies it and creates an opaque internal context bound to queues, DMA mappings, memory, keys, and resource limits. The host can forward commands, but it cannot create or modify tenant ownership.
>
> Every command is parsed and checked on-device. DMA uses context-scoped registered buffers, with device-side validation and IOMMU defense in depth. Tenant memory and execution state are isolated through hardware context tags and device-side address protection.
>
> When a context ends or the card resets, DMA is disabled, handles are invalidated, keys are destroyed, and memory is sanitized before reuse. Firmware updates are signed, anti-rollback protected, installed into an inactive slot, and accepted only after successful boot and attestation. Devices that fail verification enter a restricted quarantine mode rather than serving workloads.”

# The next stage

The next stage should be **attack walkthroughs and mitigation validation**.

Instead of adding more components, we would test the architecture against concrete attacks:

1. A malicious host submits a DMA pointer into another tenant’s buffer.
2. A tenant guesses another context handle.
3. An attacker replays an old tenant authorization.
4. A technician removes the card and reads external memory.
5. An attacker pushes older vulnerable firmware.
6. A signing key is compromised.
7. A workload crashes a shared compute engine.
8. The card loses power halfway through an update.
9. Firmware is validly signed but contains a vulnerability.
10. The attestation service is unavailable.

That stage demonstrates whether the proposed architecture actually satisfies the security invariants.
