<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# First Egyptian selector profile

## Purpose

This document defines the first native Egyptian calendar-and-clock pair that System Settings should present once the corresponding Calendar/Common providers are implemented and validated.

Calendar owns the historical model. The detailed evidence and model rules live in Calendar's `docs/EGYPTIAN_FIRST_SYSTEM.md`. System Settings owns only the user-facing policy and selector mapping.

The profile reflects the project's **current best historical model**. If stronger evidence changes Calendar's historical conclusion, System Settings follows that change. The UI does not preserve an obsolete historical rule merely for compatibility.

## Selector pair

The first Egyptian profile is:

```text
Calendar: Early Egyptian Lunar Calendar
Clock:    Early Egyptian Stellar–Solar Time
```

These are two independent Temporal Presentation choices. A convenience profile may select them together, but the stored calendar and clock remain separate policy dimensions.

## Calendar presentation

**Early Egyptian Lunar Calendar** represents the earliest native Egyptian lunar system currently favoured by the project's evidence review.

Its presentation must not call it the later 365-day civil calendar, the Coptic calendar, the Nabonassar era or a Ptolemaic/Roman calendar.

The visible calendar should be driven by the owning Calendar provider and may expose lunar month/day information and the Nile seasonal association supported by that model.

System Settings must not invent a Sirius/Sopdet rule, a fixed arithmetic intercalation cycle or late lunar-day names if Calendar does not provide them.

## Clock presentation

**Early Egyptian Stellar–Solar Time** is the first full-day Egyptian clock profile.

Its visible semantics are:

- twelve stellar night divisions;
- twelve solar daylight divisions;
- day and night numbering kept separate;
- seasonal/unequal duration;
- explicit dawn/dusk transition states when supplied by the owning model;
- no invented Egyptian minutes or seconds.

A normal display may therefore read, for example:

```text
5th hour of the night
```

or:

```text
3rd hour of the day
```

A precision-sensitive surface may append the modern within-division progress supplied by Common, but it must not label that progress as an Egyptian minute or second.

## Naming rule

The selector names are historical names, not confidence labels. They should therefore be:

```text
Early Egyptian Lunar Calendar
Early Egyptian Stellar–Solar Time
```

Do not append `reconstruction` to those user-facing names.

Historical uncertainty, competing scholarship and the evidence-change rule belong in the provider help/provenance and Calendar's specialist documentation, not in the basic selector label.

## Evidence-change rule

The first Egyptian profile is not immutable.

If Calendar's maintained evidence review changes the earliest lunar year rule, lunar boundary, stellar mapping, day-hour structure or transition semantics, System Settings must consume the revised provider behaviour and update its explanatory text in the same release.

The requirement is historical accuracy according to the strongest current evidence, not preservation of a superseded model.

## Availability

This document does not declare the two modes implemented.

The selector entries may appear only when the shared registry exposes admitted providers whose identifiers and semantics match Calendar's historical authority and whose preview/output paths are validated.

Until then, the current published catalogue remains unchanged.