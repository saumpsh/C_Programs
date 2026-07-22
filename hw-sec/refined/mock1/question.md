# Mock System Design Question: Multi-Tenant Hardware Accelerator

> **Question**:  
> Design a secure hardware accelerator card for use in a multi-tenant data center.  
> The card plugs into host servers over PCIe and performs computationally expensive operations (e.g., machine learning inference) for customer workloads.  
> Multiple customers may share the same physical card concurrently and over time. The card includes onboard DRAM, firmware, and a management interface used by data-center operators.  
> **Design the end-to-end security architecture.**

---

## Stage Roadmap

This mock problem is structured into 8 modular stages:

1. [Stage 1: Clarifying Questions](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage1.md) – Scoping workload, host trust, and physical boundaries.
2. [Stage 2: Assets & Security Objectives](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage2.md) – Defining assets, objectives, and 6 core security invariants.
3. [Stage 3: Threat Actors & Capabilities](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage3.md) – Threat modeling tenant, host, physical, and supply-chain actors.
4. [Stage 4: Trust Boundaries & System Architecture](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage4.md) – Layered system diagram and enforcement points.
5. [Stage 5: High-Level Architecture & Lifecycle Flows](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage5.md) – 12 end-to-end state transitions (provisioning to decommissioning).
6. [Stage 6: Attack Walkthroughs & Validations](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage6.md) – Stress-testing the design against 13 concrete attack vectors.
7. [Stage 7: Architectural Tradeoffs & Prioritization](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage7.md) – Hardware vs. firmware, reset granularity, P0–P7 implementation priorities.
8. [Stage 8: Closing Synthesis & Follow-Ups](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage8.md) – Executive summaries, top 3 risks, and 12 interviewer pivot scenarios.

---
*For foundational hardware security theory and general framework principles, refer to the [SSOT Reference](file:///Users/saumil/repos/C_Programs/hw-sec/refined/intro.md).*
