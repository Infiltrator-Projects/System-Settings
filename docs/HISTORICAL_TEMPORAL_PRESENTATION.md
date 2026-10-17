<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Historical temporal presentation

## Status and authority

This is a specialist design note subordinate to [PRESENTATION_POLICY.md](PRESENTATION_POLICY.md). It defines how System Settings and Common-aware applications should present historical clock systems whose attested units are coarser or structurally different from modern `HH:MM:SS` time.

It does **not** make any clock mode supported merely by documenting it. A historical clock is selectable only after its owning model, shared registry entry, formatter, boundary scheduling and validation are implemented and released.

Calendar owns historical chronology/time-model evidence. System Settings owns the user-facing selection and presentation policy. Common owns reusable formatting mechanics. Applications retain their canonical timestamps.

For the planned Ancient Egyptian mode, the historical evidence and reconstruction boundaries are maintained in Calendar's `docs/EGYPTIAN_TIMEKEEPING.md`.

## Governing rule

Historical presentation must not manufacture precision by inventing historical units.

If a historical system attests hours but not minutes or seconds, System Settings must not turn a modern timestamp such as

```text
2026-10-17T12:37:18.417+11:00
```

into a fictional historical string such as

```text
Egyptian 5:37:18
```

unless the `37` and `18` correspond to historically supported units under the owning model.

At the same time, presentation must not destroy the precision of the canonical timestamp merely because the historical notation is coarse.

These are separate concerns:

```text
canonical instant
    remains exact
        |
        v
historical model
    determines authentic/native unit and boundaries
        |
        v
presentation context
    chooses authentic display alone, or authentic display plus an
    explicitly modern precision extension
```

## Three evidence classes

A historical clock renderer should be able to identify every visible component as one of:

1. **attested** — supported by historical evidence or strong specialist scholarship;
2. **reconstructed** — a documented computational choice necessary to make incomplete evidence deterministic;
3. **modern precision extension** — a modern representation used to expose the position of an exact timestamp inside a historically attested unit.

A modern precision extension must never be named or styled so that a reasonable reader could mistake it for attested historical notation.

## Context-sensitive precision

The same canonical instant may legitimately be rendered differently in different semantic contexts.

### Ordinary clock/calendar surfaces

A panel clock, live preview or casual date/time label should prefer the historically attested granularity of the selected system.

For a future Egyptian temporal clock, a normal English rendering may therefore be similar to:

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
- flatten different day and night systems into one 24-hour sequence when the historical model does not support that interpretation.

## Planned Ancient Egyptian presentation

### User-facing name

The eventual selector label should expose the reconstruction rather than claim universality. A suitable form is:

```text
Ancient Egyptian temporal time — New Kingdom civil reconstruction
```

A shorter selector string may be used if layout requires it, but the detail/help text must preserve the New Kingdom-centred scope and the fact that it is a reconstruction.

The generic label **"Ancient Egyptian time"** by itself is too broad for specialist documentation because the evidence spans different periods and observational methods.

### Historical display

The planned model should use separate ordinal day/night hours, for example:

```text
3rd hour of the day
8th hour of the night
```

The transliterated Egyptian term `wnwt` may appear in explanatory material, but the UI must not fabricate pseudo-hieroglyphic timestamp syntax.

### Precision display

The historical model may expose an exact dimensionless phase through the current `wnwt`:

```text
phase = (instant - hour_start) / (hour_end - hour_start)
```

That phase is a modern computational interpolation. It represents the same sort of continuous progress that a shadow or water level can physically exhibit, but it is **not** to be labelled an Egyptian minute or second.

### Seconds preference

The system-wide `show-seconds` preference must not force fictional seconds into a historical clock that has no attested second unit.

For such a clock mode, `show-seconds` may instead increase the precision of an explicitly modern phase/progress display where the formatting context permits one. If no such precision extension is appropriate for the context, the historical display remains at its native granularity.

This behaviour must be documented per clock mode and regression-tested; a global boolean must not override historical semantics.

### Date and time independence

The selected calendar and clock remain independent presentation dimensions.

Selecting Calendar's Ptolemaic/Nabonassar-era Egyptian civil calendar together with the planned New Kingdom-centred Egyptian temporal clock is allowed as a user presentation combination, but System Settings must not describe that pair as a single historically attested timestamp standard. The two models are based on evidence from different historical contexts.

## Unavailable and ambiguous states

Historical clocks may legitimately be unavailable when their required physical or evidential conditions cannot be established.

System Settings must display that state honestly rather than silently falling back to a plausible-looking historical value.

Examples include:

- a location-dependent clock with no valid coordinates;
- a solar boundary that does not exist at the configured latitude/date;
- a future stellar/decanal model without the required era-specific data;
- a historical model that deliberately treats a twilight interval as outside numbered hours.

A conventional fallback may be offered as a separate clearly identified presentation, but must not masquerade as output from the selected historical clock.

## Help and provenance

Historical clock modes should expose concise user-facing provenance/help sufficient to answer:

- what period or convention is being modelled;
- which units are historically attested;
- which boundaries are reconstructed;
- whether geographic location is required;
- whether any finer precision is a modern interpolation;
- where the specialist model documentation lives.

This is especially important for systems whose familiar modern name hides substantial scholarly uncertainty or multiple historical practices.

## Acceptance requirements

A historical clock must not be exposed by System Settings until tests prove at least:

- the shared registry label identifies the intended historical model accurately;
- the preview renders a real value from the owning formatter rather than a mode name;
- native historical units are not replaced by modern `HH:MM:SS` assumptions;
- canonical timestamps remain unchanged;
- precision-sensitive contexts can distinguish nearby instants without inventing historical units;
- `show-seconds` cannot manufacture unsupported second notation;
- missing geographic/astronomical inputs produce an explicit unavailable state;
- help/provenance identifies reconstructions and modern extensions;
- Calendar, Common and System Settings agree on the same stable provider identity and semantics.

## Ancient Egyptian admission state

At the time this document is written, the Ancient Egyptian temporal clock is **planned, not supported**. The currently published clock catalogue must remain unchanged until Calendar/Common implement and validate the model.

Documentation is being recorded first so the eventual code has a falsifiable historical contract rather than acquiring behaviour by convenience and documenting it afterwards.
