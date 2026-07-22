Good. In an interview, this stage should be brief but explicit. Its purpose is to answer:

> “What exactly are we protecting, from whom, and what does protection mean?”

Do not list every possible asset randomly. Group them and connect each asset to a concrete security objective.

# Stage 2: Assets and security objectives

A strong transition from the clarification phase would be:

> “Based on those assumptions, I’ll identify the critical assets and define the security properties each one requires. That will help us prioritize the threat model and architecture.”

## 1. Customer workload assets

These are the primary assets the system exists to protect.

### Model weights

For an ML accelerator, model weights may represent valuable customer intellectual property.

Required properties:

* **Confidentiality:** Other tenants, the host, and unauthorized operators must not read them.
* **Integrity:** An attacker must not silently modify them.
* **Tenant binding:** A model must only be usable by the tenant that owns it.
* **Secure deletion:** The model must not remain recoverable after the workload ends or the device is reassigned.

### Input data

Inputs may contain sensitive customer or user data.

Required properties:

* Confidentiality
* Integrity
* Correct association with the intended workload
* Protection from replay where repeated processing would be harmful
* Secure deletion after use

### Output data

Outputs may be sensitive and must be returned to the correct tenant.

Required properties:

* Confidentiality
* Integrity
* Correct destination
* Prevention of cross-tenant mix-ups

### Intermediate computation state

This includes activations, temporary buffers, cache contents, and scratch memory.

This asset is easy to forget in an interview.

Required properties:

* Confidentiality between tenants
* Memory bounds enforcement
* Zeroization before reuse
* Protection across reset and context switching

A good sentence is:

> “I would treat intermediate state as sensitive even though it is temporary, because residual data in memory, caches, or queues can create cross-tenant leakage.”

---

## 2. Tenant identity and authorization state

The card needs to know which workload owns which resources.

Examples include:

* Tenant identifiers
* Execution-context identifiers
* Queue ownership
* Memory mappings
* Access permissions
* Resource quotas
* Session keys

Required properties:

* **Integrity:** An attacker must not change one tenant identifier into another.
* **Authenticity:** The device must know that authorization came from a trusted authority.
* **Freshness:** Old authorization tokens must not be replayed after revocation.
* **Isolation:** One tenant must not reference another tenant’s context.
* **Availability:** Corrupt state belonging to one tenant should not unnecessarily affect others.

This is central because isolation depends on more than memory separation. It also depends on correctly associating commands, queues, and resources with an authenticated tenant context.

---

## 3. Device software and configuration

### Firmware

Firmware controls the card and may have access to all tenant data.

Required properties:

* Authenticity
* Integrity
* Version enforcement
* Rollback resistance
* Controlled recovery
* Auditability

Confidentiality of firmware may be useful for intellectual property protection, but it is usually less important than authenticity and integrity.

A useful distinction:

> “For firmware, my primary security goals are authenticity and integrity. Encryption alone would not stop an attacker from installing malicious firmware.”

### Programmable hardware configuration

If the card includes an FPGA, microcode, programmable engines, or loadable kernels, those configurations are also executable security-sensitive content.

Required properties:

* Signature verification
* Compatibility checks
* Authorization
* Version control
* Isolation from tenant-supplied code

Do not only say “secure boot verifies firmware.” The verification chain should include all executable and security-relevant configuration.

### Security configuration

Examples include:

* Debug state
* Boot policy
* Allowed signing keys
* Anti-rollback version
* Memory-isolation settings
* Management permissions
* Lifecycle mode

Required properties:

* Integrity
* Authorized modification only
* Persistence across reset where appropriate
* Protection from rollback
* Auditability

---

## 4. Cryptographic assets

These are usually among the highest-value assets.

### Device root secret

This establishes the device’s identity.

Required properties:

* Non-exportability
* Confidentiality
* Integrity
* Uniqueness
* Protection across the device lifecycle

Ideally, firmware does not directly read this secret. Hardware uses it to derive or operate with subordinate keys.

### Attestation key

Used to prove device identity and boot state.

Required properties:

* Non-exportability
* Authenticity
* Controlled use
* Rotatability or revocability
* Resistance to cloning

### Session and data-encryption keys

These may protect management communication, customer data, or external device memory.

Required properties:

* Confidentiality
* Tenant separation
* Limited lifetime
* Secure derivation
* Secure deletion on reset or workload completion

### Firmware-signing trust anchors

The card may only store a root public key or hash, while the private signing key remains outside the device.

Required properties:

* Integrity of the on-device trust anchor
* Protection of the external private signing key
* Key rotation
* Revocation
* Separation of production and development keys

A strong observation is:

> “Compromise of the firmware-signing authority can affect the entire fleet, so this is a higher-blast-radius asset than an individual device key.”

---

## 5. Management-plane assets

These include:

* Device inventory
* Firmware approval policy
* Attestation allowlists
* Device certificates
* Administrative credentials
* Deployment commands
* Revocation state
* Workload-admission decisions

Required properties:

* Authentication
* Authorization
* Integrity
* Freshness
* Auditability
* Limited blast radius

The management plane should not be treated as automatically safe just because it belongs to the operator.

You could say:

> “I will treat the management plane as highly trusted but still potentially compromisable, so devices should validate signed commands and enforce local policy rather than blindly executing arbitrary control-plane instructions.”

---

## 6. Hardware resources and availability

The card itself is a shared resource.

Assets include:

* Compute engines
* Onboard memory
* Command queues
* PCIe bandwidth
* Thermal and power budgets
* Reset controls
* Firmware-update capacity

Required properties:

* Fair allocation
* Resource quotas
* Denial-of-service resistance
* Fault containment
* Safe recovery
* Protection from destructive commands

Availability does not mean every request must succeed. It means one tenant should not be able to monopolize or repeatedly reset the card.

---

## 7. Logs and security evidence

Relevant records include:

* Boot measurements
* Firmware-update events
* Administrative actions
* Authentication failures
* Security-policy violations
* Device resets
* Tenant-context creation and destruction
* Attestation results

Required properties:

* Integrity
* Authenticity
* Ordering
* Replay resistance
* Availability to incident responders
* Confidentiality where logs contain sensitive metadata

Logs should not contain customer inputs, models, plaintext keys, or raw sensitive memory.

---

# A compact interview table

You do not need to draw a large table, but mentally structure the answer this way:

| Asset                      | Main objectives                                        |
| -------------------------- | ------------------------------------------------------ |
| Model weights              | Confidentiality, integrity, tenant isolation, deletion |
| Inputs and outputs         | Confidentiality, integrity, correct routing            |
| Intermediate memory        | Isolation, bounds checking, zeroization                |
| Tenant authorization state | Authenticity, integrity, freshness                     |
| Firmware                   | Authenticity, integrity, anti-rollback                 |
| Security configuration     | Integrity, authorized modification                     |
| Device identity key        | Non-exportability, uniqueness, revocation              |
| Session keys               | Confidentiality, separation, short lifetime            |
| Management commands        | Authentication, authorization, freshness               |
| Shared compute resources   | Availability, quotas, fault containment                |
| Audit logs                 | Integrity, ordering, privacy                           |

# Prioritize the security objectives

After listing the assets, rank the most important guarantees.

For this system, I would prioritize:

## 1. Cross-tenant isolation

A tenant must not read, modify, infer, or receive another tenant’s data or execution state.

This includes:

* Memory
* Queues
* DMA buffers
* Cache state
* Outputs
* Errors
* Performance counters
* Residual state after reset

## 2. Trusted device execution

Only approved firmware and security configuration may execute.

This requires:

* Hardware-rooted secure boot
* Complete verification chain
* Rollback protection
* Controlled recovery

## 3. Host compromise containment

A compromised host must not:

* Read protected device memory
* Change another tenant’s context
* Install firmware
* Forge management commands
* Use unrestricted DMA
* Extract device keys

## 4. Authentic device admission

The control plane must distinguish:

* Genuine enrolled devices
* Counterfeit or substituted devices
* Approved firmware
* Unapproved or outdated firmware
* Production devices versus development devices

## 5. Secure lifecycle transitions

Security must hold during:

* Boot
* Workload creation
* Tenant switching
* Reset
* Firmware update
* Server reassignment
* Repair
* Decommissioning

This is important because many leaks occur during transitions rather than normal steady-state operation.

## 6. Controlled availability

A tenant should not be able to deny service to other tenants, but confidentiality and integrity take priority when the device cannot safely continue.

# Security invariants

A very strong interview technique is to convert the objectives into a few invariants.

You might state:

> “I want the architecture to enforce several invariants.”

### Invariant 1

A command can only access memory and resources belonging to its authenticated tenant context.

### Invariant 2

No executable code or security-sensitive configuration runs unless it is authorized by the secure boot or update policy.

### Invariant 3

Device-root secrets and tenant keys are never exposed through the host-visible interface.

### Invariant 4

A reset, context destruction, or reassignment removes all prior tenant-sensitive state before resources are reused.

### Invariant 5

Privileged management operations require authenticated, authorized, and fresh commands.

### Invariant 6

The control plane only admits devices whose identity and measured state satisfy current policy.

These invariants will later map directly to architectural controls.

# What to say in the mock interview

A polished two-minute version would be:

> “The primary assets are customer model weights, inputs, outputs, and intermediate computation state. They require confidentiality, integrity, tenant isolation, and secure deletion. I would also protect tenant authorization state, including queue ownership, memory mappings, and execution contexts, because corruption there could break isolation.
>
> The next category is trusted device state: firmware, programmable configuration, boot policy, and debug configuration. These primarily require authenticity, integrity, and rollback protection.
>
> Cryptographic assets include the device identity key, attestation key, session keys, and firmware-signing trust anchors. Root secrets should be non-exportable, tenant keys should be isolated and short-lived, and fleet-level signing keys require strong separation and revocation because their compromise has a large blast radius.
>
> Finally, I would protect management commands, audit records, and shared hardware capacity. Management operations need authentication, authorization, and freshness, while shared compute and memory need quotas and fault containment.
>
> My highest-priority invariants are that one tenant can never access another tenant’s resources, only approved code can execute, a compromised host cannot take over the device, privileged commands cannot be replayed or forged, and all tenant state is cleared before resources are reused.”

# What comes next

The next stage is **threat actors and attacker capabilities**.

There, we identify distinct attackers such as:

* Malicious tenant workload
* Compromised host OS or driver
* Malicious or compromised operator
* External management-plane attacker
* Data-center technician with physical access
* Supply-chain attacker
* Compromised firmware-signing infrastructure

For each attacker, we will define what access they have and what we explicitly assume they cannot do.
