<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Historical temporal presentation

## Status and authority

This is a specialist design note subordinate to [PRESENTATION_POLICY.md](PRESENTATION_POLICY.md). It defines how System Settings and Common-aware applications should present historical clock and calendar systems whose attested units, eras or notation differ from modern civil time.

It does **not** make any historical mode supported merely by documenting it. A calendar or clock is selectable only after its owning model, shared registry entry, formatter/converter, boundary scheduling where required and validation are implemented and released.

Calendar owns historical chronology/time-model evidence. System Settings owns the user-facing selection and presentation policy. Common owns reusable formatting mechanics. Applications retain their canonical timestamps.

For Egypt, the full historical catalogue and reconstruction boundaries are maintained in Calendar's `docs/EGYPTIAN_CHRONOLOGY_AND_TIMEKEEPING.md`.

## Governing rule

Historical presentation must not manufacture precision or uniformity by inventing historical units, eras or pairings.

If a historical system attests hours but not minutes or seconds, System Settings must not turn a modern timestamp such as

```text
2026-10-17T12:37:18.417+11:00
```

into a fictional historical string such as

```text
Egyptian 5:37:18
```

unless the `37` and `18` correspond to historically supported units under the owning model.

Likewise, if a historical date normally used regnal years, System Settings must not imply that a convenient modern continuous era was the ordinary historical year notation.

At the same time, presentation must not destroy the precision of the canonical timestamp merely because the historical notation is coarse.

These are separate concerns:

```text
canonical instant
    remains exact
        |
        v
historical model
    determines authentic/native units, era and boundaries
        |
        v
presentation context
    chooses authentic display alone, or authentic display plus an
    explicitly modern precision extension
```

## Evidence classes

A historical renderer should be able to identify every visible component as one of:

1. **attested** — supported by historical evidence or strong specialist scholarship;
2. **reconstructed** — a documented computational choice necessary to make incomplete evidence deterministic;
3. **modern precision extension** — a modern representation used to expose the position of an exact timestamp inside a historically attested unit;
4. **foreign system used in the region** — historically present but not to be relabelled as an indigenous system.

A reconstruction or modern precision extension must never be named or styled so that a reasonable reader could mistake it for attested historical notation.

## Context-sensitive precision

The same canonical instant may legitimately be rendered differently in different semantic contexts.

### Ordinary clock/calendar surfaces

A panel clock, live preview or casual date/time label should prefer the historically attested granularity of the selected system.

For a future New Kingdom Egyptian temporal clock, a normal English rendering may therefore be similar to:

```text
5th hour of the night
```

A continuous progress indicator through that hour is acceptable when the owning historical model supports continuous instrument progress, but the progress indicator is a modern visualisation rather than an ancient unit.

### Precision-sensitive human surfaces

File properties, forensic viewers, event details and diagnostic histories may need to distinguish instants that fall within the same historical hour.

Those surfaces may append an explicitly modern interpolation, for example:

```text
5th hour of the night · 63.7% elapsed
```

or another clearly labelled fractional phase approved by the owning model.

The application should retain access to the exact conventional/canonical timestamp when correlation, auditing or forensic work requires it.

### Machine/export surfaces

Machine-readable, protocol, storage and export timestamps do not follow decorative historical presentation unless the external format explicitly defines that behaviour.

They remain in the canonical/documented machine format.

## No false unit conversion

A historical temporal unit is not merely a renamed modern hour.

Where the owning model defines seasonal/unequal hours, Common must derive the historical hour boundaries first and only then locate the canonical instant within those boundaries.

It must not:

- divide a modern clock hour into 60 and relabel the result;
- assume historical day/night hours had fixed SI durations;
- infer minutes or seconds because a modern display context requests them;
- substitute another culture's subdivisions because they are numerically convenient;
- flatten different day and night systems into one 24-hour sequence when the historical model does not support that interpretation;
- apply one era's calendar name, clock method or month vocabulary universally across a civilisation's entire history.

## Egyptian systems are a family, not one mode

System Settings must not present one generic **Ancient Egyptian** calendar or clock when Calendar has admitted more historically precise models.

The historical catalogue includes, among other things:

- the wandering 365-day Egyptian civil calendar;
- historical regnal-year dating layered on civil month/day coordinates;
- lunar/lunistellar date-keeping whose exact status and regulation vary by period and remain partly disputed;
- the late Papyrus Carlsberg 9 lunar scheme;
- the Canopus reform attempt;
- the successful Alexandrian/Augustan leap-year reform;
- the Nabonassar astronomical era using Egyptian 365-day years;
- Middle Kingdom stellar/decanal material;
- New Kingdom shadow, sundial and water-clock timekeeping;
- later Ptolemaic/Roman seasonal and scientific equal-hour conventions.

Some of these are currently implementable, some are research-blocked, and some are foreign conventions used in Egypt rather than native Egyptian systems. Calendar's specialist document owns those classifications.

### Current Egyptian calendar mode

The currently implemented `egyptian-nabonassar` calendar is not to be shortened to a historically vague label in specialist UI/help. It is the Egyptian wandering civil structure with **Nabonassar-era astronomical year numbering**.

That is a valid deterministic calendar model, but it is not the same thing as ordinary pharaonic regnal dating.

### Future New Kingdom temporal clock

A suitable eventual selector label is:

```text
Ancient Egyptian temporal time — New Kingdom civil reconstruction
```

The model should use separate ordinal day/night hours, for example:

```text
3rd hour of the day
8th hour of the night
```

The transliterated Egyptian term `wnwt` may appear in explanatory material, but the UI must not fabricate pseudo-hieroglyphic timestamp syntax.

### Future Alexandrian calendar

If implemented, the reformed Augustan calendar should be labelled separately from both the wandering pharaonic civil calendar and the later Coptic era, for example:

```text
Egyptian Alexandrian calendar — Augustan reform
```

### Research-blocked Egyptian modes

A lunar, decanal or Ramesside stellar provider must not appear merely because a plausible algorithm can be invented. If Calendar marks the model as disputed or not admission-ready, System Settings must not expose it as settled historical fact.

## Precision display for Egyptian temporal hours

The historical model may expose an exact dimensionless phase through the current `wnwt`:

```text
phase = (instant - hour_start) / (hour_end - hour_start)
```

That phase is a modern computational interpolation. It represents the same sort of continuous progress that a shadow or water level can physically exhibit, but it is **not** to be labelled an Egyptian minute or second.

### Seconds preference

The system-wide `show-seconds` preference must not force fictional seconds into a historical clock that has no attested second unit.

For such a clock mode, `show-seconds` may instead increase the precision of an explicitly modern phase/progress display where the formatting context permits one. If no such precision extension is appropriate for the context, the historical display remains at its native granularity.

This behaviour must be documented per clock mode and regression-tested; a global boolean must not override historical semantics.

## Date and time independence

The selected calendar and clock remain independent presentation dimensions.

A user may intentionally combine models from different Egyptian periods. For example, Calendar's Ptolemaic/Nabonassar-era year count could be displayed with a New Kingdom-centred temporal clock. System Settings may allow that because calendar and clock are independent policies, but it must not describe the pair as one historically attested timestamp standard.

The same rule applies when a user combines an Egyptian calendar with a Roman, Babylonian, French Republican or modern clock, or an Egyptian clock with another calendar.

## Unavailable and ambiguous states

Historical clocks/calendars may legitimately be unavailable when their required physical or evidential conditions cannot be established.

System Settings must display that state honestly rather than silently falling back to a plausible-looking historical value.

Examples include:

- a location-dependent clock with no valid coordinates;
- a solar boundary that does not exist at the configured latitude/date;
- a stellar/decanal model without the required era-specific data;
- a historical model that deliberately treats a twilight interval as outside numbered hours;
- a regnal formatter without sufficient ruler/accession chronology;
- an observational lunar model when the required observational rule cannot be determined.

A conventional fallback may be offered as a separate clearly identified presentation, but must not masquerade as output from the selected historical model.

## Help and provenance

Historical modes should expose concise user-facing provenance/help sufficient to answer:

- what period or convention is being modelled;
- which units/era are historically attested;
- which boundaries or continuations are reconstructed;
- whether geographic location or astronomical observation is required;
- whether any finer precision is a modern interpolation;
- whether the mode is indigenous, reformed, or a foreign convention used in the region;
- where the specialist model documentation lives.

This is especially important for systems whose familiar modern name hides substantial scholarly uncertainty or multiple historical practices.

## Acceptance requirements

A historical mode must not be exposed by System Settings until tests prove at least:

- the shared registry label identifies the intended historical model accurately;
- the preview renders a real value from the owning formatter/converter rather than a mode name;
- native historical units are not replaced by modern `HH:MM:SS` assumptions;
- historical year eras are not silently substituted for regnal or other attested year labels;
- canonical timestamps remain unchanged;
- precision-sensitive contexts can distinguish nearby instants without inventing historical units;
- `show-seconds` cannot manufacture unsupported second notation;
- missing geographic/astronomical/historical inputs produce an explicit unavailable state;
- help/provenance identifies reconstructions and modern extensions;
- Calendar, Common and System Settings agree on the same stable provider identity and semantics.

## Egyptian admission state

At the time this document is written:

- `egyptian-nabonassar` is the implemented Egyptian calendar mode;
- the New Kingdom civil temporal clock is documented/planned, not supported;
- an Alexandrian reformed calendar is historically well-defined enough to be a future candidate but is not yet declared supported here;
- lunar, regnal and stellar/decanal Egyptian providers remain research or data work until Calendar's admission requirements are satisfied.

Documentation is recorded first so eventual code has a falsifiable historical contract rather than acquiring behaviour by convenience and documenting it afterwards.
