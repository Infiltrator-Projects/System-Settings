<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to System Settings

System Settings is a native C/C++ project. C, procedural C++ and object-oriented C++ are equal implementation choices; use the form that expresses the actual component most clearly and safely.

## Engineering rules

- Preserve the one-shell/many-modules architecture.
- Keep domain policy inside its owning module.
- Keep the module ABI small, versioned and C-compatible.
- Do not expose C++ ABI types across the module boundary.
- Keep startup lightweight; do not load or enumerate unrelated domains before they are needed.
- Prefer authoritative native APIs/services over parsing external command output.
- Keep the shell unprivileged.
- Request authorisation only for the protected action being performed.
- Never add a project-owned password collection path when the platform authorisation agent owns credentials.
- Treat Common as the authoritative home for generic reusable mechanisms when it is at least as strong as the local implementation.
- Do not move settings-domain policy into Common merely to increase reuse.
- Represent unavailable, unsupported, denied and cancelled states explicitly.
- Verify writable settings against authoritative resulting state where practical.
- Add regression coverage with behaviour changes.

## Module work

Read [docs/MODULES.md](docs/MODULES.md) before introducing or changing a module.

A new module should not begin as UI code. Establish:

1. domain ownership;
2. authoritative backend;
3. read semantics;
4. write/transaction semantics;
5. privilege requirements;
6. external change behaviour;
7. manifest/search/deep-link identities;
8. failure states;
9. validation strategy.

Then build the smallest implementation that proves those contracts.

## Common

The exact Common revision is pinned when implementation begins.

If System Settings develops a generic helper stronger than Common, improve Common to preserve those strengths and then consume the improved exact revision. Do not weaken a correct specialised implementation merely to claim reuse.

## Documentation

[docs/README.md](docs/README.md) defines document ownership.

Update the owning Markdown in the same change that alters a durable contract. Planned work belongs in the roadmap. Source comments explain local invariants, ownership, concurrency, ABI/security quirks and non-obvious reasons.

## Repository discipline

Normal development stays on `main`. Keep commits coherent and forward-moving. Once CI exists, `main` should remain buildable and the exact release revision must pass the required gates before publication.

Do not create long-lived architectural branches.

## Licence

Contributions are accepted under GPL-3.0-or-later unless explicitly agreed otherwise.
