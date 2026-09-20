<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Windows Compatibility

Windows is a first-class System Settings target, not a later port of the Linux implementation.

This document applies PLATFORM_COMPATIBILITY.md to Windows.

## Windows authority model

Windows does not expose one universal settings API. Different domains are owned by different documented interfaces.

System Settings therefore uses documented Win32/WinRT APIs, National Language Support/globalisation APIs, subsystem APIs for things such as power/network/devices/printing/accounts, documented security or privilege mechanisms for protected changes, and the ms-settings URI scheme when Windows exposes a Settings page rather than an appropriate public write API.

Raw registry editing is not a general Windows settings API.

## General rule

For each setting:

1. use the documented Windows authority when one exists;
2. use documented user overrides when Microsoft explicitly provides them;
3. open the native Windows Settings destination when that is the supported path;
4. create an Infiltrator extension only when Windows cannot represent the concept faithfully;
5. do not reverse-engineer private Settings-app storage just to force parity.

## Regional and temporal presentation

Windows already has user-wide regional-format preferences consumed by Windows and applications.

The Windows adapter should use documented National Language Support/globalisation APIs to read those preferences. Where System Settings is intentionally acting as the user's regional-settings utility, documented user-override APIs may be used for conventional settings and the required setting-change notification must be sent.

Conventional 12-hour and 24-hour presentation can map to Windows conventions. Decimal 10-hour time cannot be represented faithfully by the ordinary Windows clock model, so its real identity remains an Infiltrator Temporal policy consumed by Common-aware applications.

Ordinary Windows applications can continue using a conventional fallback when an extended Infiltrator profile is selected.

## Calendars

Windows exposes multiple documented calendar systems through its globalisation APIs. System Settings should reuse those capabilities when they match the requested semantic calendar.

A historically reconstructed or project-defined calendar remains an Infiltrator extension unless Windows provides an exact equivalent.

Calendar choice and clock choice remain independent dimensions.

## Time zone

Windows time-zone changes use documented time-zone APIs and may require a specific operating-system privilege.

System Settings uses that narrow authority only for the operation. Temporal presentation policy remains separate from machine time-zone configuration.

## Native Settings handoff

Windows publishes ms-settings destinations for many Settings pages, including Date & time, Region, Display, Bluetooth, Network and application settings.

Where Windows exposes no safe writable API, opening the exact native Settings page is the supported integration path. This is preferable to writing private registry state.

## Registry policy

System Settings does not modify arbitrary registry locations discovered by reverse engineering.

Registry access is acceptable only when Microsoft documents that registry contract as the supported interface, or when a documented API owns the setting and System Settings uses the API rather than its storage detail.

## Policy-managed machines

A Windows adapter must distinguish writable, read-only-by-policy, denied-by-policy, requires elevation/privilege, requires native Settings handoff, unavailable on this Windows version/SKU, and Infiltrator-extension-only states.

Policy restrictions are not bypassed by writing lower-level storage directly.

## DLL loading

Windows modules load only from trusted System Settings installation locations using the documented Windows loader and safe dependency-search behaviour. The current working directory and arbitrary PATH entries are not trusted module locations.

The exported C module ABI remains the same semantic contract used on Linux.

## UI model

Windows and Linux use the same categories, search targets, deep-link identities, semantic module state and Common design language where capability exists.

Platform-native windowing, input and accessibility machinery may differ beneath Common, but Windows-specific handles or UI objects do not enter the public module ABI.

## First Windows milestone

Before Windows is described as supported, Phase 1 proves shell startup/first paint, manifest discovery, trusted DLL loading, one module through the shared ABI, Common theme/typography, search/deep links, non-elevated startup, one conventional regional/temporal read path, a Windows Settings handoff, and install/uninstall ownership.
