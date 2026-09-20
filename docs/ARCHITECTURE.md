<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

System Settings uses a unified cross-platform shell over independently owned semantic settings modules. Linux Mint/Cinnamon and Windows are first-class targets. The architecture is intentionally neither a single monolith nor a loose folder of unrelated preference executables.

The shell provides one discoverable product. Modules preserve small ownership boundaries.

## Architectural shape

```text
+--------------------------------------------------------------+
|                     System Settings shell                    |
| window / navigation / search / history / theme / diagnostics |
+-------------------------------+------------------------------+
                                |
                     manifest + stable module ABI
                                |
       +------------------------+-------------------------+
       |                        |                         |
       v                        v                         v
+-------------+          +-------------+           +-------------+
| Display     |          | Network     |           | Users       |
| module      |          | module      |           | module      |
+------+------+          +------+------+           +------+------+
       |                        |                         |
       v                        v                         v
        semantic domain contracts / platform capabilities
       \_______________________|_________________________/
                               |
                    platform authority adapters
                     /                      \
        Mint/Cinnamon native APIs      Windows native APIs
        and services                    and settings handoffs

+--------------------------------------------------------------+
| Infiltrator Common                                          |
| theme / typography / reusable widgets / generic utilities   |
+--------------------------------------------------------------+
```

The arrows express ownership, not a mandatory call stack. A module may subscribe to a D-Bus signal, call a library API, read a kernel interface or use another native contract appropriate to its domain.

## Layer responsibilities

### Shell

The shell owns behaviour that must remain consistent for every module:

- application lifecycle and primary window;
- category navigation and panel routing;
- back/forward history;
- global search;
- deep-link parsing and dispatch;
- manifest discovery and compatibility validation;
- lazy module loading;
- Common theme and typography integration;
- standard loading, empty, unavailable and error surfaces;
- common confirmation/authentication presentation;
- diagnostics when a module cannot load;
- restoration of safe navigation state between launches.

The shell must not absorb domain policy merely because a setting is visible in its window.

### Module

A module owns one bounded settings domain and all knowledge required to expose that domain safely. It owns:

- native backend selection for that domain;
- current-state retrieval;
- validation;
- change/apply behaviour;
- capability detection;
- domain-specific asynchronous work and subscriptions;
- deep-link target resolution;
- setting-specific confirmation and rollback semantics;
- resource cleanup.

A display module knows display semantics. A network module knows network semantics. Neither belongs in the shell.

### Backend/native interface

Backends are platform adapters providing the narrowest reliable bridge from a semantic module to the system being configured. Linux examples may include:

- GSettings for desktop/session settings where it is authoritative;
- D-Bus services for system components;
- NetworkManager interfaces for network configuration;
- BlueZ interfaces for Bluetooth;
- CUPS interfaces for printing;
- systemd interfaces for service-related configuration;
- AccountsService or another authoritative account interface where appropriate;
- sysfs/procfs/ioctls where a kernel contract is genuinely the correct settings interface.

Windows sibling backends use documented Win32/WinRT/globalisation/subsystem APIs or explicit Windows Settings handoffs as appropriate. The exact backend is chosen per feature after verifying the maintained platform contract. Command-line utility output, undocumented registry storage and private desktop settings are not used as APIs merely for convenience.

See [PLATFORM_COMPATIBILITY.md](PLATFORM_COMPATIBILITY.md).

## Module discovery

The shell discovers modules from lightweight manifests. It does not load every shared library at startup.

Conceptually:

```text
/usr/share/infiltrator/system-settings/modules/
    display.settings-module
    network.settings-module
    users.settings-module

/usr/lib/infiltrator/system-settings/modules/
    display.so
    network.so
    users.so
```

The final installation paths may follow packaging conventions, but the separation is durable:

1. manifest discovery is cheap;
2. manifest parsing produces the navigation/search catalogue;
3. module code is loaded only when required;
4. module ABI compatibility is checked before activation.

This allows the shell to search individual settings without constructing every panel.

## ABI boundary

The dynamic module boundary uses a small versioned C ABI even when shell or module internals are implemented in C++.

Reasons:

- C linkage avoids dependence on a particular C++ ABI;
- opaque handles keep implementation details private;
- explicit structure-size and API-version fields allow controlled extension;
- C and C++ modules remain equally possible;
- a module can use C++ internally and expose only `extern "C"` entry points.

The ABI is defined in [MODULES.md](MODULES.md). It must remain smaller than the internal module implementation.

## First-party trust model

Initial modules are first-party and installed into system-owned, non-user-writable directories. Loading arbitrary per-user shared objects is deliberately outside the initial design.

An in-process module executes with the user's process authority and can therefore compromise the shell if it is malicious or memory-unsafe. The module loader must not pretend a shared-library boundary is a security sandbox.

A future third-party extension model may use out-of-process providers if there is a real requirement. That is a separate design problem and must not weaken the first-party settings product.

## Privilege architecture

The shell starts unprivileged. It remains unprivileged for its lifetime unless a future native platform contract proves a different model is strictly required.

Protected changes are performed through a narrow authorised interface. Preferred order:

1. existing system service with policy-based authorisation;
2. existing privileged platform API with explicit per-operation permission;
3. project-owned privileged component only if neither of the above can provide the required correct behaviour.

A project-owned helper, if ever admitted, must expose a narrow typed operation surface. It must not accept arbitrary commands, paths or shell fragments and must not inherit the GUI's broad state.

The application does not collect, store or proxy administrator passwords itself.

## Data and state flow

A module should separate four states where the backend requires it:

```text
authoritative state
      ↓
presented state
      ↓ user edit
candidate state
      ↓ validation/apply
requested state
      ↓ backend confirmation/read-back
new authoritative state
```

For simple user preferences, these stages can collapse into an immediate write followed by read-back. For multi-field or connectivity-sensitive changes, candidate state may remain staged until Apply.

A requested value is not assumed to be effective merely because the write call returned success.

## Concurrency

Platform UI objects remain on their owning UI thread. Toolkit/native UI types do not cross the public module ABI.

Potentially blocking work — service enumeration, device discovery, filesystem scanning, NSS/account operations, slow D-Bus methods or equivalent — must use asynchronous native APIs or bounded workers.

Workers exchange plain data or immutable requests with the UI. They do not retain arbitrary pointers into destroyed panels.

Each module must define shutdown ordering for outstanding requests, subscriptions, timers and workers. Closing a panel must not permit a late callback to use freed UI state.

## Startup

First paint is a product requirement.

Startup performs only work needed to show the shell and initial navigation state:

- initialise Common/UI runtime;
- enumerate and validate small manifest files;
- build category and search indexes;
- restore safe shell preferences;
- show the window.

Loading display topology, Wi-Fi networks, printers, Bluetooth devices, users or storage inventories is deferred until the relevant module is opened or until a lightweight background index explicitly requires it.

No module is allowed to make shell startup depend on an unrelated service being healthy.

## Navigation and panel lifetime

The shell may cache recently used panel instances for responsiveness, but a module cannot rely on permanent lifetime. The contract must support:

- create;
- activate;
- optionally suspend;
- destroy.

Module state that must survive panel destruction belongs in an explicit module/session model or authoritative backend, not hidden in widget pointers.

The shell can evict inactive panels under memory pressure if their contract reports that it is safe to do so.

## Write semantics

There is no one universal Apply button.

Use the strongest semantics for the setting:

- simple, low-risk, reversible preferences: immediate write + authoritative read-back;
- related multi-field configuration: stage + explicit Apply;
- connectivity-sensitive settings: staged transaction with clear recovery;
- dangerous/destructive operations: explicit confirmation;
- display mode changes capable of making the UI unusable: timed confirmation with automatic rollback where the backend supports it.

A module reports dirty/staged state to the shell so navigation cannot silently discard candidate changes.

## Error model

Module/backend results distinguish at least:

- success;
- unsupported;
- temporarily unavailable;
- permission denied;
- authorisation cancelled;
- invalid input;
- conflict/state changed externally;
- transport/service failure;
- module defect/incompatible ABI.

The shell maps common classes to consistent presentation while preserving domain detail supplied by the module.

## External change handling

Settings can change outside this application. Modules should subscribe to authoritative change notifications where available.

A visible panel should reconcile external change without overwriting an active local edit. If candidate state conflicts with changed authoritative state, the module must surface the conflict instead of silently winning.

## Common

Common owns reusable generic implementation when it is at least as strong as the best local implementation. Examples include:

- theme/palette;
- typography;
- standard controls and cards;
- generic error/status surfaces;
- safe parsing and path helpers;
- generic persistence;
- shared accessibility/presentation helpers.

System Settings retains settings-domain policy. Network configuration logic does not move into Common simply because more than one project may eventually need networking.

The repository consumes an exact Common revision when source implementation begins.

## Cross-application presentation policy

Some System Settings choices are not settings for one backend; they are user-wide presentation policy consumed by many applications.

For these policies, ownership is split deliberately:

```text
System Settings
   edits policy
       ↓
authoritative per-user policy
       ↓
Common
   resolves/formats
       ↓
applications
   present canonical data
```

The application/domain continues to own canonical values. Common owns the reusable presentation mechanism. System Settings owns the user's selected policy.

Temporal presentation is the first defined example. A filesystem or application timestamp remains canonical for storage, sorting, arithmetic and interchange; its human-visible rendering can follow the selected clock/calendar/timezone policy.

Applications must distinguish civil instants from durations and machine/export timestamps so a clock-system change cannot alter protocol timing or forensic data.

The complete contract lives in [PRESENTATION_POLICY.md](PRESENTATION_POLICY.md).

## Platform compatibility layer

Linux Mint/Cinnamon and Windows are first-class targets. The shell and semantic module contracts must not hard-code Cinnamon schema names, Windows registry paths, HWND values or platform service details throughout the UI.

Where an existing Mint/Cinnamon setting is authoritative, the owning module uses a platform compatibility adapter to read and write that existing value. New Infiltrator settings are created only when the desktop cannot represent the required semantic concept.

```text
module
  ↓
Mint/Cinnamon compatibility adapter
  ↓
authoritative GSettings / D-Bus / native desktop interface
```

For extended policies:

```text
module
  ↓
Infiltrator policy
  ↓
Common-aware applications
  └── optional compatible Mint fallback
```

An existing Mint value must never be overloaded with a different meaning to simulate an extension.

The general mapping, fallback, capability and handoff contract is defined in [PLATFORM_COMPATIBILITY.md](PLATFORM_COMPATIBILITY.md). Concrete mappings live in [MINT_COMPATIBILITY.md](MINT_COMPATIBILITY.md) and [WINDOWS_COMPATIBILITY.md](WINDOWS_COMPATIBILITY.md).

## Dependency policy

The target is zero avoidable dependencies, not zero dependencies.

Platform presentation/runtime boundaries may be retained when replacing them would mean rebuilding a major mature subsystem without a stronger result. No Linux-specific toolkit becomes part of the module ABI merely because it is used by the Linux presentation backend. Existing system services remain appropriate dependencies for the system facilities they own.

A package or helper is removed only when a stable native or project-owned contract can replace it without reducing correctness, security, desktop integration, accessibility or maintainability.

## Specialised application boundaries

System Settings owns configuration surfaces. It does not absorb mature project-owned applications merely to place every system-related task inside one process.

Current boundaries include:

- Infiltrator Software owns package/software discovery, installation, removal, repositories, updates, history and package repair;
- System Monitor owns live monitoring, process/service inspection and performance telemetry;
- Defragmenter owns filesystem analysis, defragmentation and recovery.

A settings module may expose context and a launch/deep-link action into one of these applications. It must not copy the specialised application's engine into System Settings.

For example, a Storage module may own mount/configuration settings and capacity-related configuration, but "Defragment filesystem" should transfer to Defragmenter. A System/Software row may transfer to Software's Updates view rather than implementing a second updater.

The shell should treat external project applications as explicit destinations. Their failure to be installed is an availability state, not a reason to hide duplicate fallback implementations inside Settings.

## Packaging boundary

The package owns:

- one shell executable;
- first-party module libraries;
- module manifests/search metadata;
- desktop entry and icons;
- translations when introduced;
- Common resources required by the application.

A module may depend on an operating-system service without bundling that service.

## Release architecture rule

A release may describe a module as supported only when:

1. its manifest and ABI are valid;
2. its read path is verified;
3. its write path is verified for every advertised writable setting;
4. permission/authentication failure is handled;
5. external state changes reconcile safely where applicable;
6. relevant validation evidence exists;
7. documentation agrees with the implementation.
