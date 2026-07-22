# Refinement & De-duplication Log

This document tracks all changes made when restructuring the hardware security documentation from [`hw-sec/org_llm_gen/`](file:///Users/saumil/repos/C_Programs/hw-sec/org_llm_gen) to [`hw-sec/refined/`](file:///Users/saumil/repos/C_Programs/hw-sec/refined). 

All original files remain 100% intact in `hw-sec/org_llm_gen/` for reference.

---

## Global Refinement Strategy Summary

1. **Single Source of Truth (SSOT)**: Extracted theoretical concepts (Secure Boot, RoT, PUF, Attestation, DMA/IOMMU, A/B Updates) into [`refined/intro.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/intro.md).
2. **Canonical Architecture Diagram**: Replaced 4 repeated ASCII diagrams across stage files with a single master diagram in [`refined/mock1/stage4.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage4.md).
3. **Structured Micro-Cards & Tables**: Transformed verbose repetitive text into high-density tables, 6 Core Invariants, and structured attack micro-cards (`Attack` → `Invariant` → `Prevention` → `Residual Risk`).
4. **Candidate Script Consolidation**: Consolidated multi-paragraph script repetitions at the end of intermediate stages into clear executive summaries in [`refined/mock1/stage8.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage8.md).

---

## Detailed File-by-File Change Log

| Original Source File | Refined Target File | Summary of Refinements & De-duplications |
| :--- | :--- | :--- |
| **`org_llm_gen/intro.md`** | [`refined/intro.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/intro.md) | **Refined into SSOT Theory Reference**. Consolidated general 7-step design framework and theoretical definitions (RoT, PUF, Measured Boot, Attestation Nonces, DMA/IOMMU, A/B Updates, Zeroization, Whiteboard layout). Stripped mock-specific interview prose. |
| **`org_llm_gen/mock1/question.md`** | [`refined/mock1/question.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/question.md) | **Cleaned & Linked Roadmap**. Formatted prompt cleanly and created clickable links to all 8 refined stage files and `intro.md`. |
| **`org_llm_gen/mock1/stage1.md`** | [`refined/mock1/stage1.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage1.md) | **De-duplicated Theory from Questions**. Kept the 12 candidate Q&As and the consolidated assumption summary. Removed redundant background explanations of PCIe/DMA and firmware updates covered in `intro.md`. |
| **`org_llm_gen/mock1/stage2.md`** | [`refined/mock1/stage2.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage2.md) | **Formulated Core Invariants**. Transformed asset discussions into a compact Asset-to-Objective Table and established the **6 Core Security Invariants** used for validation in later stages. |
| **`org_llm_gen/mock1/stage3.md`** | [`refined/mock1/stage3.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage3.md) | **Created Threat Actor Matrix**. Streamlined threat modeling into 10 Threat Actor Cards and a concise Threat Actor vs. Target Asset Matrix. Removed preview ASCII diagrams. |
| **`org_llm_gen/mock1/stage4.md`** | [`refined/mock1/stage4.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage4.md) | **Established Master System Architecture**. Created the single authoritative ASCII System Diagram, 3-layer trust level definitions, and Trust Boundary Enforcement Matrix. Removed candidate script repeats. |
| **`org_llm_gen/mock1/stage5.md`** | [`refined/mock1/stage5.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage5.md) | **Streamlined State Transitions**. Standardized the 12 End-to-End Lifecycle Sequence Flows (manufacturing to decommissioning). Replaced long narrative explanations with structured step-by-step flows. |
| **`org_llm_gen/mock1/stage6.md`** | [`refined/mock1/stage6.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage6.md) | **Converted Attacks to Micro-Cards**. Converted 13 attack walkthroughs into micro-cards explicitly linked to Stage 2 Invariants (`Attack` → `Invariant` → `Prevention` → `Residual Risk`). |
| **`org_llm_gen/mock1/stage7.md`** | [`refined/mock1/stage7.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage7.md) | **Structured Tradeoffs & Roadmap**. Formalized 4 core architectural tradeoffs, structured P0–P7 implementation priority matrix, and listed explicit residual risks. |
| **`org_llm_gen/mock1/stage8.md`** | [`refined/mock1/stage8.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage8.md) | **Consolidated Summaries & Pivots**. Combined all executive summaries (2-min & 30-sec pitch), top 3 risks, and created a quick-reference matrix for 12 scenario pivots. |

---

## File Path Cross-Reference Mapping

| Document Topic | Original Path | Refined Path |
| :--- | :--- | :--- |
| Framework & Theory | `hw-sec/org_llm_gen/intro.md` | [`hw-sec/refined/intro.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/intro.md) |
| Question & Roadmap | `hw-sec/org_llm_gen/mock1/question.md` | [`hw-sec/refined/mock1/question.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/question.md) |
| Stage 1: Clarification | `hw-sec/org_llm_gen/mock1/stage1.md` | [`hw-sec/refined/mock1/stage1.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage1.md) |
| Stage 2: Assets & Invariants | `hw-sec/org_llm_gen/mock1/stage2.md` | [`hw-sec/refined/mock1/stage2.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage2.md) |
| Stage 3: Threat Actors | `hw-sec/org_llm_gen/mock1/stage3.md` | [`hw-sec/refined/mock1/stage3.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage3.md) |
| Stage 4: Trust Architecture | `hw-sec/org_llm_gen/mock1/stage4.md` | [`hw-sec/refined/mock1/stage4.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage4.md) |
| Stage 5: Lifecycle Flows | `hw-sec/org_llm_gen/mock1/stage5.md` | [`hw-sec/refined/mock1/stage5.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage5.md) |
| Stage 6: Attack Validations | `hw-sec/org_llm_gen/mock1/stage6.md` | [`hw-sec/refined/mock1/stage6.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage6.md) |
| Stage 7: Tradeoffs & P0-P7 | `hw-sec/org_llm_gen/mock1/stage7.md` | [`hw-sec/refined/mock1/stage7.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage7.md) |
| Stage 8: Synthesis & Pivots | `hw-sec/org_llm_gen/mock1/stage8.md` | [`hw-sec/refined/mock1/stage8.md`](file:///Users/saumil/repos/C_Programs/hw-sec/refined/mock1/stage8.md) |
