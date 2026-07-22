Let’s do this as an interactive mock interview rather than solving it immediately.

## Mock system-design question

> **Design a secure hardware accelerator card for use in a multi-tenant data center.**
>
> The card plugs into servers over PCIe and performs computationally expensive operations for customer workloads. Multiple customers may share the same physical card. The card has onboard memory, firmware, and a management interface used by the data-center operator.
>
> Design the security architecture for the system.

This question is broad enough to test:

* Hardware and software trust boundaries
* Secure boot and firmware updates
* PCIe and DMA security
* Multi-tenant isolation
* Device identity and attestation
* Management-plane security
* Physical and supply-chain threats
* Availability and operational recovery

## How we will work through it

We will handle it in stages:

1. Clarify requirements and assumptions
2. Define assets and security goals
3. Identify threat actors
4. Draw trust boundaries
5. Propose the high-level architecture
6. Walk through boot, provisioning, and normal request flows
7. Threat-model the design
8. Cover firmware updates, failures, recovery, and decommissioning
9. Summarize the most important tradeoffs

At each stage, you will answer first as though you were in the interview. I will then:

* Point out what was strong
* Identify missing questions or risks
* Suggest a more systematic way to phrase your answer
* Move to the next stage

## What not to do initially

Do not start with secure boot, encryption, TPMs, or implementation details.

Your first job is to reduce ambiguity.

A strong opening might be:

> “Before proposing an architecture, I’d like to clarify the workload, deployment model, trust assumptions, and the security properties we need.”

## Stage 1: clarifying questions

You are now the candidate.

Ask the interviewer the clarifying questions you would ask before designing the system. Try to organize them into categories rather than listing random questions.

Aim for around **8–12 questions**.
