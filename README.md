<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Settings

**Project copyright:** © 2000-2026 Shannon Smith

System Settings is a native cross-platform settings environment for Linux Mint/Cinnamon and Windows. Its user interface is deliberately unified, but its implementation is deliberately modular: one searchable Settings shell presents many small, independently owned control panels.

The project takes the strongest part of the classic Amiga/Control Panel model — small focused preference tools with clear ownership — and combines it with the strongest part of a modern settings application — one consistent window, global search, deep links, shared navigation and predictable privilege handling.

**Status:** Phase 1 in progress: Linux GTK4 Date & Time panel, portable model, and Linux/Windows policy persistence. Windows currently builds the policy CLI and tests, not a settings GUI. Dynamic modules, global search and deep links remain planned.
**Primary targets:** Linux Mint/Cinnamon and Windows desktop  
**Implementation:** native C/C++, using the strongest style for each component  
**Shared foundation:** pinned Infiltratr Common 1.19.25  
**Licence:** GPL-3.0-or-later

## Core idea

System Settings is **one product, not one monolith**.

The planned product presents one application:

```text
System Settings
├── Appearance
├── Hardware
├── Network
├── System
└── Security
```

The intended module structure is (only Date & Time is currently built in):

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

## Current implementation

The first implementation slice is Date & Time. The repository now contains the shared semantic Date & Time model, Linux and Windows per-user temporal-policy stores, module metadata, tests, Linux/Windows CI, and the first native System Settings shell. On Linux the GTK4 shell presents a real Date & Time panel with a live clock/date preview, the clock and calendar catalogues from the exact pinned Common revision, seconds policy, geographic location and current time-zone display. Changes are saved immediately through the same model used by non-UI consumers.

The current temporal policy is version 3 in Common 1.19.25. System Settings is the sole Infiltrator authority for the richer clock system, calendar system, seconds and geographic-location policy; Calendar does not maintain competing local choices. The policy is deliberately optional for consumers: the installed Linux package publishes a versioned `temporal-v3` provider capability marker, but consumers still remain on their native platform defaults until a valid Infiltrator policy has actually been saved. On Mint/Cinnamon this means the Calendar replacement continues to follow Cinnamon/locale temporal preferences until System Settings publishes an extended policy.

## User experience

The current window has a Date & Time page and sidebar. The planned multi-module category model is:

- **Appearance** — theme, fonts, desktop and visual behaviour;
- **Hardware** — displays, sound, keyboard, mouse/touchpad, Bluetooth and printers;
- **Network** — Wi-Fi, Ethernet, VPN, proxy and related connectivity;
- **System** — users, date/time, storage, startup, power, system information and entry points to specialised system applications such as Software;
- **Security** — authentication, firewall/security surfaces and permission-related configuration.

These categories are navigation aids, not code boundaries. A module may expose settings under more than one searchable term while retaining one clear implementation owner.

### Search

Global search is a planned interface; it is not implemented in the current shell.

A module can publish searchable entries down to individual settings:

```text
"lid"  → Power → Close laptop lid
"dns"  → Network → Wi-Fi → DNS
"dns"  → Network → Ethernet → DNS
"font" → Appearance → Fonts
```

Planned search results will deep-link directly to the relevant panel and setting. Search metadata must be available without constructing every panel, so startup remains fast.

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
                   PLATFORM AUTHORITY LAYER
          Mint/Cinnamon adapters / Windows adapters /
       native APIs / services / documented settings handoffs

                         INFILTRATOR COMMON
                theme / typography / shared widgets /
              generic persistence / common infrastructure
```

See [Architecture](docs/ARCHITECTURE.md), [Design](docs/DESIGN.md), [Module contract](docs/MODULES.md), [System-wide Presentation Policy](docs/PRESENTATION_POLICY.md), [Platform Compatibility](docs/PLATFORM_COMPATIBILITY.md), [Mint/Cinnamon Compatibility](docs/MINT_COMPATIBILITY.md), [Windows Compatibility](docs/WINDOWS_COMPATIBILITY.md), [Decisions](docs/DECISIONS.md), [Roadmap](docs/ROADMAP.md) and [Validation](docs/VALIDATION.md).

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

System Settings starts with ordinary user authority on every platform and does not request elevation or administrative credentials merely because it has been opened.

Read-only settings are displayed using the current user's normal access. User-owned configuration is changed without elevation. System-wide changes use the narrowest authoritative system interface available and request authorisation only when the user performs the protected action.

For example:

```text
open System Settings                 → no authentication
change desktop theme                 → no authentication
change profile picture               → normally no authentication
create or remove a local account     → authorise that operation
change a protected system service    → authorise that operation
```

The preferred boundary is the narrowest documented platform authority, such as D-Bus/Polkit on Linux or a documented Windows API/UAC/security boundary on Windows. A project-owned privileged helper is not introduced merely for convenience.

See [SECURITY.md](SECURITY.md).

## Common

Common is the authoritative home for generic mechanisms shared across Infiltrator projects when its implementation is at least as strong as the best local implementation.

System Settings is expected to consume Common for presentation primitives such as the project theme, typography, standard controls, spacing, status surfaces, durable generic preferences and other reusable infrastructure.

Common must not become a dumping ground for System Settings policy. A function for rendering a standard toggle may belong in Common; code deciding how NetworkManager Wi-Fi configuration is written does not.

Common is consumed through an exact pinned release/commit so each settings release is reproducible. System Settings also uses Common's canonical project identity/build-profile vocabulary, locale-independent numeric parsing, deterministic ASCII matching and semantic design palette rather than maintaining parallel generic implementations.

## System-wide presentation policy

System Settings also owns user-wide presentation preferences whose value comes from being consistent across applications. The first defined family is temporal presentation: applications keep canonical timestamps, while Common renders human-visible dates/times according to the system policy selected here.

This allows filesystem timestamps, histories and other UI timestamps to follow the selected clock mode, calendar, seconds policy and location-aware historical/astronomical presentation without rewriting the underlying data.

See [System-wide Presentation Policy](docs/PRESENTATION_POLICY.md).

## Specialised application ownership

System Settings is the configuration front door, not a reason to absorb every existing system application.

Where another Infiltrator application already owns a complete domain, Settings exposes a clear launch/deep-link entry rather than duplicating the implementation:

- **Software** remains the owner of software discovery, installation/removal, repositories, system updates, history and package repair. Settings may expose Software or Updates entries, but the operation belongs to Software.
- **System Monitor** remains the owner of live process/performance/hardware/service monitoring. Settings may link to it from system-information or diagnostic contexts without copying its monitoring engine.
- **Defragmenter** remains the owner of filesystem analysis, defragmentation and recovery. A Storage settings module owns actual storage configuration, while defragmentation actions launch/deep-link to Defragmenter.

This rule keeps specialised applications strong and prevents System Settings becoming a second implementation of functionality that already has an authoritative project owner.

Cross-application deep links should become stable project contracts where useful. Until a specialised application supports a specific deep link, Settings may launch its normal entry point rather than reimplementing the feature.

## Platform compatibility

Linux Mint/Cinnamon and Windows are first-class targets. Modules express the setting's meaning once and use platform adapters to reach the documented authority on each operating system. A platform may require a native Settings handoff when it exposes no safe writable API.

The rule is: **reuse the platform authority when it already represents the setting; extend only what is missing; do not use undocumented internals merely to make two operating systems look identical.**

See [Platform Compatibility](docs/PLATFORM_COMPATIBILITY.md), [Mint/Cinnamon Compatibility](docs/MINT_COMPATIBILITY.md) and [Windows Compatibility](docs/WINDOWS_COMPATIBILITY.md).

## Mint/Cinnamon compatibility

The Date & Time vertical slice now acts as a real replacement frontend on current systemd-based Mint: it can change the operating-system time zone, network-time state and manual system clock through timedated; edit Cinnamon's native format preferences; and resolve named localities into coordinates for richer Common-aware temporal presentation.

System Settings is intended to be a compatible superset of the Mint/Cinnamon settings environment. When Mint/Cinnamon already has an authoritative setting for the same concept, System Settings should use that setting directly rather than create a duplicate. New Infiltrator policy is introduced only for concepts the existing desktop cannot represent faithfully.

The rule is: **reuse what is already correct, extend what is missing, replace only when necessary.**

See [Mint/Cinnamon Compatibility](docs/MINT_COMPATIBILITY.md).

## Native-interface policy

System Settings is a control surface over operating-system and desktop facilities; it is not an attempt to replace every facility beneath it.

Where a stable native API exists, use it directly. Linux examples include GSettings, D-Bus and kernel/system-service interfaces. Windows examples include documented Win32/WinRT/globalisation/subsystem APIs and supported Settings-page handoffs. Platform-private storage is not treated as an API merely because it can be discovered.

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

Published releases are produced only from an exact verified `main` revision and their tags/assets will be treated as immutable.

## Build and validation

On Debian/Ubuntu/Mint, install `cmake`, a C11 compiler, `pkg-config`,
`libgtk-4-dev`, `libgeocode-glib-dev`, `xvfb` and `dbus-x11`.
The Linux build requires GTK >= 4.6 and geocode-glib **2.0**; the 1.0 pkg-config
interface is not an alternative. Initialise the exact Common submodule first:

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel "$(($(nproc)>1?$(nproc)-1:1))"
ctest --test-dir build --output-on-failure
```

The shell test needs Xvfb and a working local socket facility; the timedated
fixture creates a private D-Bus and never changes the host clock. On Windows,
configure with CMake/Visual Studio and use `--config Release` for the build and
`-C Release` for CTest. Windows currently provides `system-settings-time`, the
portable tests and the Windows persistence test.

`system-settings-time` without options prints the policy. Its supported options
are `--clock MODE`, `--calendar ID`, `--seconds on|off`, `--location LAT LON` and
`--clear-location`. One invocation validates all options before one persistent
write; malformed later options leave the existing file unchanged. This CLI
edits presentation preferences, not the protected operating-system clock.

See [the September 2026 audit](docs/AUDIT-2026-09-25.md) for findings, coverage,
validation evidence and remaining product gaps.

## Documentation map

The maintained documents are indexed in [docs/README.md](docs/README.md).

## Licence

Copyright © 2000-2026 Shannon Smith.

Shannon Smith-owned source, documentation and project artwork are licensed under GPL-3.0-or-later.

## Release assets

Each release publishes the Debian package, a `System-Settings-VERSION-native.run`
source installer, a complete source ZIP (including pinned Common), and SHA256SUMS.
The native installer builds and tests as the ordinary user with CPU-specific
optimisation, then asks sudo only to install the generated Debian package through
APT. Its `--help` and `--extract DIRECTORY` modes need no elevation. Build
prerequisites are the same as above; it does not install dependencies silently.
