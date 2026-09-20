<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Design

System Settings should feel like one deliberate operating-system component even though its implementation is modular.

The design goal is not to imitate a particular release of Windows, macOS, AmigaOS or Linux Mint. Those systems are evidence. System Settings keeps the strongest useful patterns and rejects weaknesses that are consequences of their historical architecture.

## Interaction model

The primary window has four stable regions:

```text
+----------------------------------------------------------------+
| System Settings                                      Search    |
+----------------------+-----------------------------------------+
| categories/history   | panel title / description               |
|                      |                                         |
| Appearance           | current module content                  |
| Hardware             |                                         |
| Network              |                                         |
| System               |                                         |
| Security             |                                         |
+----------------------+-----------------------------------------+
```

The exact responsive layout may change with window width, but the conceptual model remains:

- persistent orientation;
- clear current location;
- fast category switching;
- direct search;
- no maze of modal launchers.

## Categories

Initial categories are:

### Appearance

Theme, fonts, desktop presentation and user-facing visual behaviour.

### Hardware

Displays, sound, keyboard, mouse/touchpad, Bluetooth, printers and hardware-facing controls.

### Network

Wi-Fi, Ethernet, VPN, proxy and connectivity configuration.

### System

Users, date/time, storage, startup, power, system information and entry points into specialised system applications.

### Security

Authentication, firewall/security surfaces and other security configuration that does not belong more naturally to a specific domain.

Categories are allowed to evolve when real modules demonstrate a stronger grouping. A category is not retained merely because it was written down first.

## Specialised tools

A coherent settings experience does not require every system tool to be embedded.

Rows that lead to an established specialised application should look intentional and should state the destination when that matters. Examples include:

- Software / Updates → Infiltrator Software;
- live performance/process diagnostics → System Monitor;
- filesystem defragmentation/recovery → Defragmenter.

When the destination supports a stable deep link, Settings should open the relevant view directly. Otherwise it launches the application's normal entry point. Settings does not create a reduced copy of the specialised feature merely to avoid leaving the window.

If the destination application is unavailable, present that as an unavailable integration with enough identity to understand what is missing.

## Search

Search must answer the user's intent, not merely match panel names.

Each manifest can index:

- panel title;
- keywords;
- individual setting names;
- synonyms;
- protocol/technology terms;
- deep-link identifiers.

Examples:

```text
resolution → Hardware → Displays → Resolution
dns        → Network → Wi-Fi → DNS
proxy      → Network → Proxy
lid        → System → Power → Close laptop lid
password   → System → Users → Password
```

Search results display the path so identically named settings remain understandable.

Search should tolerate ordinary spacing/case differences and useful aliases. Fuzzy matching may be added only if it improves discovery without producing noisy unrelated results.

## Deep links

Every searchable setting has a stable logical identity. The shell can route directly to it from:

- search;
- command line;
- a future notification;
- another project-owned application;
- documentation/help.

Deep links identify behaviour, not widget hierarchy. Refactoring a panel layout should not unnecessarily invalidate links.

## Presentation

System Settings consumes the complete Common presentation contract rather than recreating a private approximation.

That includes the current:

- typography roles;
- Day/Night/Follow-system themes;
- graphite/cyan/gold Night palette where defined by Common;
- standard radii, cards, separators and status surfaces;
- buttons, toggles, text fields, lists and navigation controls;
- spacing and focus treatment.

A local panel may introduce a domain visualisation when necessary, but it must not invent a second general design system.

## System-wide presentation choices

Some controls change how information is represented throughout the user's environment rather than configuring only the Settings application.

Temporal Presentation is the first such design. Date & Time should eventually expose user-facing choices such as timezone, calendar profile, clock profile, seconds/date style and related presentation settings without implying that underlying timestamps are rewritten.

A preview should show the effect of a clock/calendar choice before it is applied. The UI must distinguish ordinary 12/24-hour selection from alternate profiles such as decimal 10-hour time, and historically inspired profiles must state the actual implemented model rather than relying on a vague name.

Changing presentation policy should update compatible running applications through the shared Common contract where practical.

See [PRESENTATION_POLICY.md](PRESENTATION_POLICY.md).

## Responsive behaviour

The settings window must remain useful on ordinary laptop resolutions. No panel may assume an oversized desktop.

At narrower widths:

- category navigation may collapse to a compact rail/drawer;
- two-column setting groups may become one column;
- labels wrap rather than clip;
- primary actions remain reachable without horizontal scrolling.

Minimum sizes must be justified by real content rather than developer convenience.

## Panel construction

Panels load on demand. A panel may initially show a lightweight skeleton/status while its authoritative data arrives.

Loading indicators are used only for work that is actually pending. An indefinitely spinning panel is a defect; timeouts/errors must resolve to actionable state.

## Apply model

### Immediate settings

Use immediate apply for simple user-level preferences when:

- the operation is low risk;
- a single value is independently meaningful;
- rollback is trivial or the setting remains visible;
- the backend can be read back immediately.

Example: choosing a theme.

### Staged settings

Use staged Apply when several values form one configuration or a partial change could be invalid.

Example: manual IP address, prefix, gateway and DNS fields.

The panel makes dirty state visible. Apply validates the complete candidate before issuing the change.

### Destructive actions

Deleting an account, forgetting a network profile or another destructive action requires clear confirmation proportionate to the consequence.

Confirmation text identifies what will happen. It does not use vague "Are you sure?" wording without context.

### Recovery-sensitive changes

Display configuration that can leave the user without a usable screen should use timed confirmation and automatic rollback when technically reliable.

Networking changes that may disconnect the current session should disclose that consequence before application and preserve a recovery path where possible.

## Authentication experience

Opening a panel does not request credentials simply because some actions inside it are protected.

Protected controls can display their current state read-only. Authentication occurs only when the user invokes the protected operation.

Cancellation is ordinary control flow, not an error dialog storm.

The UI never requests an administrator password into a project-owned text field when the platform authorisation agent can own that interaction.

## State changed elsewhere

If the backend changes while a panel is open:

- untouched controls update to the new authoritative value;
- active edits are not silently overwritten;
- a conflicting edit is marked and the user is told what changed;
- applying stale candidate data requires revalidation.

## Errors

Errors are placed near the affected operation when possible.

The design distinguishes:

- unsupported hardware/service;
- service stopped/unavailable;
- permission denied;
- authentication cancelled;
- invalid user input;
- transient failure;
- permanent/backend error.

Technical detail can be expandable. The primary message should state what failed and whether anything changed.

## Accessibility

Accessibility is part of the component contract.

At minimum:

- every interactive control has an accessible name/role;
- keyboard traversal is complete and predictable;
- focus is visible in every theme;
- colour is never the sole carrier of state;
- search results and validation errors are announced appropriately through the toolkit accessibility layer;
- text respects supported scaling rather than relying on fixed pixel heights;
- destructive and authentication actions remain operable without a pointer.

A custom control that cannot provide equivalent keyboard/accessibility semantics should not replace a standard control merely for appearance.

## Keyboard behaviour

Expected shell-level shortcuts should include conventional navigation/search patterns where they do not conflict with the desktop:

- focus search;
- back/forward navigation;
- close window;
- move through results/categories.

Exact bindings are finalised during implementation and documented with tests where practical.

## Saving shell preferences

Only shell-owned preferences are persisted by the shell, for example:

- selected theme mode;
- window geometry if appropriate;
- last safe category/panel;
- navigation/search presentation preferences.

Domain settings remain in their authoritative operating-system backend. System Settings does not create a shadow database of the system merely to remember what it believes the values are.

## Visual restraint

Settings is an operating-system tool, not a dashboard. Animation and decorative surfaces must support orientation or state rather than compete with the controls.

Dense expert panels are acceptable when the domain requires them, but hierarchy should remain clear. Hiding important configuration behind arbitrary whitespace is not simplicity.

## Non-goals

The initial product is not:

- a clone of Cinnamon Settings;
- a new configuration database;
- a replacement for NetworkManager, BlueZ, CUPS, systemd or other system services;
- a generic third-party plugin host;
- a privileged control daemon;
- a collection of independent windows pretending to be one application.
