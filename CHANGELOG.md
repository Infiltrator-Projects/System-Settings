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

### Status

No supported binary release exists yet. The repository is in architecture/bootstrap stage.
