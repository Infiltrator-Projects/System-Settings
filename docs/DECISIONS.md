<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Decisions

This file records durable architectural choices for System Settings.

## ADR-001 — One shell, many focused modules

**Decision.** Present one System Settings application while implementing settings domains as independently owned modules.

**Rationale.** A single application provides discovery, search and consistency. Small modules preserve the maintainability and failure-locality strengths of classic preference/control-panel systems.

**Consequence.** The shell cannot accumulate domain-specific implementations merely because doing so is convenient.

## ADR-002 — Static manifests before executable module code

**Decision.** Navigation and search metadata are read from bounded manifests without loading module libraries.

**Rationale.** Global discovery should be fast and should not trigger device enumeration, service calls or arbitrary module initialisation.

**Consequence.** Searchable targets and basic module metadata must exist independently of the module UI.

## ADR-003 — Versioned C ABI at the dynamic boundary

**Decision.** The module boundary uses a versioned C ABI with opaque handles and explicit structure sizes.

**Rationale.** C linkage is stable across C/C++ implementation choices and avoids exposing compiler-specific C++ ABI details.

**Consequence.** C++ modules can use C++ freely internally but cannot expose STL objects, exceptions or implementation classes across the module boundary.

## ADR-004 — Lazy panel construction

**Decision.** A module's full implementation/UI is loaded only when needed.

**Rationale.** Opening settings should not wait for every hardware and service domain on the machine.

**Consequence.** Modules must support explicit lifecycle and cannot rely on application-long widget lifetime.

## ADR-005 — First-party trusted modules only at initial release

**Decision.** Initial in-process modules are installed in trusted system-owned locations; arbitrary user-installed shared-library modules are not loaded.

**Rationale.** A shared library executes with the shell's full user-process authority and is not a sandbox.

**Consequence.** A future third-party ecosystem requires a separately designed trust/isolation model, likely out of process.

## ADR-006 — The shell is not privileged

**Decision.** System Settings starts and operates as the normal user. Protected changes authorise the specific operation through the platform's native policy mechanism.

**Rationale.** Most settings do not require administrator authority, and broad elevation unnecessarily increases risk and creates poor user experience.

**Consequence.** Modules must understand permission-denied/cancelled states and cannot assume root access.

## ADR-007 — Native maintained interfaces before command utility output

**Decision.** Use authoritative libraries, D-Bus interfaces, kernel APIs and system-service contracts where practical instead of parsing or scripting command-line tools.

**Rationale.** CLI text and process invocation are generally weaker machine contracts than the APIs the utilities themselves use.

**Consequence.** A CLI tool is used only when evidence shows it is genuinely the strongest maintained interface for that operation.

## ADR-008 — Common owns generic infrastructure, modules own settings policy

**Decision.** Reusable project mechanisms converge on Common when Common is at least as strong as the local implementation. Settings-domain policy remains in System Settings.

**Rationale.** This avoids duplicated generic code without turning Common into a collection of application-specific backends.

**Consequence.** Presentation primitives may move to Common; NetworkManager policy does not.

## ADR-009 — Apply semantics follow risk and transaction shape

**Decision.** Do not impose either universal instant-apply or universal Apply buttons.

**Rationale.** A theme toggle and a multi-field static IP configuration have different correctness/recovery requirements.

**Consequence.** Each setting/domain documents whether it is immediate, staged, destructive-confirmed or rollback-confirmed.

## ADR-010 — Writes are verified against authoritative state

**Decision.** Where practical, a module reads back or observes the authoritative resulting state after a change.

**Rationale.** A successful request does not prove that policy, hardware or another actor left the requested value effective.

**Consequence.** UI success state follows confirmed effective state rather than optimistic local bookkeeping.

## ADR-011 — Search targets are logical identities

**Decision.** Search/deep-link identifiers refer to settings concepts, not widget indices or layout paths.

**Rationale.** UI refactoring must not unnecessarily break external links or search routing.

**Consequence.** Modules maintain stable target identifiers across presentation changes.

## ADR-012 — No shadow configuration database

**Decision.** System Settings stores shell preferences but does not duplicate operating-system settings into a project database as the source of truth.

**Rationale.** Configuration can change outside this program; shadow state inevitably drifts.

**Consequence.** Modules read authoritative backends and reconcile external changes.

## ADR-013 — Main is the normal development line

**Decision.** Normal development remains on `main`; long-lived feature branches are not part of the project workflow.

**Rationale.** The repository is maintained as one forward-moving product line.

**Consequence.** Every `main` change must remain coherent enough to build/validate once implementation exists, and releases advance from exact verified main revisions.


## ADR-014 — Specialised applications retain their domains

**Decision.** System Settings links or deep-links to established project applications instead of reimplementing the complete domains they already own.

**Rationale.** Software management, live monitoring and filesystem defragmentation are substantial products with their own engines, validation and safety contracts. Duplicating them inside Settings would create two authorities and eventually inconsistent behaviour.

**Consequence.** Infiltrator Software remains the owner of package/software/update workflows, System Monitor remains the owner of live monitoring, and Defragmenter remains the owner of filesystem defragmentation/recovery. Settings may own configuration adjacent to those domains and provide intentional launch/deep-link entry points.


## ADR-015 — Canonical data is separate from system-wide presentation policy

**Decision.** System Settings owns user-wide presentation preferences, while applications retain canonical domain data and Common performs shared presentation transforms.

**Rationale.** A preference such as decimal 10-hour time should affect filesystem timestamps, histories and other human-visible times consistently without rewriting stored timestamps, breaking sort order, changing protocol values or requiring each application to implement the clock system independently.

**Consequence.** Human-visible temporal formatting migrates toward Common Temporal. Applications classify values as instants, civil date/time, calendar dates, durations or machine/export timestamps. Clock/calendar presentation applies only where semantically appropriate. Future cross-application presentation families may reuse the same architecture.

See [PRESENTATION_POLICY.md](PRESENTATION_POLICY.md).


## ADR-016 — Mint/Cinnamon compatibility at the edges, independence at the core

**Decision.** System Settings reuses an existing Mint/Cinnamon authority when it represents the same semantic setting, creates an Infiltrator extension only when the concept cannot be represented faithfully upstream, and uses a compatible fallback only where that fallback is safe and unambiguous.

**Rationale.** Duplicating ordinary desktop settings would create conflicting sources of truth, while restricting System Settings to only what Mint currently supports would prevent new system-wide policies such as decimal 10-hour time. A compatibility-adapter boundary permits both interoperability and extension.

**Consequence.** Modules identify the authoritative owner before implementation. Existing desktop settings are read/written through platform adapters; Infiltrator extensions retain their own semantics; optional integrations may teach selected Mint applications to consume extensions without requiring full upstream forks.

See [MINT_COMPATIBILITY.md](MINT_COMPATIBILITY.md).


## ADR-017 — Mint/Cinnamon and Windows are first-class from the first executable

**Decision.** The shell, semantic modules, manifest format, module ABI, Common presentation-policy interface and validation strategy are designed for Linux Mint/Cinnamon and Windows from Phase 1 rather than porting a completed Linux implementation later.

**Rationale.** Platform assumptions become expensive once they enter ABI, UI ownership, dynamic loading, privilege and persistence contracts.

**Consequence.** Phase 1 CI/builds include both platforms and at least one module/fixture through the same semantic ABI. Mint may receive broader feature coverage first, but Windows portability is not deferred architecturally.

## ADR-018 — Toolkit/native UI objects do not cross the module ABI

**Decision.** Public module contracts cannot expose GTK widgets, HWND values, WinUI objects or other platform-specific presentation types.

**Rationale.** Exposing one toolkit in ABI v1 would bind every module to that platform/runtime.

**Consequence.** Modules communicate through semantic state and a platform-neutral host/presentation abstraction. Platform-specific UI remains behind that boundary.

## ADR-019 — Documented platform authority before private storage

**Decision.** Platform adapters use documented operating-system/desktop APIs or documented native-settings handoffs before storage-level mechanisms such as private registry keys or private desktop configuration.

**Rationale.** Private storage can bypass platform policy, notifications, validation and migration logic.

**Consequence.** If Windows exposes only a supported Settings destination for a workflow, System Settings may hand off rather than reverse-engineering Settings internals. Equivalent discipline applies to Mint/Cinnamon.


## ADR-020 — A time zone is regional evidence, not a physical-location assertion

**Decision.** System Settings may use the operating-system IANA time zone to seed an approximate geographic reference, but it never silently treats that reference coordinate as the user's actual physical location.

**Rationale.** A time zone such as `Australia/Melbourne` spans a large area. The tzdata coordinate identifies a representative reference for the zone, not the machine's position. Conflating the two would create false precision and would make future Language & Region work architecturally ambiguous.

**Consequence.** Geographic location has an explicit source: absent, time-zone reference, or user-selected/custom. The system time zone remains authoritative for civil-time rules. A future locality chooser may resolve a named place such as a town or suburb to coordinates, but that resolution is a separate location operation and must not rewrite language, regional formats or time zone unless the user explicitly requests it.


## ADR-021 — Date & Time replaces the Mint frontend, not the native authorities

**Decision.** The System Settings Date & Time module is the project-owned frontend for the complete supported Mint/Cinnamon Date & Time workflow. It directly edits the same native authorities Mint edits for system time zone, network time, manual clock setting and conventional desktop format preferences. Infiltrator-only clock/calendar/geographic presentation remains an extension beside those native values.

**Rationale.** A replacement settings application is incomplete if it merely displays the operating-system value or asks the user to keep using the old Mint panel for ordinary operations. Conversely, inventing parallel storage for values already owned by timedated or Cinnamon would create conflicting sources of truth.

**Consequence.** On current systemd-based Mint systems, System Settings uses `org.freedesktop.timedate1` for the real system time zone, NTP state and manual clock changes, and `org.cinnamon.desktop.interface` for the native format settings used by Cinnamon. The GUI remains unprivileged; timedated/polkit owns operation-scoped authorisation. The old Mint panel is no longer required for these supported operations.

The world-map widget itself is not an authority. The replacement may use a searchable IANA selector and named-locality search instead of embedding Mint's GTK3-only `libtimezonemap` widget inside the GTK4 shell, provided all underlying system capabilities remain reachable.

Named locality is distinct from time-zone identity. A locality search result supplies geographic coordinates for location-dependent presentation and may propose/apply the nearest plausible same-country IANA zone, but the explicit system time-zone selector remains authoritative and user-correctable.


## ADR-022 — System Settings is the control authority; native settings are backends

**Decision.** For settings that Linux/Cinnamon can represent exactly, System Settings exposes the user-facing choice and reads/writes the native platform authority rather than presenting the native source as a competing mode.

**Rationale.** A replacement settings application should not ask the user to choose between "System Settings" and "the OS" when both names refer to the same underlying preference. Doing so creates duplicate authorities and confusing states such as a visible `Standard time (OS locale)` beside explicit 12-hour and 24-hour choices.

**Consequence.** Explicit conventional clock choices map to Cinnamon/GNOME's native 12/24-hour values. External changes to that native value are reconciled back into the equivalent explicit System Settings choice while a conventional mode is active. Extended clock systems retain their System Settings/Common policy; the native 12/24-hour value beneath them is only a compatibility fallback for software that cannot understand the richer policy.

The Common identifier `standard` remains valid internally as a bootstrap/fallback for consumers that must operate without an installed System Settings authority. It is not a third user-facing conventional clock choice when System Settings is present.

## ADR-023 — Built-in module extraction before freezing the public ABI

**Decision.** Date & Time domain implementation is removed from the generic Linux shell now and built as a separate first-party module target. The current shell-to-panel bridge is private and GTK-specific; it is not the promised public module ABI.

**Rationale.** Keeping a 2,000-line domain implementation in the shell violated ADR-001 and made every new settings page more likely to deepen the monolith. Conversely, forcing the first refactor through an unfinished dynamic ABI would freeze toolkit assumptions prematurely.

**Consequence.** `src/shell/linux/main.c` owns lifecycle, common framing and navigation only. Date & Time owns its widgets, callbacks, backend coordination, async work and cleanup in `src/modules/date-time/linux-date-time-panel.c`. CMake rejects selected Date & Time backend/model symbols if they reappear in the generic shell. A later phase can wrap this module behind the versioned toolkit-neutral ABI without moving the domain policy back into the shell.
