<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Platform Compatibility

System Settings is one product with platform-specific authority adapters. Linux Mint/Cinnamon and Windows are first-class targets; neither is treated as a later port of the other.

The rule is simple: keep settings semantics platform-neutral, reuse the operating system's documented authority when it already owns the setting, add an Infiltrator policy only when the platform cannot represent the concept faithfully, and hand off to the native Settings UI when the platform exposes no safe writable API.

## Layers

System Settings shell
→ semantic settings module
→ platform capability/authority interface
→ Mint/Cinnamon adapter or Windows adapter
→ documented native authority

The semantic module knows what the setting means. The adapter knows how that meaning is represented, whether it is readable or writable, how changes are observed and what privilege is required.

The shell must not contain platform details such as GSettings schema names, D-Bus service details, Windows registry paths, HWND values or WinRT objects.

## Authority classes

Every setting is one of these:

1. Native platform authority — an existing documented desktop/OS API owns the value.
2. Infiltrator policy — System Settings owns a new cross-application policy the platform cannot express faithfully.
3. Specialised application authority — another Infiltrator application owns the complete domain and Settings launches or deep-links to it.
4. Native Settings handoff — the platform exposes a supported Settings page but no appropriate public write API.
5. Unavailable/unresolved — System Settings does not write until the correct authority is known.

There must not be two independent writable values for the same semantic setting.

## Capabilities

Platforms do not expose identical knobs. A module therefore queries capabilities such as readable, writable, requires authorisation/elevation, observable for external changes, native-handoff-only, unsupported, or Infiltrator-extension-only.

The UI may remain visually aligned while honestly showing platform differences. Missing capability is not a reason to use undocumented internals.

## UI and module ABI

The module ABI must remain platform-neutral. GTK widgets, HWND values, WinUI objects and similar native UI types do not cross the public module boundary.

The exact Common/System Settings presentation abstraction is finalised before ABI v1 is frozen. Platform-specific UI machinery can exist beneath that abstraction.

## Module loading

Manifests identify logical modules, not Unix-only .so files. The loader resolves the trusted native implementation for the current platform.

Linux can load a shared object through the native dynamic loader. Windows can load a DLL through the documented Windows loader. Neither platform trusts the current working directory or arbitrary user-writable search paths.

## Privilege

The shell starts with ordinary user authority on every platform. Protected operations use the narrowest documented native boundary for that operation. The whole application is not kept elevated because one setting requires higher privilege.

## Platform documents

Mint-specific mapping lives in MINT_COMPATIBILITY.md.

Windows-specific mapping lives in WINDOWS_COMPATIBILITY.md.

These documents implement this shared contract rather than redefining it.

## Validation

Cross-platform completion requires one semantic contract, an identified authority or handoff on each supported platform, per-platform read/write/observe tests, explicit capability differences, and proof that the public module ABI remains platform-neutral.
