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

## Compatible fallback

An extended Infiltrator policy may have no exact representation in Mint.

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
