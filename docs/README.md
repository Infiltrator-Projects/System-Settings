<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Documentation

This directory is the maintained engineering map for System Settings. Each document has one job. When implementation changes a contract, update the owning document instead of creating a second description elsewhere.

## Canonical documents

- [Engineering philosophy](PHILOSOPHY.md) — governing engineering and documentation standard.
- [ARCHITECTURE.md](ARCHITECTURE.md) — component boundaries, data flow, module lifecycle, concurrency and platform integration.
- [DESIGN.md](DESIGN.md) — user-facing interaction model, navigation, search, apply behaviour, presentation and accessibility.
- [UI-VISION.md](UI-VISION.md) — current visual north star and product-specific graphical direction; subordinate to Design, accessibility/platform requirements and Common's canonical design contract.
- [MODULES.md](MODULES.md) — module discovery, manifests, ABI, lifecycle, deep links, state and trust requirements.
- [PRESENTATION_POLICY.md](PRESENTATION_POLICY.md) — system-wide user presentation policy, beginning with temporal/clock/calendar formatting.
- [HISTORICAL_TEMPORAL_PRESENTATION.md](HISTORICAL_TEMPORAL_PRESENTATION.md) — specialist presentation rules for historical clock and calendar systems whose native units or notation differ from modern civil time.
- [TEMPORAL_PROVIDER_DISCOVERY.md](TEMPORAL_PROVIDER_DISCOVERY.md) — concept for filesystem-discovered calendar/clock resources; System Settings eventually enumerates what Common discovers rather than owning a master list.
- [EGYPTIAN_FIRST_PROFILE.md](EGYPTIAN_FIRST_PROFILE.md) — selector contract for the first Egyptian profile: Early Egyptian Lunar Calendar plus Early Egyptian Stellar–Solar Time, following Calendar's maintained best-evidence model.
- [PLATFORM_COMPATIBILITY.md](PLATFORM_COMPATIBILITY.md) — shared cross-platform authority, capability, adapter, UI/ABI and handoff rules.
- [MINT_COMPATIBILITY.md](MINT_COMPATIBILITY.md) — Mint/Cinnamon-specific mapping, fallbacks and integration.
- [WINDOWS_COMPATIBILITY.md](WINDOWS_COMPATIBILITY.md) — Windows-specific globalisation, privilege, registry and Settings-handoff rules.
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
- temporal resource discovery → Temporal Provider Discovery, subordinate to Common's canonical resource design;
- historical clock/calendar display precision → Historical Temporal Presentation, subordinate to Presentation Policy and the owning Calendar model evidence;
- first Egyptian selector pairing → Egyptian First Profile, subordinate to Calendar's owning historical model;
- cross-platform authority and adapter rules → Platform Compatibility;
- Mint/Cinnamon-specific mapping → Mint Compatibility;
- Windows-specific mapping → Windows Compatibility;
- interaction and visual behaviour → Design;
- product-specific visual north star and graphical direction → UI Vision, within Design/Common constraints;
- why a durable choice was made → Decisions;
- not-yet-complete work → Roadmap;
- proof and acceptance → Validation.

If a rule needs to be visible in several places, keep its full definition in one owning document and link to it from the others.
