<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Module Contract

This document owns the first-party System Settings module model.

The contract is intentionally small. A module must be able to describe itself without executing code, and the shell must be able to load the implementation lazily through a versioned ABI.

## Terminology

**Manifest** — static metadata read without loading module code.

**Module library** — first-party native shared library implementing one settings domain.

**Module instance** — runtime module state after successful activation.

**Panel** — the module's user-facing surface managed by the shell.

**Deep link** — stable logical target identifying a panel or individual setting.

**Backend** — authoritative native interface used by the module to read or change system state.

## Identity

Each module has a stable reverse-DNS-style or project-style identifier. The final naming convention should be short and durable, for example:

```text
org.infiltrator.settings.display
org.infiltrator.settings.network
org.infiltrator.settings.users
```

The identifier is not a display string and is not translated.

Renaming a UI panel must not change module identity.

## Manifest

A conceptual manifest contains:

```ini
[Module]
Id=org.infiltrator.settings.display
ApiVersion=1
Library=display
Category=hardware
Order=20
Title=Displays
Summary=Resolution, scaling, refresh rate and monitor arrangement
Icon=display

[Search]
Keywords=monitor;screen;resolution;refresh;scale;scaling;orientation;hdr

[Target "resolution"]
Title=Resolution
Keywords=mode;size;pixels

[Target "refresh-rate"]
Title=Refresh rate
Keywords=hz;frequency
```

The final syntax uses a project-owned portable parser/format rather than depending on one platform's configuration library, and it must remain:

- human-readable;
- deterministic;
- bounded;
- versioned;
- parseable without executing module code.

The manifest must not contain executable shell commands.

## Search metadata

Search entries are static enough to index before the library is loaded.

A target records:

- stable target ID;
- display title;
- optional summary;
- synonyms/keywords;
- optional category override only when genuinely needed.

Dynamic state such as connected device names is not required for the initial global index. A loaded module may contribute dynamic secondary search results later through a separate bounded interface.

## ABI design

The module ABI is a C ABI.

A conceptual shape is:

```c
typedef struct SsHostApi SsHostApi;
typedef struct SsModule SsModule;

typedef struct SsModuleApiV1 {
    uint32_t struct_size;
    uint32_t api_version;

    SsModule *(*create)(const SsHostApi *host);
    void (*destroy)(SsModule *module);

    bool (*activate)(SsModule *module, const char *target_id);
    bool (*can_close)(SsModule *module);
    void (*request_close)(SsModule *module);

    /* UI/panel and state hooks are defined by the concrete v1 contract. */
} SsModuleApiV1;

extern const SsModuleApiV1 *
system_settings_module_get_api(uint32_t host_api_version);
```

This is illustrative until implementation commits the header. The important invariants are already decided:

- one exported well-known entry point;
- explicit API version;
- structure size for compatible extension;
- opaque module state;
- no C++ types or STL containers across the ABI;
- ownership and lifetime documented for every pointer;
- host callbacks grouped in a versioned host API;
- no direct access to shell internals.

## Versioning

A module declares the API it requires in both manifest and runtime ABI.

The shell validates the manifest before invoking the platform-native dynamic loader and validates the returned runtime table before calling any function.

Rules:

- incompatible major API → module remains unloaded and is shown diagnostically as incompatible;
- optional tail fields may be added when structure-size checks make that safe;
- a module never guesses missing callbacks;
- ABI version negotiation must be deterministic;
- the shell never loads a library solely to ask what manifest it has.

## Loading

Module load sequence:

```text
discover manifest
      ↓
parse + validate bounded metadata
      ↓
add navigation/search entries
      ↓ user opens target
resolve trusted platform library path
      ↓
load shared object or DLL through native loader
      ↓
resolve one entry point
      ↓
negotiate ABI
      ↓
create module instance
      ↓
create/attach panel lazily
      ↓
activate requested deep link
```

Failure at any stage becomes a shell-owned module error state. It must not crash the entire settings catalogue.

## Library path trust

A manifest cannot name an arbitrary absolute path supplied from a user-writable location.

The loader resolves a constrained library basename inside the trusted System Settings module directory. Path traversal and symlink policy are validated according to the packaging/security design.

Initial module locations must be writable only by trusted system administration/package installation.

## Host services

The host API may provide generic services such as:

- logging/diagnostics;
- shell navigation requests;
- Common theme/presentation access;
- standard confirmation surface;
- standard status/error reporting;
- async completion dispatch to the UI thread;
- authorisation launch/request abstraction when a platform service requires it.

The host API must not become a grab bag for domain backends. NetworkManager belongs to the network module, not the shell API.

## UI ownership

The v1 module ABI is platform-neutral. GTK widgets, HWND values, WinUI objects and other toolkit/native presentation types do **not** cross the public module boundary.

The exact Common/System Settings presentation abstraction is finalised in Phase 1, but the ownership rule is already fixed:

- the shell owns the container/window;
- the module owns widgets/state it creates;
- the module may not destroy host-owned containers;
- the shell destroys/detaches a panel only through the documented lifecycle;
- UI objects are accessed only from the UI thread.

Platform-specific presentation implementations may exist behind the host abstraction, but making a toolkit-native pointer part of ABI v1 is prohibited because it would make the module contract platform-specific.

## Lifecycle

A module must tolerate:

1. create;
2. activate a target;
3. remain active while authoritative state changes;
4. deactivate or become hidden;
5. reactivate, possibly with another target;
6. destroy.

A panel may be destroyed and recreated within one application session if the shell adopts eviction. Durable state must not live solely in transient widgets.

## Deep-link activation

`activate(target_id)` has two responsibilities:

- ensure the relevant subsection is presented;
- move focus/attention to the logical setting without relying on a brittle widget index.

A target that no longer exists should return an explicit not-found result so old links degrade to the panel rather than causing undefined behaviour.

## Dirty state

Modules with staged edits expose dirty state to the shell.

Before navigation/destruction, the module can report one of:

- clean;
- dirty but safely discardable after confirmation;
- operation in progress and temporarily non-closable;
- fatal/error state that is still closable.

The shell owns consistent confirmation presentation; the module owns the explanation and whether candidate data can be discarded.

## Read/write contract

A writable setting should normally follow:

```text
read authoritative value
        ↓
present/edit
        ↓
validate candidate
        ↓
request write/transaction
        ↓
observe completion
        ↓
read back or receive authoritative change signal
        ↓
publish effective state
```

A successful request that cannot be confirmed must not be presented as a confidently verified new value.

## Authorisation

Modules do not ask the shell to "become root".

A protected operation uses the native authorised backend. The module supplies domain intent; the platform authorisation agent owns credentials.

The module must handle:

- authorised success;
- denied;
- cancelled;
- policy unavailable;
- backend failure after authorisation.

Cancellation is not logged as a security failure unless the platform reports one.

## Concurrency

Each module documents:

- which calls must occur on the UI thread;
- which callbacks may arrive from worker/service threads;
- how cancellation works;
- how destruction waits for or invalidates outstanding operations.

No callback may access a module after destruction. Generation IDs, cancellables or reference-counted request contexts are acceptable mechanisms depending on the implementation.

## Failure isolation

A normal backend failure is contained inside the module panel.

Because first-party libraries are in-process on both Linux and Windows, memory corruption or a hard crash cannot be sandboxed by the loader. CI, sanitizers and narrow interfaces are therefore part of the module trust model.

If future third-party modules are required, use an explicitly designed process boundary rather than pretending in-process dynamic loading provides isolation.

## Admission criteria for a new first-party module

A new module is admitted when:

- its domain and owner are clear;
- it does not duplicate an existing module without a stronger reason;
- authoritative backend interfaces are identified;
- read and write semantics are understood;
- privilege requirements are explicit;
- search/deep-link targets are defined;
- failure and unavailable states are representable;
- tests can prove its important contracts;
- its implementation does not require the shell to know its domain internals.

## Planned initial modules

The initial architecture anticipates, without claiming implementation:

- Appearance;
- Displays;
- Sound;
- Keyboard;
- Mouse/Touchpad;
- Network;
- Bluetooth;
- Printers;
- Power;
- Users;
- Date & Time;
- Storage;
- Startup;
- Security;
- System Information.

The list is a roadmap input, not a requirement to create empty placeholder libraries.
