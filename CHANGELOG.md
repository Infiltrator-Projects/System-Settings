<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Changelog

All notable user-visible and architectural changes are recorded here.

## Unreleased

No unreleased changes.

## 0.3.3 — 2026-09-21

- Pin the final Common 1.19.19 release revision containing the shared POSIX temporal authority contract and release-build-safe contract test.
- Retain the explicit temporal-v3 provider marker, shared atomic policy store and Mint/Cinnamon compatibility behaviour introduced in 0.3.2.

## 0.3.2 — 2026-09-21

- Publish the explicit `temporal-v3` provider capability marker so consumers can detect the installed System Settings authority without probing executable names or PATH.
- Replace the Linux Date & Time module's duplicated XDG path, policy parsing and atomic-write code with Common 1.19.19's shared POSIX temporal store.
- Preserve Mint/Cinnamon as the initial/default authority until the user actually saves an Infiltrator temporal policy.

## 0.3.1 — 2026-09-21

- Seed the first Linux Date & Time policy from Cinnamon's native 12/24-hour and seconds preferences when no Infiltrator policy has been saved yet.
- Mirror exact conventional choices back to Cinnamon when System Settings saves them, keeping stock Mint consumers aligned without pretending richer Infiltrator clocks or calendars have a native equivalent.
- Keep the Infiltrator policy optional for consumers so Calendar remains a standalone Mint replacement before System Settings is installed or configured.
- Document the native-fallback/enrichment boundary as a durable Mint/Cinnamon compatibility contract.

## 0.3.0 — 2026-09-20

- Moved Date & Time to Common 1.19.18 temporal policy v3.
- Removed the retired secondary-calendar feature from the model, GUI, CLI, search manifest and persisted policy.
- Replaced primary/secondary terminology with one authoritative system Calendar setting.
- System Settings now writes `calendar=...` alongside clock mode, seconds and location; current consumers read that policy directly.
- Removed the consumer-side “Follow System Settings” model from the architecture: following System Settings is the normal contract, not an optional override mode.
- Retained private migration of existing v2 presentation.conf data so the previous primary calendar becomes the single v3 calendar while the retired secondary value is discarded.
- Preserved Linux and Windows persistence with the same one-calendar policy contract.

## 0.2.0 — 2026-09-20

- Replaced the four-profile bootstrap model with Common 1.19.16 temporal policy v2.
- Made System Settings the authority for the complete temporal environment rather than only a clock-format preference.
- Added all 21 shared clock systems, all 30 primary calendars, optional secondary calendar, seconds policy and geographic latitude/longitude to Date & Time.
- Removed the global self-referential “Follow system” clock concept; the global conventional default is Standard time (OS locale).
- Added deterministic v1-to-v2 policy migration through Common and retained one authoritative per-user presentation.conf store on Linux and Windows.
- Updated the Linux GTK4 panel, CLI, model tests and Calendar integration contract to use the same stable clock/calendar identifiers.

## 0.1.0 — 2026-09-20

### Architecture

- Established System Settings as one unified shell over focused first-party settings modules.
- Defined static manifest discovery so navigation and search do not require loading every module.
- Chosen a versioned C ABI for the future native module boundary while retaining C/C++ freedom inside modules.
- Defined lazy module/panel loading and first-paint-oriented startup.
- Defined trusted system-owned module locations; arbitrary user-writable in-process plugins are not part of the initial model.
- Defined operation-scoped authorisation with an unprivileged shell.
- Defined authoritative-state read-back/reconciliation after settings writes.
- Defined logical deep-link/search identities independent of widget layout.
- Defined Common as shared presentation/infrastructure while keeping settings-domain policy local.
- Added the initial phased roadmap and validation/security requirements.
- Defined integration boundaries so Software, System Monitor and Defragmenter retain ownership of their specialised domains while Settings provides launch/deep-link entry points where appropriate.
- Defined system-wide presentation policy: System Settings owns the user's policy, Common provides shared formatting, and applications retain canonical data. Temporal Presentation is the first family, including the planned decimal 10-hour clock model.
- Defined Mint/Cinnamon compatibility policy: reuse existing authoritative desktop settings, add Infiltrator extensions only for missing semantics, preserve safe conventional fallbacks, and avoid unnecessary upstream forks.

- Promoted Windows from a future portability concern to a first-class target from Phase 1.
- Added shared platform-compatibility and Windows-specific compatibility contracts.
- Prohibited GTK, HWND, WinUI and other platform UI objects from the public module ABI.
- Generalised trusted module loading to Linux shared objects and Windows DLLs.
- Added native Windows Settings handoff as a supported integration mode where no safe public write API exists.
- Generalised validation, privilege and security contracts for Linux and Windows.

### Implementation

- Added the first native GTK4 System Settings shell on Linux with a visible Date & Time panel.
- Added live clock/date preview, system/12-hour/24-hour/decimal-10 selection, seconds control and current time-zone display.
- Added desktop integration and install rules so the shell appears as System Settings after installation.

- Added the first Date & Time semantic module model and static module manifest.
- Added portable system/12-hour/24-hour/decimal-10 policy selection through Common 1.19.14.
- Added Linux XDG and Windows LocalAppData per-user persistence adapters.
- Added Linux and Windows CI for the first executable integration slice.
- Added a temporary `system-settings-time` integration utility for exercising the policy before the full shell UI exists.
- Kept platform persistence injected behind the model so the core has no Linux/Windows dependency cycle.
- Began Calendar integration as the first consumer of the system temporal policy.

### Status

0.1.0 was the first bootstrap binary release; 0.2.0 replaces its limited temporal model with the complete authority described above.
