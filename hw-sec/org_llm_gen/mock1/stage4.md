# Stage 4: Trust boundaries and system decomposition

A strong transition is:

> “Now that I’ve defined the attackers, I’ll decompose the system into components and mark the trust boundaries. The goal is to make clear where validation, isolation, and authentication must happen.”

This stage should produce a simple architecture diagram, not a detailed final design.

## 1. Start with the main components

Draw the system in three layers.

```text
Customer workload
      |
Application / runtime
      |
Host OS / hypervisor / driver
      |
================ PCIe trust boundary ================
      |
Device command interface
      |
Security monitor / command validator
      |
Tenant execution contexts
      |
Compute engines + onboard memory
```

Then draw the management path separately:

```text
Fleet management service
      |
Attestation / admission service
      |
Firmware update service
      |
========== management trust boundary ==========
      |
Device management interface
```

And underneath the device:

```text
Immutable boot ROM
      |
Verified bootloader
      |
Verified firmware
      |
Protected device identity and key storage
```

## 2. Mark what is trusted and untrusted

For this design:

### Untrusted

* Customer workload
* Customer-controlled model and input data
* Host application
* Host OS
* Hypervisor
* Device driver
* Host memory containing queues and DMA buffers
* PCIe command traffic

This is important:

> “Even though the driver is operator-provided, I treat it as untrusted because the host may be compromised.”

### Trusted, but attackable

* Device security monitor
* Device firmware
* Hardware isolation logic
* Management services
* Attestation service
* Firmware release pipeline

These components are trusted because security depends on them, but they are not assumed infallible.

### Highly trusted roots

* Immutable boot ROM
* Root public-key hash
* Device root secret
* Hardware lifecycle state
* Anti-rollback state

These should be small, simple, and difficult to modify.

## 3. Define the important trust boundaries

A good interview answer identifies at least five.

### Boundary 1: Tenant to host software

The tenant submits workloads through software APIs.

Risk:

* Malformed input
* Resource abuse
* Attempts to escape a tenant context

Control point:

* Host-side validation can improve reliability, but it is not sufficient for security.

### Boundary 2: Host to accelerator over PCIe

This is the most important runtime boundary.

Risk:

* Arbitrary commands
* Malicious DMA descriptors
* Replays
* Forged tenant identifiers
* Unauthorized management requests
* Reset abuse

Control point:

* Device-side command validation
* Device-side ownership checks
* Restricted DMA
* Separate privileged interface

### Boundary 3: Tenant context to tenant context

Even inside the device, tenants must be isolated.

Risk:

* Cross-tenant memory access
* Queue confusion
* Cache leakage
* Output misrouting
* Resource starvation

Control point:

* Hardware-enforced context IDs
* Per-tenant address spaces
* Queue ownership
* Resource quotas
* Zeroization

### Boundary 4: Firmware to hardware root of trust

Firmware is mutable and potentially vulnerable; the root of trust is not.

Risk:

* Malicious firmware
* Downgrade
* Corrupted configuration
* Debug enablement

Control point:

* Secure boot
* Measured boot
* Rollback protection
* Hardware-enforced lifecycle state

### Boundary 5: Management plane to device

The management system sends privileged commands.

Risk:

* Forged commands
* Replay
* Overprivileged operators
* Compromised orchestration
* Fleet-wide abuse

Control point:

* Mutual authentication
* Signed commands
* Authorization scopes
* Freshness
* Device-local policy checks

### Boundary 6: Manufacturing to production

The device changes from factory state to production state.

Risk:

* Development keys retained
* Debug interfaces left enabled
* Duplicate identity
* Counterfeit enrollment

Control point:

* Secure provisioning
* Lifecycle fuses
* Certificate enrollment
* Factory-to-production transition

## 4. Identify the enforcement points

A very strong move is to say where each security property is enforced.

| Security property                | Primary enforcement point                  |
| -------------------------------- | ------------------------------------------ |
| Firmware authenticity            | Boot ROM                                   |
| Rollback prevention              | Protected monotonic state                  |
| Device identity                  | Hardware-protected key storage             |
| Command validation               | Device command processor                   |
| Tenant memory isolation          | Device MMU / hardware checks               |
| DMA restriction                  | Device-side DMA engine plus IOMMU          |
| Privileged command authorization | Management security monitor                |
| Secure deletion                  | Memory controller / context teardown logic |
| Admission policy                 | Control plane plus attestation verifier    |
| Resource fairness                | Device scheduler                           |

The principle is:

> “The most critical properties should be enforced as close as possible to the asset and should not rely on a weaker upstream component.”

## 5. Keep the trusted computing base small

The interviewer may ask what belongs in the trusted computing base.

Include:

* Boot ROM
* Secure boot verification
* Device security monitor
* Memory protection logic
* Key-management logic
* Minimal privileged firmware
* Attestation implementation

Try not to include:

* Full host driver
* User SDK
* Large orchestration stack
* General tenant runtime
* Debug tools
* Complex model-processing code, where possible

A strong statement:

> “I would separate the small security-critical control path from the larger performance-oriented execution path so a bug in model processing does not automatically expose device keys or bypass tenant isolation.”

## 6. Distinguish control plane and data plane

This is often a key interview point.

### Data plane

Handles:

* Workload commands
* Model loading
* Input buffers
* Execution
* Results

Properties:

* High throughput
* Low latency
* Tenant-scoped
* Narrow command set

### Control plane

Handles:

* Device admission
* Firmware updates
* Security configuration
* Attestation
* Quarantine
* Device reset policy

Properties:

* Strong authentication
* Lower volume
* More auditing
* Strict authorization
* Separate credentials

Even if both use PCIe, they should be logically separate.

> “Sharing a physical transport does not mean sharing authorization or command parsing.”

## 7. Walk through one normal request

This helps validate the boundaries.

Example flow:

1. Control plane admits an attested device.
2. Control plane authorizes a tenant context.
3. Device creates a hardware context with a unique context ID.
4. Host registers DMA buffers for that context.
5. Tenant submits commands through its queue.
6. Device validates:

   * queue ownership
   * context ID
   * command format
   * buffer ranges
   * permitted operation
7. Device executes using tenant-scoped memory.
8. Output is written only to authorized buffers.
9. On context teardown:

   * keys are destroyed
   * memory is zeroized
   * queues are invalidated
   * context ID cannot be reused immediately without reinitialization

This flow exposes where checks are needed.

## 8. Important design question: who tells the card the tenant identity?

This is a subtle issue.

The host cannot simply say:

> “This command belongs to tenant A.”

Because the host may be compromised.

Possible design:

* The control plane issues a signed, short-lived tenant-context authorization.
* The device verifies it.
* The device creates an internal opaque context handle.
* Host commands refer to that handle.
* The host cannot create or modify tenant authorization on its own.

This avoids trusting arbitrary host-provided tenant IDs.

## 9. Important design question: can the host read onboard memory?

Prefer:

* No direct unrestricted host mapping of secure onboard memory
* Access only through validated commands
* Explicitly shared regions where needed
* Device MMU enforcement
* Sensitive firmware and key regions inaccessible to DMA and tenant engines

You can say:

> “I would avoid exposing raw onboard physical addresses to the host. The host should operate through opaque handles and validated mappings.”

## 10. What the whiteboard should look like

A compact version:

```text
                   CONTROL PLANE
        +--------------------------------+
        | Fleet Manager                  |
        | Attestation Verifier           |
        | Firmware Update Service        |
        +---------------+----------------+
                        |
                 Authenticated commands
                        |
================ MANAGEMENT BOUNDARY =================

UNTRUSTED HOST                         TRUSTED DEVICE
+-------------------+         +---------------------------+
| Tenant workload   |         | Command validation        |
| Runtime / SDK     | PCIe    | Security monitor          |
| OS / hypervisor   |-------> | Tenant context manager    |
| Driver            | DMA     | Device MMU / DMA engine   |
| Host memory       |<------> | Compute + onboard memory  |
+-------------------+         +-------------+-------------+
                                            |
                                  +---------+---------+
                                  | Boot ROM          |
                                  | Root keys         |
                                  | Rollback state    |
                                  | Device identity   |
                                  +-------------------+
```

Label:

* Host side: untrusted
* Device security monitor: trusted
* Compute engines: constrained, not necessarily fully trusted
* Boot ROM and key storage: root of trust

## A polished interview version

> “I would divide the system into an untrusted host side, a trusted device control layer, a tenant execution layer, and a separate management plane. The host OS, driver, host memory, and PCIe requests are untrusted. All commands cross a device-side validation boundary before reaching tenant execution resources.
>
> Inside the device, a small security monitor owns context creation, queue binding, DMA mappings, key access, and privileged operations. Tenant execution contexts are isolated through hardware context identifiers, device-side address translation, and per-context resource ownership.
>
> Beneath that is a minimal hardware root of trust containing the boot ROM, root trust anchor, protected device identity, and rollback state. The management plane is logically separate from the workload data plane and uses authenticated, authorized, fresh commands.
>
> The main trust boundaries are host-to-device, tenant-to-tenant, firmware-to-root-of-trust, management-plane-to-device, and manufacturing-to-production. I’ll place each critical security check at the lowest practical layer rather than relying on the host.”

# Next stage

The next stage is the **high-level security architecture and lifecycle flows**.

We would design:

1. Device provisioning and identity
2. Secure and measured boot
3. Remote attestation and admission
4. Tenant context creation
5. Normal command and DMA flow
6. Context teardown and zeroization
7. Firmware update and recovery

This is where the diagram becomes an actual architecture rather than only a trust-boundary map.
