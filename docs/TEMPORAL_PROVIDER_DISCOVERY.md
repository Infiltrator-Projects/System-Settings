<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Temporal provider discovery

## Status

This is a target architecture for Date & Time capability discovery. It does not claim that the current System Settings implementation has already migrated away from Common's compiled temporal catalogue.

The canonical provider ABI, discovery, trust and migration design is owned by Common in `Libraries/docs/TEMPORAL_PROVIDERS.md`. This document defines only the System Settings side of that contract.

## System Settings responsibility

System Settings is the user-facing selection authority for temporal presentation. It does not own calendar mathematics or historical clock algorithms.

When the runtime temporal registry is available, the Date & Time module must populate its calendar and clock selectors from that registry rather than a hand-written list compiled into System Settings.

Conceptually:

```text
Common runtime temporal registry
        ↓
Date & Time module enumerates available calendars/clocks
        ↓
user selects stable provider IDs
        ↓
System Settings persists presentation policy
```

Adding or removing a conforming provider must not require a System Settings source change merely to update the selector catalogue.

## Independence of calendar and clock choice

Calendar and clock remain independently selectable capabilities. A provider may ship a historically related pair, but System Settings must not force them into one inseparable profile unless a future provider explicitly exposes a separate profile concept with its own semantics.

For example, the first Egyptian provider may supply:

- `egyptian-early-lunar` — Early Egyptian Lunar Calendar;
- `egyptian-stellar-solar` — Early Egyptian Stellar–Solar Time.

Their co-location in one provider package does not prevent either capability from being selected independently.

## Missing provider behaviour

Persisted policy uses stable IDs. If a previously selected provider is no longer available, System Settings must report that selection as unavailable rather than silently replacing it with a different calendar or clock and pretending the user's policy changed.

A fallback may be offered explicitly to the user, but it is not an implicit mutation of historical meaning.

## Trust boundary

System Settings does not directly `dlopen`/`LoadLibrary` arbitrary user-provided temporal code merely because it appears in a discoverable directory.

Common owns provider discovery and trust validation. System Settings consumes the resulting registry and diagnostics. This keeps the Date & Time module from becoming a second loader or security-policy authority.

## Migration rule

The current calls to `infiltratr_temporal_calendar_count()`, `infiltratr_temporal_calendar_at()`, `infiltratr_temporal_clock_mode_count()` and `infiltratr_temporal_clock_mode_at()` remain valid until Common changes their backing implementation to the runtime provider registry or introduces their versioned successors.

The System Settings UI should not be rewritten around temporary hard-coded Egyptian entries. The correct path is to complete provider discovery underneath the existing enumeration concept so newly installed capabilities appear automatically.

## Acceptance

This design goal is satisfied when:

- System Settings has no independent compiled catalogue of temporal capabilities;
- selectors enumerate the Common runtime registry;
- installing a conforming trusted provider causes its calendar/clock capabilities to appear without a System Settings code change;
- removing a provider produces an explicit unavailable state for persisted selections;
- malformed/incompatible providers cannot break unrelated Date & Time functionality;
- provider-specific historical rules and documentation remain outside System Settings.
