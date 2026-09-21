<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System-wide Presentation Policy

System Settings owns user-wide **presentation policy**: preferences that change how information is presented consistently across applications without changing the canonical data those applications store, calculate or exchange.

Infiltrator Common is the shared implementation layer that applications consume. System Settings is the user-facing authority that edits the policy.

The first concrete policy family is **Temporal Presentation**.

## Core invariant

Presentation policy must never redefine the underlying data merely to change how a user sees it.

```text
canonical value
      │
      ├── storage / calculation / interchange
      │        remains canonical
      │
      ▼
presentation policy
      │
      ▼
Common formatter
      │
      ▼
human-visible representation
```

Examples:

- a filesystem modification time remains the filesystem's native timestamp;
- an application database may retain Unix time, UTC nanoseconds or another exact representation;
- a capture/export format may retain ISO 8601 or another documented machine format;
- the UI can render that same instant as 12-hour time, 24-hour time, decimal 10-hour time or another supported clock/calendar representation.

A display preference must not corrupt sort order, arithmetic, protocol interoperability, forensic evidence or persisted timestamps.

## Ownership

### System Settings

System Settings owns:

- the user's selected presentation policies;
- the UI for changing those policies;
- validation of policy configuration;
- publication/change notification so running applications can respond;
- stable policy identifiers and defaults.

### Common

Common owns reusable policy consumption and formatting:

- reading/resolving the current policy;
- conversion from canonical values to human-visible representations;
- consistent formatting contexts;
- change notification/subscription helpers where appropriate;
- fallbacks when a requested presentation profile is unavailable or invalid.

Applications should not each reimplement system-wide presentation policy.

### Applications

Applications own:

- their canonical data model;
- choosing the semantic formatting context;
- machine/export formats where a stable external contract is required;
- domain-specific exceptions that genuinely must not follow user presentation.

An application asks Common to present a value; it does not need to understand every clock, calendar, numeric or measurement system Common may support.

## Temporal Presentation

Temporal policy separates at least four concepts:

1. **Instant** — a point on the time line.
2. **Civil date/time** — an instant interpreted through a timezone/calendar/clock presentation.
3. **Calendar date** — a date representation independent of a displayed clock where appropriate.
4. **Duration** — elapsed quantity, not a wall-clock timestamp.

These are not interchangeable.

A selected 10-hour civil clock must not silently reinterpret:

- a 127 ms OBD response;
- a 30-second timeout;
- a 2-hour process duration;
- a protocol-defined microsecond field.

Durations retain their domain units unless the application explicitly asks for a duration-format policy.

## Canonical versus presented time

Example:

```text
Canonical filesystem timestamp:
    2026-09-20T09:42:31.417Z

User policy:
    Timezone: Australia/Melbourne
    Clock mode: decimal

Displayed:
    <decimal-clock representation>

Machine export:
    2026-09-20T09:42:31.417Z
```

The exact canonical representation is chosen by the owning subsystem. System Settings does not require every application to store Unix time; it requires presentation to be separated from storage.

## System-wide effect

When an Infiltrator application displays a human-facing timestamp, it should normally use Common Temporal rather than directly formatting the value.

That includes, where applicable:

- filesystem Created/Modified/Accessed times;
- Software history;
- System Monitor event/process timestamps;
- Calendar presentation;
- Runner Monitor histories;
- Defragmenter operation history;
- MBLink/Jaglink/LINK user-facing capture/event times;
- settings diagnostics;
- future file-manager and desktop surfaces.

Changing the policy should therefore affect the next presentation of those values without rewriting the underlying files, databases or captures.

Running applications should refresh affected visible values when practical after a policy change.

## Formatting contexts

Applications need semantic contexts rather than format strings scattered through source code.

Conceptually:

```c
COMMON_TEMPORAL_TIME_SHORT
COMMON_TEMPORAL_TIME_WITH_SECONDS
COMMON_TEMPORAL_DATE_SHORT
COMMON_TEMPORAL_DATE_LONG
COMMON_TEMPORAL_DATE_TIME
COMMON_TEMPORAL_FILE_TIMESTAMP
COMMON_TEMPORAL_HISTORY_TIMESTAMP
COMMON_TEMPORAL_LOG_DISPLAY_TIMESTAMP
COMMON_TEMPORAL_EXPORT_TIMESTAMP
```

The final Common API may use different names/types, but the contract is durable: the application expresses **what kind of human-facing value it needs**, while Common resolves **how the user asked for that value to look**.

Machine/export contexts may deliberately resolve to a fixed canonical representation instead of following the human display clock.

## Clock systems

Temporal Presentation uses stable clock-system identifiers supplied by Common rather than application-local conditionals.

Common 1.19.20 defines the complete current catalogue shared with Calendar: standard OS-locale time, explicit 12-hour and 24-hour time, Internet Time, Unix time, binary and hexadecimal clocks, Julian/MJD, sidereal/apparent/mean-solar time, French Republican decimal time, traditional Chinese double-hours and hundred-kè, Roman temporal time, Edo Japanese seasonal time, Italian hours, Babylonian hours, Indian ghaṭī time and Nuremberg hours.

System Settings is the authority. The internal Common catalogue retains the `standard` identifier as a bootstrap/fallback for consumers that must run without System Settings, but System Settings itself does not present that identifier as a third conventional choice. Its user-facing conventional clock systems are **Standard time (12-hour)** and **Standard time (24-hour)**. The Cinnamon/GNOME 12/24-hour Boolean is the exact native compatibility backend for those modes, not another visible authority. Common-aware applications consume the System Settings policy directly rather than offering competing local clock/calendar choices.

### Decimal 10-hour profile

The intended decimal clock model has:

```text
1 civil day      = 10 decimal hours
1 decimal hour   = 100 decimal minutes
1 decimal minute = 100 decimal seconds
```

A normal non-transition civil day therefore maps conceptually:

```text
00:00 conventional → 0:00:00 decimal
06:00 conventional → 2:50:00 decimal
12:00 conventional → 5:00:00 decimal
18:00 conventional → 7:50:00 decimal
next midnight     → 0:00:00 next day
```

Daylight-saving transition semantics must be specified and regression-tested before the profile is considered complete. The implementation must not invent ambiguous behaviour at repeated or skipped local times.

## Calendar profiles

Clock representation and calendar representation are independent policy dimensions.

Temporal policy v3 combines these dimensions directly. For example:

```text
Timezone:       Australia/Melbourne
Calendar:       Egyptian civil
Clock:          French Republican decimal time
Seconds:        shown
Location:       optional latitude/longitude
```

The current shared catalogue contains 30 selectable calendar systems. There is one system calendar policy, not primary/secondary calendar state. Calendar remains the implementation owner of chronology arithmetic; Common/System Settings own the stable policy identifiers and user-wide selection contract.

## Timezone

Timezone remains a semantic input to civil presentation.

Changing the clock profile does not change the timezone. A canonical instant is first interpreted according to the selected/authoritative timezone rules, then formatted through the selected clock/calendar presentation.

System timezone changes and user presentation choices may be separate settings where the operating system supports that distinction.

## Geographic location and regional context

Time zone, locale and physical location are related inputs but are not the same setting.

On Linux, System Settings may read the configured IANA time zone and the representative coordinate published by the installed tzdata database. That coordinate is useful as a starting approximation for location-dependent clock systems. It is not evidence that the computer is physically located at that coordinate.

The operating-system time zone always provides the initial regional approximation when tzdata publishes a reference coordinate. There is no user-facing "Not selected" pseudo-location: the application can start usefully from native system state without claiming that the reference point is the user's precise position.

The user may refine that approximation by searching for a named locality. On Linux the current implementation uses geocode-glib's maintained geocoding service path and records the selected display name plus coordinates as geographic presentation metadata. The coordinates are the semantic input used by location-dependent clocks; the display name exists so the UI can continue to say "Mooroopna" rather than degrading the user's choice back into two unexplained numbers.

Manual latitude/longitude remains available as an advanced override. A locality may also produce a best-effort same-country time-zone suggestion from installed tzdata, but geographic location never silently becomes the source of truth for the IANA time zone: the system time-zone selector remains independently visible and editable.

Language, regional formats, time zone and geographic location will remain independently writable dimensions when the broader Language & Region module is implemented.

## Filesystem timestamps

Filesystem metadata demonstrates the architectural rule clearly.

System Settings does not rewrite inode/file timestamps when the user changes clock presentation.

Instead:

```text
filesystem timestamp
        ↓
canonical instant
        ↓
Common Temporal
        ↓
selected system presentation
        ↓
Created / Modified / Accessed text
```

Sorting by time operates on the canonical value, not the formatted string.

## Logs

The word "log" covers two different contracts.

### Machine/forensic logs

When logs are intended for parsing, interchange, correlation or forensic analysis, their stored timestamp should remain an unambiguous documented canonical format.

### Human log viewers

When an application renders a log for a person, the viewer may present the canonical timestamp through the active temporal policy while retaining access to the original canonical value.

Thus a log can be stored once and viewed in different clock/calendar systems without data loss.

## Policy persistence and notification

Presentation policy is user state, not application-local state.

The Infiltrator policy is an optional enrichment layer for participating applications, not a hard dependency that prevents an application from running without System Settings. When no valid policy has been published, a platform-integrated consumer resolves the equivalent native operating-system/desktop preferences and remains fully usable. On Mint/Cinnamon, Calendar therefore behaves like the stock temporal surface until an Infiltrator policy exists; once a valid policy exists, the richer Infiltrator clock/calendar/location choices become authoritative for Common-aware presentation.

Common 1.19.19 provides the canonical POSIX XDG location, validated load/atomic-save mechanics and the installed `temporal-v3` provider capability contract for this policy. System Settings publishes the provider marker when installed and writes the authoritative per-user `presentation.conf` only after the user changes temporal policy. Policy v3 stores `clock-mode`, `calendar`, `show-seconds`, `location-configured`, `latitude` and `longitude`. Linux stores it below the XDG configuration home; Windows stores the equivalent user policy below LocalAppData. Common parses/serializes the same versioned contract on both platforms. Existing v2 files are privately migrated by keeping their former primary calendar as the single v3 calendar and discarding the retired secondary value.

## Third-party applications

All Infiltrator applications can be migrated to consume Common policy.

Arbitrary existing applications on Linux or Windows cannot be forced to obey a new clock/calendar representation if they hard-code their own formatting. System Settings may later expose a documented public API/service so willing third-party applications can use the same policy.

Compatibility with ordinary locale-aware applications should be preserved where possible through each platform's documented locale/globalisation settings. The project must not globally intercept unrelated process time functions in a way that changes program semantics.

## Migration rule for existing applications

The migration is a presentation cleanup, not a timestamp-storage rewrite.

For each project:

1. inventory direct human-visible date/time formatting;
2. classify each use as instant, civil date/time, calendar date, duration or machine/export timestamp;
3. replace human-visible formatting with Common Temporal contexts;
4. retain protocol/storage/export contracts unless there is a separate reason to change them;
5. remove duplicate local formatting helpers once Common is at least as strong;
6. add regression tests proving the selected policy changes visible output but not canonical stored values.

New Infiltrator code should not introduce raw `strftime`, `std::put_time`, toolkit-specific date formatting or equivalent for user-visible timestamps when the Common Temporal contract can express the requirement.

## Broader presentation-policy direction

Temporal Presentation establishes a more general architectural pattern.

Future policy families may include:

- number formatting and grouping;
- measurement systems;
- temperature;
- pressure;
- distance/speed;
- storage-unit presentation;
- other cross-application presentation conventions.

Each future family must preserve the same rule:

> canonical data belongs to the application/domain; presentation policy belongs to the user.

A future policy is admitted only when consistency across applications is valuable and the transformation can be defined without changing the meaning of the underlying data.

## Validation

Temporal policy is not complete until tests prove at least:

- canonical values are unchanged by presentation-policy changes;
- sorting/comparison uses canonical values;
- every supported profile produces deterministic output;
- timezone changes and clock-profile changes remain distinct;
- durations are not accidentally treated as civil timestamps;
- machine/export contexts remain stable when documented as fixed;
- visible values refresh after policy changes where supported;
- invalid/unknown profiles fall back safely;
- DST skipped/repeated local times behave according to the documented profile contract;
- multiple migrated applications produce identical output for the same timestamp/context/policy.

See [VALIDATION.md](VALIDATION.md) for the repository-wide evidence rules.
