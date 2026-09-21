<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Mint/Cinnamon Compatibility

System Settings is designed to replace the user-facing settings experience on Linux Mint/Cinnamon without unnecessarily replacing the underlying Mint/Cinnamon configuration contracts. This document is the Mint-specific application of the general [Platform Compatibility](PLATFORM_COMPATIBILITY.md) contract; Windows is a sibling first-class target documented separately.

The compatibility rule is:

> **Use the existing Mint/Cinnamon setting when it already represents the same thing. Extend it only when the requested concept does not exist. Replace an underlying contract only when there is a demonstrated technical reason.**

This allows System Settings to be more capable than Cinnamon Settings while remaining a good citizen of the desktop it is running on.

## Three compatibility levels

### 1. Native compatibility

When Mint, Cinnamon, GNOME or another authoritative desktop/system component already owns the real setting, System Settings reads and writes that existing setting directly.

Examples include ordinary desktop preferences such as:

- conventional 12/24-hour clock preference;
- theme/desktop preferences;
- keyboard and pointer preferences;
- other GSettings, D-Bus or system-service settings that are already the actual source of truth.

System Settings must not create a second Infiltrator preference that shadows an equivalent existing Mint preference.

Conceptually:

```text
System Settings
      ↓
existing Mint/Cinnamon setting
      ↓
Cinnamon / Nemo / other existing consumers
```

If the user changes the value through Cinnamon's tools instead, System Settings should observe the authoritative change and display the new value.

### 2. Infiltrator extension

When the desired concept does not exist in Mint/Cinnamon, System Settings may define an Infiltrator policy for it.

Examples include a clock profile such as decimal 10-hour time or another documented temporal representation that the ordinary Cinnamon preference model cannot express.

Conceptually:

```text
System Settings
      ↓
Infiltrator extended policy
      ↓
Common
      ↓
Infiltrator applications
```

The new policy must not overload an existing Mint value with a different meaning. A Boolean such as "use 24-hour clock" remains a Boolean about the conventional clock; it must never secretly mean "decimal clock enabled."

### 3. Enhanced integration

Where useful, selected Mint applications may later be taught to consume an Infiltrator extension through a small, maintainable integration rather than through a permanent full fork.

For example, a file manager could continue reading the filesystem's canonical timestamp but ask the shared temporal service to render that timestamp using the selected system presentation policy.

Enhanced integration is optional. The core System Settings design must remain correct even when an unmodified third-party/Mint application understands only the ordinary Mint setting.

## Provider capability

On Mint/Linux, the System Settings package publishes the canonical `/usr/share/infiltrator/policy-providers/temporal-v3` capability marker defined by Common. Consumers use that marker rather than executable-name/PATH probing. The marker means the richer provider is installed; it does not by itself mean a user policy exists. Until a valid policy is present, native Cinnamon settings remain authoritative.

## Region, time zone and geographic location

The long-term Mint/Cinnamon integration treats language, regional formats, time zone and geographic location as related but separate authorities.

System Settings should read the native Mint/Linux values first and eventually replace the user-facing locale/control-centre surface by writing through to those same native authorities. It must not create an Infiltrator shadow copy of ordinary locale or time-zone state.

The installed IANA time zone may provide a representative tzdata coordinate. System Settings can offer that coordinate as an explicit approximation for location-dependent Infiltrator features, but must not infer that the user physically lives in the reference city. A more precise locality choice is a separate user action.

## Date & Time replacement boundary

The Date & Time module deliberately replaces the current Mint/Cinnamon frontend while retaining Mint/Linux native authorities underneath it.

For the supported current systemd path:

```text
System Settings
    ├── Time zone ───────────────→ org.freedesktop.timedate1.SetTimezone
    ├── Network time ────────────→ org.freedesktop.timedate1.SetNTP
    ├── Manual date/time ────────→ org.freedesktop.timedate1.SetTime
    ├── 12/24-hour ──────────────→ org.cinnamon.desktop.interface
    │                                + GNOME clock-format compatibility mirror
    ├── Display date ────────────→ org.cinnamon.desktop.interface
    ├── Display seconds ─────────→ temporal policy + exact Cinnamon fallback
    └── First day of week ───────→ org.cinnamon.desktop.interface
```

The shell never becomes root. Protected timedated operations request policy authorisation through the system service only when the user performs the operation.

The former Mint map/Region/City presentation is not itself a source of truth. System Settings exposes the authoritative IANA time zone directly and adds named-place search. A place such as Mooroopna resolves to a display name and coordinates; those coordinates feed location-dependent Common temporal presentation. A same-country nearest-zone lookup may update the system time zone, but the explicit IANA selector remains visible so a heuristic can always be corrected.

Named-place lookup is asynchronous and network-dependent. Failure of the geocoder must not disable manual coordinates, the time-zone selector, NTP controls or other Date & Time settings.

## Compatible fallback

An extended Infiltrator policy may have no exact representation in Mint.

The presence of System Settings must never become a runtime prerequisite for an application that can otherwise operate on Mint. Before a valid Infiltrator policy exists, Common-aware consumers use Mint/Cinnamon's native temporal preferences and ordinary locale behaviour. Publishing an Infiltrator policy enriches those consumers; it does not create a second mandatory desktop service.

In that case System Settings records the real Infiltrator policy and, only where useful and unambiguous, maintains a compatible conventional Mint value for applications that do not understand the extension.

Example:

```text
Selected policy: decimal 10-hour clock

Infiltrator policy:
    decimal-10

Common-aware applications:
    decimal-10 presentation

Unmodified Cinnamon/Mint applications:
    conventional compatible fallback
```

A fallback is not an assertion that the two policies are equivalent. It exists only to keep ordinary desktop components usable.

If no safe fallback exists, System Settings must not write a misleading Mint value merely to force apparent compatibility.

## Authority rules

For every setting, implementation work must identify one authoritative owner.

The preference falls into one of these classes:

1. **Existing desktop/system authority** — System Settings is another frontend to the existing value.
2. **Infiltrator presentation/system policy** — System Settings owns the new policy and Common exposes it to participating applications.
3. **Specialised application authority** — System Settings launches or deep-links to the application that already owns the domain.
4. **Unsupported/unresolved** — no write is performed until the correct authority is known.

There must not be two independent writable values representing the same semantic setting.

## Read/write synchronisation

For an existing Mint/Cinnamon setting:

```text
read authoritative Mint value
       ↓
display in System Settings
       ↓ user changes value
write authoritative Mint value
       ↓
read back / observe change
       ↓
display confirmed state
```

System Settings does not maintain a second local copy merely to remember what it wrote.

For an Infiltrator extension:

```text
read Infiltrator policy
       ↓
display in System Settings
       ↓ user changes value
write Infiltrator policy
       ↓
publish change
       ↓
Common-aware applications refresh
       ↓
optional compatible Mint fallback updated
```

## Temporal example

Conventional 12-hour and 24-hour presentation can map to the existing desktop preference where that preference is authoritative.

A decimal 10-hour profile cannot be represented faithfully by that conventional choice, so its real identity remains an Infiltrator temporal policy.

Conceptually:

```text
12-hour selected
  Infiltrator temporal policy = conventional-12
  Mint conventional setting   = 12-hour

24-hour selected
  Infiltrator temporal policy = conventional-24
  Mint conventional setting   = 24-hour

decimal 10-hour selected
  Infiltrator temporal policy = decimal-10
  Mint conventional setting   = compatible conventional fallback
```

The canonical timestamp is unchanged in all three cases.

See [PRESENTATION_POLICY.md](PRESENTATION_POLICY.md).

## Backend adapters

Mint/Cinnamon compatibility should be implemented behind explicit adapters rather than scattered throughout panels.

Conceptually:

```text
Date & Time module
      ↓
Temporal policy service
      ├── Mint/Cinnamon adapter
      └── Infiltrator extension store
```

and:

```text
Appearance module
      ↓
Mint/Cinnamon appearance adapter
      ↓
authoritative desktop settings
```

This keeps the user-facing module independent of the exact schema/service names and gives the project one place to adapt when Mint changes an underlying interface.

An adapter must not invent semantics that the upstream setting does not have.

## Version and capability detection

System Settings targets a changing Linux desktop, so compatibility code must detect available schemas/interfaces/capabilities rather than assuming every Mint release is identical.

A module should distinguish:

- supported and writable;
- supported but read-only;
- missing on this desktop version;
- replaced by a newer authoritative interface;
- extension available only to Common-aware applications.

Unknown versions should degrade to explicit unavailable/unsupported state rather than writing guessed keys.

## Other Linux desktops

Mint/Cinnamon is the first target, but this architecture deliberately avoids baking Cinnamon-specific details into the shell or Common.

A future platform adapter may map the same logical setting to another desktop's authoritative interface.

The logical product contract therefore looks like:

```text
System Settings module
        ↓
platform compatibility adapter
        ↓
authoritative desktop/system setting
```

while genuinely new Infiltrator policies remain separately defined.

## Forking policy

Do not maintain a full Cinnamon, Nemo or other upstream fork merely to obtain an integration that can be achieved through:

- an existing public setting/API;
- a small upstreamable patch;
- a narrow optional adapter/plugin;
- a documented public temporal/presentation service.

A fork is considered only when the feature is important, no stable extension point exists, and the maintenance cost is justified explicitly.

## Validation

Compatibility work is not complete until tests or environment evidence prove:

- System Settings reads the value set by the existing Mint tool;
- changing the value in System Settings changes the authoritative Mint value;
- existing consumers observe the change where they normally would;
- changing the value outside System Settings is reflected back;
- Infiltrator extensions never corrupt or overload an existing Mint setting;
- fallbacks are deterministic and documented;
- missing/changed schemas fail safely;
- extended policies remain correct even when ordinary Mint applications only use the fallback.

## Design principle

System Settings is intended to be a **compatible superset**, not an isolated second configuration universe.

Its relationship with Mint/Cinnamon is therefore:

```text
reuse what is already correct
        ↓
extend what is missing
        ↓
replace only when necessary
```
