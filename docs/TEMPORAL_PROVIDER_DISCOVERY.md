<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Temporal resource discovery

## Status

This is a target architecture for Date & Time capability discovery. It does not claim that the current System Settings implementation has already migrated away from Common's compiled temporal catalogue.

The canonical design is owned by Common in `Libraries/docs/TEMPORAL_PROVIDERS.md`. This document defines only the System Settings side of that contract.

## The directory is the catalogue

System Settings does not own a list of calendars or clocks.

The installed temporal resource directories are the capability authority. Common discovers what is present and exposes a runtime registry. System Settings simply enumerates that registry.

Conceptually:

```text
Temporal/Calendars/*
Temporal/Clocks/*
        ↓
Common discovers installed resources
        ↓
runtime calendar/clock registry
        ↓
Date & Time selectors
```

Drop a valid calendar resource into the calendar directory and it becomes discoverable. Drop a valid clock resource into the clock directory and it becomes discoverable. Remove the resource and the capability disappears.

No System Settings source edit or master-list update is part of this operation.

## Resource format is not System Settings' concern

System Settings must not care whether a temporal resource is compiled object/module code, a shared library, data interpreted by Common, a directory bundle or some future format chosen after prototyping.

The UI consumes only the stable identity, display metadata, capability information and availability state exposed by Common.

The defining contract is discovery-by-presence in the recognised directory, not a particular implementation language or binary format.

## Calendar and clock remain independent

Calendar and clock are independently selectable resources.

For example, the first Egyptian work may install:

- `egyptian-early-lunar` — Early Egyptian Lunar Calendar;
- `egyptian-stellar-solar` — Early Egyptian Stellar–Solar Time.

They may be distributed together, but System Settings still sees two independently selectable capabilities.

## Persistence and missing resources

System Settings persists stable resource IDs, not array positions.

If a selected resource is later removed, the saved selection becomes explicitly unavailable. System Settings does not silently substitute another calendar or clock. If the same stable resource is installed again, the original selection can become active again without rewriting canonical timestamps.

## Trust boundary

System Settings is not the authority for parsing/loading whatever physical resource format is eventually chosen.

Common owns resource discovery, validation and any implementation-specific loading/trust rules. System Settings consumes only the resulting registry and diagnostics. If resources ultimately contain native executable code, Common must enforce the corresponding compatibility/trust policy; if they are data-driven, Common validates them as external data.

## Current implementation migration

The current calls to `infiltratr_temporal_calendar_count()`, `infiltratr_temporal_calendar_at()`, `infiltratr_temporal_clock_mode_count()` and `infiltratr_temporal_clock_mode_at()` remain valid until Common changes their backing implementation to the filesystem-discovered resource registry or introduces versioned successors.

The System Settings UI should not be rewritten around temporary hard-coded Egyptian entries. The correct direction is to change what sits underneath enumeration so new installed resources appear automatically.

## Acceptance

This design goal is satisfied when:

- System Settings has no independent compiled catalogue of temporal capabilities;
- selectors enumerate Common's registry derived from the temporal resource directories;
- installing a valid resource makes its capability appear without a System Settings code change;
- removing a resource produces an explicit unavailable state for persisted selections;
- malformed/incompatible resources cannot break unrelated Date & Time functionality;
- provider-specific historical rules and documentation remain outside System Settings;
- the physical resource representation can evolve without changing this user-facing discovery contract.
