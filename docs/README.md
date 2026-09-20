<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Documentation

This directory is the maintained engineering map for System Settings. Each document has one job. When implementation changes a contract, update the owning document instead of creating a second description elsewhere.

## Canonical documents

- [ARCHITECTURE.md](ARCHITECTURE.md) — component boundaries, data flow, module lifecycle, concurrency and platform integration.
- [DESIGN.md](DESIGN.md) — user-facing interaction model, navigation, search, apply behaviour, presentation and accessibility.
- [MODULES.md](MODULES.md) — module discovery, manifests, ABI, lifecycle, deep links, state and trust requirements.
- [PRESENTATION_POLICY.md](PRESENTATION_POLICY.md) — system-wide user presentation policy, beginning with temporal/clock/calendar formatting.
- [MINT_COMPATIBILITY.md](MINT_COMPATIBILITY.md) — mapping to existing Mint/Cinnamon authorities, Infiltrator extensions, fallbacks and optional enhanced integration.
- [DECISIONS.md](DECISIONS.md) — durable architectural decision records and their consequences.
- [ROADMAP.md](ROADMAP.md) — ordered implementation direction; not a promise of dates.
- [VALIDATION.md](VALIDATION.md) — evidence required before behaviour is described as working.

Repository-level [README.md](../README.md) is the product overview. [SECURITY.md](../SECURITY.md) owns vulnerability reporting and the privilege/trust model at a project level. [CONTRIBUTING.md](../CONTRIBUTING.md) owns development rules. [CHANGELOG.md](../CHANGELOG.md) records released/user-visible change history.

## Documentation rule

Documentation describes the strongest contract that the source and tests actually implement. Planned behaviour belongs in the roadmap or is labelled planned. A proposed interface is not described as a supported interface merely because it has been designed.

## Comments versus Markdown

Markdown owns durable product and architectural contracts. Source comments should explain local invariants, ownership, concurrency, ABI quirks, security boundaries and non-obvious reasoning. Comments should not duplicate entire architecture documents or narrate obvious statements.

## Avoiding drift

When two documents appear to overlap, choose one owner:

- shell/module boundaries → Architecture;
- exact module contract → Modules;
- cross-application representation policy → Presentation Policy;
- Mint/Cinnamon reuse, mapping and fallback policy → Mint Compatibility;
- interaction and visual behaviour → Design;
- why a durable choice was made → Decisions;
- not-yet-complete work → Roadmap;
- proof and acceptance → Validation.

If a rule needs to be visible in several places, keep its full definition in one owning document and link to it from the others.
