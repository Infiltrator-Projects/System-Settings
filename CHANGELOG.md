<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Changelog

All notable user-visible and architectural changes are recorded here.

## Unreleased

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

- Added the first Date & Time semantic module model and static module manifest.
- Added portable system/12-hour/24-hour/decimal-10 policy selection through Common 1.19.14.
- Added Linux XDG and Windows LocalAppData per-user persistence adapters.
- Added Linux and Windows CI for the first executable integration slice.
- Added a temporary `system-settings-time` integration utility for exercising the policy before the full shell UI exists.
- Kept platform persistence injected behind the model so the core has no Linux/Windows dependency cycle.
- Began Calendar integration as the first consumer of the system temporal policy.

### Status

No supported binary release exists yet. The repository is in architecture/bootstrap stage.
