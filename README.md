<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Settings

**Project copyright:** © 2000-2026 Shannon Smith

System Settings is a native settings environment for Linux Mint/Cinnamon. Its user interface is deliberately unified, but its implementation is deliberately modular: one searchable Settings shell presents many small, independently owned control panels.

The project takes the strongest part of the classic Amiga/Control Panel model — small focused preference tools with clear ownership — and combines it with the strongest part of a modern settings application — one consistent window, global search, deep links, shared navigation and predictable privilege handling.

**Status:** architecture and design foundation; no supported release yet  
**Primary target:** Linux Mint/Cinnamon  
**Implementation:** native C/C++, using the strongest style for each component  
**Shared foundation:** Infiltrator Common, pinned to an exact release when implementation begins  
**Licence:** GPL-3.0-or-later

## Core idea

System Settings is **one product, not one monolith**.

The user sees one application:

```text
System Settings
├── Appearance
├── Hardware
├── Network
├── System
└── Security
```

Internally, the shell discovers focused modules:

```text
system-settings
        │
        ├── appearance
        ├── displays
        ├── sound
        ├── keyboard
        ├── mouse
        ├── network
        ├── bluetooth
        ├── printers
        ├── users
        ├── date-time
        ├── storage
        ├── startup
        ├── security
        └── system-information
```

A module owns its domain. The shell owns the common experience.

This separation is not cosmetic. It prevents the settings application becoming a large collection of unrelated platform code coupled through one executable source tree, while avoiding the discoverability problems of a folder full of independent preference programs.

## Product goals

System Settings should:

- present the machine's configuration through one coherent, searchable GUI;
- keep each settings domain independently testable and maintainable;
- allow direct navigation to a category, panel or individual setting;
- use authoritative native APIs and services rather than parsing command-line utility output where a stronger interface exists;
- start without administrative privilege and request authentication only for the specific operation that requires it;
- keep slow discovery and system calls away from the UI thread;
- use Common for shared presentation and reusable project infrastructure without moving domain-specific policy into Common;
- minimise avoidable dependencies without recreating mature operating-system subsystems;
- represent unsupported, unavailable and permission-denied states explicitly rather than pretending an operation succeeded;
- preserve a clean path for future modules without requiring the shell itself to be redesigned.

## User experience

The default window is category-oriented rather than an undifferentiated scrolling list. The initial category model is:

- **Appearance** — theme, fonts, desktop and visual behaviour;
- **Hardware** — displays, sound, keyboard, mouse/touchpad, Bluetooth and printers;
- **Network** — Wi-Fi, Ethernet, VPN, proxy and related connectivity;
- **System** — users, date/time, storage, startup, power, system information and entry points to specialised system applications such as Software;
- **Security** — authentication, firewall/security surfaces and permission-related configuration.

These categories are navigation aids, not code boundaries. A module may expose settings under more than one searchable term while retaining one clear implementation owner.

### Search

Search is a first-class interface, not a filter over panel names.

A module can publish searchable entries down to individual settings:

```text
"lid"  → Power → Close laptop lid
"dns"  → Network → Wi-Fi → DNS
"dns"  → Network → Ethernet → DNS
"font" → Appearance → Fonts
```

Search results deep-link directly to the relevant panel and setting. Search metadata must be available without constructing every panel, so startup remains fast.

### Direct invocation

The shell should support direct navigation conceptually equivalent to:

```text
system-settings
system-settings display
system-settings network
system-settings users
system-settings search:dns
```

The exact command-line contract will be finalised with the first implementation, but deep-link identity is part of the architecture from the beginning.

## Architecture

```text
                         SYSTEM SETTINGS SHELL
                 navigation / search / history / UI
                                │
                module discovery + stable panel API
                                │
       ┌────────────────────────┼────────────────────────┐
       ▼                        ▼                        ▼
    Display                  Network                   Users
    module                   module                    module
       │                        │                        │
       └────────────────────────┼────────────────────────┘
                                ▼
                       NATIVE SYSTEM BACKENDS
              GSettings / D-Bus / system services /
               kernel interfaces / desktop services

                         INFILTRATOR COMMON
                theme / typography / shared widgets /
              generic persistence / common infrastructure
```

See [Architecture](docs/ARCHITECTURE.md), [Design](docs/DESIGN.md), [Module contract](docs/MODULES.md), [Decisions](docs/DECISIONS.md), [Roadmap](docs/ROADMAP.md) and [Validation](docs/VALIDATION.md).

## Shell ownership

The shell owns behaviour that must be identical regardless of which panel is active:

- top-level window and title treatment;
- category navigation;
- back/forward history;
- global search and deep-link routing;
- module discovery and compatibility checks;
- common empty/loading/error states;
- theme selection and Common integration;
- accessibility plumbing that genuinely belongs at shell level;
- consistent presentation of authentication-required actions;
- diagnostics for failed or incompatible modules.

The shell does not own display configuration, Wi-Fi policy, account creation, printer discovery or other domain behaviour merely because those settings appear inside its window.

## Module ownership

A module owns one bounded settings domain. It provides:

- stable identity and version metadata;
- category and ordering metadata;
- searchable keywords and setting-level search entries;
- a lazily created presentation surface;
- deep-link targets;
- read/current-state behaviour;
- write/apply behaviour;
- explicit capability and availability state;
- any domain-specific asynchronous workers or subscriptions;
- cleanup for every resource it owns.

Modules should remain small enough that a defect in one domain does not require reasoning about unrelated domains.

First-party modules are loaded only from trusted, system-owned locations. User-writable arbitrary in-process modules are not part of the initial trust model.

## Privilege model

System Settings does **not** start as root and does not ask for administrative credentials merely because it has been opened.

Read-only settings are displayed using the current user's normal access. User-owned configuration is changed without elevation. System-wide changes use the narrowest authoritative system interface available and request authorisation only when the user performs the protected action.

For example:

```text
open System Settings                 → no authentication
change desktop theme                 → no authentication
change profile picture               → normally no authentication
create or remove a local account     → authorise that operation
change a protected system service    → authorise that operation
```

The preferred boundary is an existing system service with policy-controlled authorisation such as D-Bus/Polkit. A project-owned privileged helper is not introduced merely for convenience.

See [SECURITY.md](SECURITY.md).

## Common

Common is the authoritative home for generic mechanisms shared across Infiltrator projects when its implementation is at least as strong as the best local implementation.

System Settings is expected to consume Common for presentation primitives such as the project theme, typography, standard controls, spacing, status surfaces, durable generic preferences and other reusable infrastructure.

Common must not become a dumping ground for System Settings policy. A function for rendering a standard toggle may belong in Common; code deciding how NetworkManager Wi-Fi configuration is written does not.

When implementation begins, Common will be consumed through an exact pinned revision so a settings release is reproducible.

## Specialised application ownership

System Settings is the configuration front door, not a reason to absorb every existing system application.

Where another Infiltrator application already owns a complete domain, Settings exposes a clear launch/deep-link entry rather than duplicating the implementation:

- **Software** remains the owner of software discovery, installation/removal, repositories, system updates, history and package repair. Settings may expose Software or Updates entries, but the operation belongs to Software.
- **System Monitor** remains the owner of live process/performance/hardware/service monitoring. Settings may link to it from system-information or diagnostic contexts without copying its monitoring engine.
- **Defragmenter** remains the owner of filesystem analysis, defragmentation and recovery. A Storage settings module owns actual storage configuration, while defragmentation actions launch/deep-link to Defragmenter.

This rule keeps specialised applications strong and prevents System Settings becoming a second implementation of functionality that already has an authoritative project owner.

Cross-application deep links should become stable project contracts where useful. Until a specialised application supports a specific deep link, Settings may launch its normal entry point rather than reimplementing the feature.

## Native-interface policy

System Settings is a control surface over operating-system and desktop facilities; it is not an attempt to replace every facility beneath it.

Where a stable native API exists, use it directly. Examples may include GSettings for desktop preferences, D-Bus services for system components, kernel interfaces for appropriate low-level state, and existing system services for network, printing, Bluetooth, accounts or authentication.

Command-line utilities are not treated as APIs merely because they are easy to invoke. A utility may be used only where it is genuinely the strongest maintained contract for the operation.

## Performance model

Opening System Settings should construct only the shell and the initial visible content. Modules are discovered from lightweight metadata and panels are created on demand.

Expensive device enumeration, service discovery or network inspection must not block first paint. Long-running or potentially blocking work belongs on bounded workers or asynchronous native APIs. UI objects remain owned by the UI thread.

Search metadata must not require loading every module's full UI.

## Failure model

Settings software modifies persistent system state, so false success is unacceptable.

Operations distinguish at least:

- success;
- unsupported;
- unavailable;
- permission denied / authorisation cancelled;
- invalid input;
- temporary system failure;
- module/backend failure.

A module must refresh authoritative state after a write where practical rather than assuming that a requested value became the effective value.

## Repository policy

Normal development stays on `main`. The project does not use long-lived feature branches as an architectural workflow.

Documentation is part of the implementation contract. A feature is not complete when code exists but architecture, behaviour, validation or privilege semantics are undocumented.

Published releases, once introduced, will be produced only from an exact verified `main` revision and their tags/assets will be treated as immutable.

## Documentation map

The maintained documents are indexed in [docs/README.md](docs/README.md).

## Licence

Copyright © 2000-2026 Shannon Smith.

Shannon Smith-owned source, documentation and project artwork are licensed under GPL-3.0-or-later.
