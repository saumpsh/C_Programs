# Stage 3: Threat actors and attacker capabilities

A strong transition is:

> “Now that I’ve identified the assets and security invariants, I’ll define the threat actors and the capabilities I’m granting each one. This keeps the threat model concrete and prevents me from either under-designing or defending against unrealistic attacks.”

The key is not merely naming attackers. For each attacker, state:

1. What access they have
2. What they are trying to achieve
3. What they cannot do under our assumptions
4. Which trust boundary they attack

---

## 1. Malicious tenant workload

This is the most basic attacker in a multi-tenant system.

### Capabilities

Assume the tenant can:

* Submit arbitrary valid and malformed workload commands
* Choose command parameters, buffer sizes, offsets, and timing
* Run workloads repeatedly
* Attempt to consume excessive compute, memory, or queue capacity
* Observe its own outputs, errors, latency, and performance
* Coordinate requests across multiple tenant contexts it controls

Depending on the deployment, the tenant may also control application code running on the host.

### Goals

The tenant may try to:

* Read another tenant’s model or data
* Modify another tenant’s computation
* Escape its assigned execution context
* Infer secrets through timing or shared-resource contention
* Exhaust device resources
* Trigger crashes or resets
* Recover residual data from previously used memory

### Out-of-scope capability

The tenant does not initially possess operator credentials or physical access.

### Important design implication

Every tenant-controlled field must be treated as untrusted, even when supplied through an official SDK.

A good interview statement:

> “The software API is not a trust boundary. A malicious tenant can bypass the SDK and construct raw requests.”

---

## 2. Compromised host OS, hypervisor, or driver

This is a more powerful attacker than a normal tenant.

### Capabilities

Assume the attacker can:

* Control the host operating system and device driver
* Read and modify all ordinary host memory
* Construct arbitrary PCIe command queues
* Supply arbitrary DMA descriptors
* Reorder, replay, omit, or alter device commands
* Reset or power-cycle the card through available host controls
* Lie about tenant identity
* Inspect traffic between user processes and the device
* Attempt to invoke management commands over PCIe

The attacker may also manipulate IOMMU configuration if the IOMMU is controlled entirely by the compromised host.

### Goals

They may try to:

* Access protected onboard memory
* Extract model weights or intermediate results
* Impersonate another tenant
* Install malicious firmware
* extract device keys
* Forge attestation
* cause cross-tenant DMA
* repeatedly reset the device to bypass security state
* downgrade firmware

### Important design implication

The card must enforce critical policy itself.

> “I would not rely solely on the host driver or host-configured IOMMU for tenant isolation because the host is inside the attacker model.”

The IOMMU can still provide defense in depth, but it is not the only enforcement mechanism.

---

## 3. Compromised tenant plus compromised host

It is useful to explicitly combine attackers.

A tenant that compromises the host may gain:

* Raw access to PCIe queues
* Control of DMA mappings
* Access to other tenant processes on that server
* Ability to manipulate reset and error paths

The architecture should still prevent access to:

* Other tenants’ onboard device memory
* Firmware update authority
* Device identity keys
* Other servers or cards
* Fleet management credentials

This tests whether the device has meaningful independence from its host.

---

## 4. Malicious or careless data-center operator

Operators often have legitimate privileges, which makes this threat subtle.

### Capabilities

An operator may be able to:

* Install or remove cards
* Reassign cards between servers
* Trigger firmware updates
* Access management consoles
* View telemetry
* Reset or quarantine devices
* Change fleet configuration
* Handle failed hardware

Not every operator has all of these privileges, so role separation matters.

### Goals or failure modes

A malicious or careless operator may:

* Deploy unauthorized firmware
* Access customer data
* disable security controls
* enable debug mode
* reassign a device without sanitizing it
* misuse break-glass access
* suppress or alter logs

### Important design implication

Do not treat “the operator” as one all-powerful trusted identity.

Use:

* Role-based permissions
* Least privilege
* Separation of duties
* Multi-party approval for sensitive operations
* Tamper-evident audit logs
* Short-lived credentials
* Explicit break-glass procedures

A good line:

> “I trust the organization’s control plane more than a tenant, but I do not assume every human operator should have unrestricted device authority.”

---

## 5. External attacker against the management plane

This attacker may not initially control a host or tenant workload.

### Capabilities

Assume they can:

* Reach exposed management services
* Steal or guess credentials
* Replay captured management requests
* Exploit service vulnerabilities
* Compromise an orchestration component
* Intercept traffic on insufficiently protected networks
* Attempt certificate or identity spoofing

### Goals

They may attempt to:

* Push malicious firmware
* admit counterfeit devices
* revoke legitimate devices
* alter workload assignment
* disable logging
* retrieve fleet-wide information
* cause a large-scale outage

### Blast radius

This attacker can be more dangerous than a compromised host because the management plane may control thousands of cards.

### Important design implication

Central authorization must be tightly constrained.

Controls may include:

* Mutual authentication
* Signed privileged commands
* Short validity windows
* Replay protection
* Scope-limited credentials
* Offline or isolated firmware-signing authority
* Independent policy checks on the device
* Segmentation between orchestration and signing

---

## 6. Compromised firmware-signing infrastructure

This should be treated as a distinct threat because of its fleet-wide impact.

### Capabilities

Assume the attacker may gain access to:

* A firmware signing key
* A build or release pipeline
* Firmware approval metadata
* Update manifests

### Goals

They may try to:

* Sign malicious firmware
* downgrade devices to a vulnerable version
* target selected devices
* deploy persistent backdoors
* disable attestation or logging

### Important design implication

A valid signature alone may not be enough.

The architecture can reduce risk through:

* Offline root keys
* Intermediate signing keys
* Key scopes by device family or environment
* Multi-party release approval
* Reproducible or independently verified builds
* Version policy
* Key revocation
* Emergency trust-anchor rotation
* Staged rollout and anomaly detection

A strong observation:

> “Secure boot protects against unsigned firmware, but it does not protect against misuse of an authorized signing key.”

---

## 7. Physical data-center attacker or technician

This attacker has non-invasive physical access.

### Capabilities

Assume they can:

* Remove or replace the PCIe card
* Attach probes to exposed board-level interfaces
* Connect to JTAG, UART, or test points
* Read removable storage chips
* Move a card to another server
* Power-cycle the device
* Replace it with a counterfeit or modified card

### Goals

They may try to:

* Extract stored customer data
* obtain device identity secrets
* enable debug interfaces
* clone or substitute a device
* install modified firmware
* bypass lifecycle controls

### Out of scope

Under our prior assumptions, they cannot:

* Decap chips
* perform advanced semiconductor probing
* defeat strong silicon-level protections using a laboratory attack

### Important design implication

Use proportional protections:

* Disable or authenticate debug interfaces
* Encrypt sensitive external nonvolatile storage
* Bind device identity to hardware
* Verify identity upon enrollment and boot
* Sanitize before repair or reassignment
* Avoid storing plaintext secrets in easily removable components

---

## 8. Supply-chain attacker

This attacker acts before deployment.

### Capabilities

Depending on scope, they may:

* Substitute components
* clone board identifiers
* modify firmware during manufacturing
* insert unauthorized test firmware
* steal provisioning credentials
* divert devices
* introduce counterfeit cards

### Goals

They may seek persistence before the device enters the data center.

### Trust assumption

We previously assumed the trusted chip design and trusted manufacturing facility are not malicious. Therefore, we are mostly defending against:

* Component substitution
* Provisioning mistakes
* Unauthorized devices
* Compromise outside the trusted manufacturing boundary

### Important design implication

Controls include:

* Secure provisioning
* Unique per-device identity
* Signed manufacturing and production firmware
* Lifecycle states
* Enrollment verification
* Inventory reconciliation
* Factory debug closure
* Certificate issuance only after verification

---

## 9. Malicious or vulnerable firmware

Firmware is also part of the trusted computing base, but it can contain vulnerabilities.

### Capabilities after compromise

Compromised firmware may be able to:

* Read all device memory
* bypass tenant isolation
* forge logs
* misuse device keys
* expose data through the host interface
* persist across workloads
* manipulate attestation if measurements do not cover runtime state

### Why this matters

Secure boot does not prove that approved firmware is safe. It only proves that it was authorized.

### Design implication

Reduce the firmware trusted computing base:

* Move critical access checks into hardware
* Use memory protection between firmware components
* Minimize privileged firmware
* Use safe parsing practices
* Enable update and revocation
* Add runtime fault detection where practical

A strong statement:

> “I would not place every isolation guarantee in a large monolithic firmware image, because one parser bug could compromise all tenants.”

---

## 10. Denial-of-service attacker

Availability deserves its own attacker view.

This attacker may be a tenant, compromised host, or external management attacker.

### Capabilities

They may:

* Submit infinite work
* create many contexts
* fill command queues
* cause repeated faults
* trigger watchdogs
* force resets
* exhaust memory
* create thermal or power pressure
* repeatedly fail authentication
* generate excessive logs

### Goals

* Starve other tenants
* force card-level resets
* degrade fleet capacity
* create operational noise
* hide another attack within failures

### Design implication

Use:

* Per-tenant quotas
* Scheduling fairness
* Timeouts
* Fault containment
* Rate limits
* Log throttling
* Resource accounting
* Per-context reset where possible
* Escalation from context termination to device quarantine

---

# Summarize the attacker model

At the end of this stage, give the interviewer a concise summary:

> “My strongest runtime attacker is a compromised host that can issue arbitrary PCIe commands, manipulate host memory, and trigger resets. The card must still protect device keys, firmware integrity, and tenant-separated onboard state. I also consider malicious tenants, external management-plane attackers, privileged operators, physical technicians, supply-chain substitution, and compromise of the firmware release pipeline. Invasive semiconductor attacks are out of scope. Because control-plane and signing compromises have fleet-wide blast radius, I will design stronger separation and recovery around those systems.”

---

# Map attackers to assets

A compact matrix helps you reason systematically:

| Threat actor              | Primary targets                                          |
| ------------------------- | -------------------------------------------------------- |
| Malicious tenant          | Other tenants’ data, shared resources                    |
| Compromised host          | Device memory, DMA, firmware interface, tenant identity  |
| Malicious operator        | Firmware policy, debug controls, telemetry, reassignment |
| Management-plane attacker | Fleet commands, admission, updates, availability         |
| Signing-key attacker      | Entire firmware trust chain                              |
| Physical technician       | Debug ports, storage, device substitution                |
| Supply-chain attacker     | Device identity, manufacturing firmware, components      |
| Compromised firmware      | All device-resident assets                               |
| DoS attacker              | Compute, memory, queues, resets, thermal limits          |

---

# What should come next

The next stage is **trust boundaries and high-level system decomposition**.

You would draw the major components:

```text
Customer workload
        |
Application / runtime
        |
Host OS and driver — untrusted
        |
PCIe boundary
        |
Device command interface
        |
Security monitor / command validation
        |
Tenant execution contexts
        |
Onboard memory and compute engines
```

Alongside that:

```text
Fleet management
        |
Attestation and admission service
        |
Firmware update service
        |
Device management interface
```

And beneath the device:

```text
Immutable root of trust
Secure boot chain
Device identity and protected key storage
```

The goal in the next stage will be to mark which components are trusted, which are untrusted, and where enforcement must occur.
