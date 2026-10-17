<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Temporal resource discovery

## Status

Calendar discovery is now an implemented additive runtime contract. System Settings consumes Common's merged installed calendar catalogue: every compiled calendar remains available and valid native resources discovered from `/System/Resources/Calendars` are appended without a System Settings master-list edit.

Clock resource discovery is intentionally one step behind. Common can discover and validate clock metadata from `/System/Resources/Clocks`, but System Settings continues to expose only executable built-in clock modes until a complete clock behaviour ABI has been qualified. Metadata presence alone is not sufficient to make a clock selectable.

The canonical resource contract is owned by Common in `Libraries`; Calendar owns calendar behaviour validation. This document defines only the System Settings side of that contract.

## The directory is the calendar catalogue

System Settings does not own a list of calendars.

Common discovers installed calendar resources and exposes one merged metadata registry. System Settings enumerates that registry, persists stable IDs and leaves chronology execution to Calendar.

```text
compiled Calendar providers ───────────────┐
                                           ├─ Common installed calendar catalogue
/System/Resources/Calendars/* ─────────────┘
                                                      ↓
                                           Date & Time selector
                                                      ↓
                                           Calendar behaviour engine
```

A native resource is not admitted merely because a file exists. Common validates its generic temporal metadata ABI; Calendar independently validates its complete calendar-provider behaviour ABI before the chronology engine accepts it. Existing built-in IDs win duplicates, so an optional resource cannot silently replace established chronology.

## Calendar and clock remain independent

Calendar and clock are independent presentation dimensions. The native filesystem therefore keeps separate capability roots:

```text
/System/Resources/Calendars
/System/Resources/Clocks
```

The calendar path is live today. The clock path is reserved and discoverable at the metadata layer, but its entries are not yet selectable in System Settings because no executable clock-provider contract has been qualified. This is deliberate fail-closed behaviour, not a missing fallback.

For example, the Egyptian work may install:

- `egyptian-early-lunar` — Early Egyptian Lunar Calendar;
- `egyptian-stellar-solar` — Early Egyptian Stellar–Solar Time.

They may be distributed together, but remain independently selectable capabilities once both behaviour contracts exist.

## Persistence and missing resources

System Settings persists stable resource IDs, never catalogue positions.

If a selected filesystem calendar is later removed, the persisted ID is retained. The Date & Time preview reports the resource as unavailable rather than silently substituting another calendar. Reinstalling a resource with the same stable ID can make the original selection executable again without rewriting canonical timestamps.

## Trust and ownership

System Settings does not parse or load calendar modules directly. Common owns generic discovery and metadata validation. Calendar owns chronology behaviour and its provider ABI. System Settings consumes only the installed metadata catalogue and the existing bounded Calendar formatting boundary.

This separation is intentional:

- Common answers which validated temporal capabilities are installed;
- Calendar answers how a calendar behaves;
- System Settings owns user selection and persistence;
- provider-specific historical rules remain with the provider/Calendar documentation.

## Current acceptance state

The calendar half of the design is satisfied when:

- all existing compiled calendars remain available;
- `/System/Resources/Calendars` is the exact case-sensitive native capability directory;
- a valid native calendar appears in the Date & Time selector without a System Settings source edit;
- Calendar independently validates executable behaviour before use;
- built-ins win duplicate IDs;
- malformed or incompatible modules cannot remove unrelated capability;
- missing persisted resources are shown as unavailable rather than silently replaced.

Clock completion is a separate future step: only after an executable clock behaviour ABI is qualified should System Settings switch its clock selector from the built-in catalogue to the installed clock catalogue.
