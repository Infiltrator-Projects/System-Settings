<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Validation

System Settings changes persistent machine configuration. Validation therefore has to prove more than compilation or successful widget construction.

## Evidence classes

A capability can have several evidence levels:

1. **compiled** — source builds with the required warning policy;
2. **unit verified** — deterministic local logic passes tests;
3. **integration verified** — module communicates correctly with a controlled/real native service interface;
4. **environment verified** — behaviour is demonstrated on the relevant supported platform/environment;
5. **hardware verified** — hardware-sensitive behaviour is demonstrated on representative physical hardware when simulation cannot prove it.

Documentation must not silently promote a lower evidence level into a stronger claim.

## Shell tests

Automated shell coverage should include:

- manifest parsing and malformed-input rejection;
- duplicate module/target IDs;
- unsupported ABI versions;
- missing library/entry point on both native loader families;
- lazy-load proof: unopened modules do not initialise;
- category ordering;
- search token/synonym routing;
- deep-link routing;
- history/back/forward behaviour;
- module load failure isolation;
- dirty-navigation handling;
- module destruction with pending asynchronous work;
- Linux shared-object and Windows DLL trusted-path loading;
- proof that public module ABI contains no platform UI/toolkit types;
- restoration of safe last location;
- theme persistence through Common.

## Manifest/parser boundaries

Manifest tests should cover:

- maximum accepted lengths/counts;
- invalid UTF-8 where relevant;
- missing required fields;
- unknown optional fields;
- path traversal attempts;
- absolute library path attempts;
- duplicate search target identifiers;
- invalid category identifiers;
- incompatible API versions;
- malformed escapes/separators;
- deterministic ordering.

A manifest cannot cause arbitrary command execution.

## Module ABI tests

The project should build fixture modules that intentionally provide:

- valid v1 API;
- too-small API structure;
- newer unsupported major API;
- null required callbacks;
- failing create;
- failing activation;
- delayed async completion followed by destruction.

This ensures loader error handling is tested independently of real settings modules.

## Read/write validation

Every writable setting needs evidence for:

- initial read;
- valid write;
- authoritative read-back or change notification;
- invalid candidate rejection;
- permission denied where relevant;
- authorisation cancelled where relevant;
- backend/service disappearance;
- external change while panel is open;
- navigation/close during operation where relevant.

Optimistic UI state without backend confirmation is not enough.

## Transaction and rollback tests

Staged modules should test:

- dirty state;
- Apply disabled for invalid candidate;
- multi-field transaction failure;
- backend changes between read and apply;
- discard confirmation;
- successful apply then authoritative reconciliation.

Recovery-sensitive modules add domain tests. Display settings, for example, should prove timed revert logic using a controlled environment and physical hardware where necessary.

## Privilege tests

Required security regression checks include:

- opening the application never triggers authentication;
- opening a protected panel read-only never triggers authentication merely because it is protected;
- only the invoked protected operation requests authorisation;
- cancellation leaves state unchanged;
- denial leaves state unchanged;
- a successful authorisation cannot be reused for unrelated project-defined arbitrary actions;
- the application never receives/stores a plaintext administrator password from its own UI.

The platform may cache authorisation according to its own policy; tests distinguish that from project-wide privilege.

## Responsiveness

Validation should measure user-visible startup and interaction properties rather than only CPU time.

At minimum prove:

- shell first paint does not wait for network, Bluetooth, printing, account or storage enumeration;
- a blocked/unavailable service does not prevent unrelated panels opening;
- global search does not instantiate every module;
- slow module discovery work does not block the UI thread;
- repeated panel open/close does not leak threads, file descriptors or subscriptions.

Performance budgets can be made numeric once the initial implementation provides a baseline.

## Date & Time replacement acceptance

The Mint/Cinnamon Date & Time replacement is validated at separate evidence levels.

Automated builds/tests must prove:

- the full IANA zone catalogue can be built from installed tzdata;
- regional coordinate parsing remains bounded and deterministic;
- Linux builds link the maintained geocode-glib API and package its runtime dependency;
- Windows remains buildable even though Linux-native timedated/geocoding UI is not compiled there;
- strict warnings remain errors.

Environment testing on a supported Mint/Cinnamon system must additionally prove:

- opening Date & Time causes no authentication prompt;
- selecting an IANA time zone changes the actual timedated `Timezone` property and external Cinnamon tools observe it;
- enabling/disabling Network time changes timedated `NTP` and the panel reconciles the resulting property notification;
- manual date/time is disabled while NTP is active and a protected manual change uses operation-scoped polkit authorisation;
- native 12/24-hour, panel-date, seconds and first-day-of-week settings agree with Cinnamon when changed from either interface;
- a locality query such as `Mooroopna` returns selectable named results when the geocoding service is reachable;
- selecting a locality publishes its coordinates to the temporal policy without pretending that the locality name is an operating-system time-zone identifier;
- closing the window or issuing a second search while a geocode request is outstanding cannot update destroyed/stale UI.

The Mint GTK3 map is presentation, not authority. Functional replacement does not require embedding that widget in the GTK4 shell; it requires preserving or improving every underlying system operation it exposed.

## External-change tests

For backends that provide notifications:

- change a setting outside System Settings;
- verify untouched visible state updates;
- begin a local edit, change authoritative state elsewhere;
- verify the local edit is not silently overwritten;
- verify Apply detects/reconciles stale state appropriately.

## Sanitizers and static quality

CI should include, as appropriate:

- strict compiler warnings;
- ASan;
- UBSan;
- leak/lifecycle stress where reliable;
- 32-bit compile checks for ABI/size assumptions if supported by dependencies;
- CTest/Make parity if both build front ends are retained;
- documentation generation with warnings treated as errors once source API docs exist.

## Manual target matrix

Before a stable release, maintain a tested environment matrix for every supported platform.

Linux records at least:

- Linux Mint release;
- Cinnamon/session type;
- kernel;
- relevant service versions;
- GPU/display environment for display testing;
- network hardware/service environment;
- Bluetooth/printing hardware where those modules are advertised.

Windows records at least:

- Windows release/build and edition/SKU where relevant;
- architecture;
- policy/elevation context;
- GPU/display environment;
- network, Bluetooth and printing hardware where advertised;
- native Settings handoff availability for delegated operations.

A virtual machine is valid evidence for generic shell behaviour but is not automatically evidence for hardware behaviour.

## Regression rule

Every fixed reproducible defect gains the narrowest useful permanent regression test unless the only proof requires physical/manual conditions. Manual-only regressions are recorded as explicit acceptance steps.

## Release criterion

A release commit must:

- pass all required automated gates;
- have no known mismatch between supported behaviour and documentation;
- include only modules whose advertised read/write contracts meet their evidence requirements;
- build release assets from the exact tested revision;
- retain immutable published tags/assets, with later fixes advancing the version.
