<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Changelog

All notable user-visible and architectural changes are recorded here.

## Unreleased

No unreleased changes.

## 0.4.1 — 2026-09-24

- Align the Linux System Settings shell a little more closely with the shared Infiltrator desktop language without changing its architecture or adding dependencies.
- Match Software's 248 px navigation width, add the suite-style 3 px cyan selected-navigation marker, and reduce the main page title from 30 px to 28 px.
- Keep the change intentionally narrow: no new Common API, toolkit, framework, helper library or external runtime dependency.

## 0.4.0 — 2026-09-24

- Rework the Linux System Settings interface from the original functional prototype into a quieter graphite settings shell with application identity and About access integrated into the persistent sidebar instead of a competing full-width header.
- Replace the five equally prominent Date & Time cards with a deliberate hierarchy: one live presentation hero, one full-width Location & time zone workflow, and responsive Presentation/System clock groups.
- Merge clock, calendar, seconds, panel-date and week-start controls into one Presentation surface; keep NTP/manual protected clock mutation together in System clock.
- Combine latitude/longitude into one advanced Coordinates row, shorten control-surface copy, reduce oversized dropdowns and make locality search/manual Set the only primary-accent actions.
- Replace per-section coloured edge stripes with restrained Common graphite surfaces, selection/focus accents and clearer section icon/title/summary hierarchy.
- Use a wrapping GTK FlowBox for the two lower setting groups so the layout naturally falls back to one column on narrower windows instead of forcing horizontal overflow.
- Preserve every existing Date & Time backend, signal, policy and persistence path; this release is an interface restructuring rather than a settings-semantics change.

## 0.3.19 — 2026-09-24

- Raise source-level documentation to the same standard as the project's architecture documents: document Date & Time persist-before-publish semantics, borrowed/owned state and platform-store fallback contracts.
- Document timedated asynchronous ownership, cancellation/completion behaviour, absolute microsecond clock writes and the rule that privilege remains with timedated/polkit rather than the GUI.
- Document Calendar runtime capability binding, destructive rebinding, lazy discovery throttling and cached chronology lifetime without adding redundant line-by-line commentary.
- Document locality metadata durability/privacy, geocoding result ownership, tzdata ISO-6709 coordinate grammar and Linux/Windows temporal-policy recovery/publication invariants.
- This release intentionally changes documentation/comments only; runtime behaviour is unchanged.

## 0.3.18 — 2026-09-22

- Isolate Windows temporal-policy file I/O behind a private path-based adapter so recovery behaviour can be regression-tested without touching the real user profile.
- Move the Windows persistence regression to a unique temporary directory; local CTest runs can no longer overwrite or delete a developer's real `%LOCALAPPDATA%\\Infiltrator\\presentation.conf`.

## 0.3.17 — 2026-09-22

- Pin Infiltratr Common 1.19.24, adopting the hardened temporal-policy parser, bounded POSIX temporal document reads and safer atomic persistence path introduced after 1.19.20.
- Make Windows temporal-policy loading recover from empty, oversized and malformed per-user policy files in the same way as Linux instead of preventing Date & Time initialisation; add a native Windows regression test for missing, corrupt, valid and saved policy states.
- Preserve the authoritative current timedated zone in the Linux selector even when it is a valid tzdata alias omitted from zone.tab/zone1970.tab, with deterministic alias regression coverage.
- Route conventional 12/24-hour mirroring through the canonical Cinnamon/GNOME compatibility writer instead of bypassing its GNOME clock-format update.
- Construct the timedated proxy asynchronously, defer the first Calendar runtime preview until the GTK event loop can present the window, remove the synchronous constructor from the module API, and eliminate broad library-root subdirectory scanning from preview discovery.
- Generation-gate timezone, NTP and manual-clock writes so an older cancelled D-Bus completion cannot overwrite status or reconciliation from a newer user operation.
- Extend the Calendar runtime fixture to prove missing-runtime recovery after the preview provider is already alive, while keeping specialised clock and chronology capabilities optional.
- Reuse Common's deterministic ASCII case-insensitive comparison for manual-time parsing instead of maintaining a private duplicate.
- Split Date & Time GTK construction into a dedicated UI translation unit so widget construction is no longer mixed into the backend/policy lifecycle file.
- Add an Ubuntu ASan/UBSan CI gate alongside the existing Linux/Windows strict-warning builds and Debian package validation.

## 0.3.16 — 2026-09-22

- Fix all specialised Date & Time previews being reported as unavailable after opening System Settings. The Calendar preview provider is now created with the panel instead of waiting for an unrelated timedated property-change signal.
- Keep one Calendar preview provider for the complete panel lifetime. Clock-list construction no longer destroys it, timedated changes no longer replace and leak it, and panel teardown now releases it explicitly.
- Route every Calendar-owned clock and every non-Gregorian calendar preview through the same lazy safety guard, preserving the existing five-second runtime rediscovery path if Calendar is installed or upgraded while System Settings remains open.

## 0.3.15 — 2026-09-21

- Fix Calendar runtime discovery for the Date & Time live preview. The bridge now prefers trusted system library locations, including Debian/Mint multiarch directories, instead of relying only on the process loader finding `libcalendar-plus.so.0` by bare soname.
- Retry Calendar runtime discovery while System Settings remains open, so installing or upgrading Calendar no longer leaves specialised clock/calendar previews permanently unavailable until the settings application is restarted.
- Treat specialised clock and chronology capabilities independently, so a missing chronology entry point cannot unnecessarily disable an otherwise valid clock-preview runtime, and vice versa.
- Add an executable runtime-bridge fixture test covering Internet Time and Positivist date rendering through the same dynamic ABI used by the real application.

## 0.3.14 — 2026-09-21

- Fix the Date & Time live preview for specialised clock systems. Internet Time, Roman temporal time, sidereal/solar clocks and the other Calendar-owned modes now use Calendar's actual formatter instead of displaying the clock-mode name as if it were a value.
- Fix non-Gregorian calendar preview. Positivist and every other supported calendar now render today's date through Calendar's chronology engine instead of showing a Gregorian date with a "selected calendar" label.
- Add a small optional runtime bridge to Calendar's versioned C ABI. System Settings still owns policy while Calendar remains the implementation owner of specialised clock and chronology algorithms; no duplicate algorithms were copied into System Settings.
- Keep System Settings usable without Calendar installed. Conventional 12/24-hour and decimal previews remain available from Common; specialised previews explicitly report unavailable instead of fabricating a value when the Calendar runtime is absent.

## 0.3.13 — 2026-09-21

- Extract the complete Linux Date & Time implementation from the generic application shell into a dedicated first-party module target; `main.c` now owns application lifecycle, common framing and navigation rather than timedated, locality, temporal-policy and Date & Time widget logic.
- Add a private built-in Linux module bridge and shared Linux UI helpers without exposing GTK through the future public module ABI.
- Add a configure-time architecture guard that rejects Date & Time model/backend ownership if it leaks back into the generic Linux shell.
- Stop presenting Common's internal `standard` bootstrap identifier as a third conventional clock choice. System Settings now exposes explicit 12-hour/24-hour systems while retaining `standard` internally for consumers that must operate without the settings authority.
- Reconcile external Cinnamon 12/24-hour changes into the equivalent explicit System Settings conventional clock choice, while preserving extended clock systems such as decimal or sidereal.
- Reconcile external Cinnamon seconds changes into the shared temporal policy instead of allowing native and Common-aware applications to drift.
- Keep persisted locality metadata aligned with the actual system IANA zone when that zone changes, while preserving deliberate geographic coordinates.
- Add unit coverage for native/conventional clock-policy mapping and document the control-authority versus native-backend model.

## 0.3.12 — 2026-09-21

- Remove the redundant user-facing `Use 24-hour clock` switch from Desktop format. Explicit 12-hour and 24-hour presentation are already first-class Clock system choices, so exposing the Cinnamon compatibility Boolean as a second control created two UI authorities for the same choice.
- Keep Cinnamon/GNOME's conventional 12/24-hour setting only as a compatibility backend: selecting the explicit Standard 12-hour or Standard 24-hour clock mode mirrors the matching native desktop value, while richer clock modes do not pretend to have a conventional equivalent.
- Retain `Standard time (OS locale)` as the locale/native conventional mode; it reads the native desktop convention rather than adding another visible toggle.

## 0.3.11 — 2026-09-21

- Hide manual date/time controls completely while Network time is enabled instead of leaving inactive-looking manual fields visible beside an authoritative NTP source.
- Reorder Date & Time so location and the IANA time zone form one coherent settings card; a named locality drives coordinates and normally the matching zone, while an explicit zone remains available for correction.
- Preserve the distinction between a precise named/custom location and a time-zone-derived regional reference. When location is still only the system reference, changing the real system zone moves the reference coordinates with it rather than leaving stale Melbourne coordinates behind.
- Move Network time/manual setting below the selected Clock and Calendar systems so the source controls follow the presentation choices they govern.
- Make reversible manual time entry follow Standard 12/24-hour or French Republican decimal clock presentation. Decimal input is converted exactly back to conventional microseconds before calling timedated.
- Refuse to expose a misleading manual editor for clock/calendar combinations that cannot yet be safely converted back to a canonical system instant; Gregorian plus Standard/12h/24h/decimal is currently reversible and covered by unit tests.

## 0.3.10 — 2026-09-21

- Turn Date & Time from a presentation-only companion into a functional replacement for Mint/Cinnamon's current Date & Time panel on the supported systemd/timedated path.
- Replace the read-only operating-system time-zone display with a searchable IANA time-zone selector backed by installed tzdata; changes are applied to Linux itself through `org.freedesktop.timedate1` and remain visible to Cinnamon and ordinary applications.
- Add native Network time control and protected manual date/time setting through asynchronous timedated D-Bus calls, leaving the GUI unprivileged and delegating authorisation to the operating system.
- Remove the geographic-location `Not selected`/source-mode dropdown. The system time zone now always provides the initial geographic approximation until the user refines it.
- Add asynchronous named-locality search through geocode-glib/Nominatim. Queries such as `Mooroopna` return named matches and coordinates; choosing one stores the selected locality/coordinates for location-dependent Common clocks and applies the nearest same-country IANA zone as a visible, editable system-time-zone suggestion.
- Retain latitude/longitude as advanced overrides rather than the primary geographic interface.
- Recreate Mint's native format controls in the same System Settings page: 12/24-hour clock, panel date visibility, seconds compatibility, and first day of week. Native Cinnamon/GNOME settings remain the source of truth where they already exist.
- Harden asynchronous locality searches against stale completion after cancellation/window close and reconcile external timedated/GSettings changes while the panel is open.
- Add the required geocode-glib dependency to Linux CI and release packaging; Linux and Windows CI remain mandatory before release.

## 0.3.9 — 2026-09-21

- Separate operating-system time-zone identity from physical geographic location: a configured time zone is now treated as regional evidence, never silently promoted to the user's exact location.
- Detect the full IANA time-zone identifier on Linux and read its representative coordinate from the installed tzdata `zone1970.tab`/`zone.tab` database when available.
- Replace the geographic-location on/off switch with an explicit source selector: no location, system time-zone reference, or custom coordinates.
- Seed custom coordinates from the system regional reference instead of the misleading 0°,0° origin, while keeping the values editable to locality-level precision.
- Display the full system zone such as `Australia/Melbourne` rather than only the transient abbreviation such as AEST/AEDT.
- Add deterministic parser/lookup regression coverage and document the longer-term Language & Region/locality-search architecture.

## 0.3.8 — 2026-09-21

- Replace the inherited Mint `preferences-system` launcher icon with a first-party System Settings icon using the same graphite, black and cyan visual language as the other Infiltrator desktop applications.
- Install the scalable icon under the freedesktop hicolor application-icon path and bind the desktop launcher and project identity to `org.infiltrator.SystemSettings`.
- Extend Debian-package validation so CI verifies both the installed icon asset and the desktop-file icon identity, preventing a regression to the generic system icon.

## 0.3.7 — 2026-09-21

- Repair GTK 4 switch rendering by replacing the broad widget background override that could hide the slider and reduce disabled switches to solid dark rectangles.
- Give switches, drop-downs and spin controls explicit Common-aware input, border, hover, focus, checked and disabled states so controls remain legible in both Day and Night palettes.
- Use more of Common's semantic colour roles without inventing product-local colours: accent for the live preview and clock controls, information for calendar, warning for geographic settings, and success for operating-system time.
- Add coloured section edges and selected-navigation accenting to improve hierarchy while retaining the established graphite System Settings layout.

## 0.3.6 — 2026-09-21

- Finish the Common-to-System-Settings design pass by consuming Common's canonical regular/bold typography weights instead of hard-coding the current numeric weight.
- Replace exact duplicated titlebar, page-summary and card spacing literals with Common's control, content, compact and section spacing metrics.
- Keep component-specific GTK geometry local where Common does not define an equivalent semantic role.

## 0.3.5 — 2026-09-21

- Consume Common 1.19.20's strict locale-independent ranged double parser for command-line geographic coordinates instead of libc `strtod()`.
- Use Common's canonical temporal catalogue IDs and bounded string copy in the Date & Time model and Linux Cinnamon compatibility adapter instead of private length/copy logic.
- Use Common's allocation-free deterministic ASCII matching for GTK dark-theme detection and Common's semantic success/fault palette roles for status presentation.
- Adopt Common's canonical `InfiltratrProjectInfo` and build-profile vocabulary as System Settings' single runtime identity source; published packages identify as `Generic / APT package` while source builds identify as `Source / CMake build`.
- Add release-build-safe identity regression coverage on Linux and Windows and document the strengthened Common ownership boundary.

## 0.3.4 — 2026-09-21

- Pin Common 1.19.20 with validated temporal-provider identity, symmetric coordinate validation and allocation-backed POSIX policy paths.
- Recover from malformed or empty temporal policy by returning to Mint/Cinnamon defaults instead of refusing to open Date & Time.
- Share one Cinnamon temporal-settings adapter between persistence and the GTK shell, and reconcile external 12/24-hour and seconds changes while Mint remains authoritative.
- Make the Standard clock preview follow Cinnamon's actual 12/24-hour preference and stop labelling Gregorian preview text as though it were a converted non-Gregorian date.
- Add Linux policy-store regression coverage for corrupt-policy recovery and valid-policy reload.

## 0.3.3 — 2026-09-21

- Pin the final Common 1.19.19 release revision containing the shared POSIX temporal authority contract and release-build-safe contract test.
- Retain the explicit temporal-v3 provider marker, shared atomic policy store and Mint/Cinnamon compatibility behaviour introduced in 0.3.2.

## 0.3.2 — 2026-09-21

- Publish the explicit `temporal-v3` provider capability marker so consumers can detect the installed System Settings authority without probing executable names or PATH.
- Replace the Linux Date & Time module's duplicated XDG path, policy parsing and atomic-write code with Common 1.19.19's shared POSIX temporal store.
- Preserve Mint/Cinnamon as the initial/default authority until the user actually saves an Infiltrator temporal policy.

## 0.3.1 — 2026-09-21

- Seed the first Linux Date & Time policy from Cinnamon's native 12/24-hour and seconds preferences when no Infiltrator policy has been saved yet.
- Mirror exact conventional choices back to Cinnamon when System Settings saves them, keeping stock Mint consumers aligned without pretending richer Infiltrator clocks or calendars have a native equivalent.
- Keep the Infiltrator policy optional for consumers so Calendar remains a standalone Mint replacement before System Settings is installed or configured.
- Document the native-fallback/enrichment boundary as a durable Mint/Cinnamon compatibility contract.

## 0.3.0 — 2026-09-20

- Moved Date & Time to Common 1.19.18 temporal policy v3.
- Removed the retired secondary-calendar feature from the model, GUI, CLI, search manifest and persisted policy.
- Replaced primary/secondary terminology with one authoritative system Calendar setting.
- System Settings now writes `calendar=...` alongside clock mode, seconds and location; current consumers read that policy directly.
- Removed the consumer-side “Follow System Settings” model from the architecture: following System Settings is the normal contract, not an optional override mode.
- Retained private migration of existing v2 presentation.conf data so the previous primary calendar becomes the single v3 calendar while the retired secondary value is discarded.
- Preserved Linux and Windows persistence with the same one-calendar policy contract.

## 0.2.0 — 2026-09-20

- Replaced the four-profile bootstrap model with Common 1.19.16 temporal policy v2.
- Made System Settings the authority for the complete temporal environment rather than only a clock-format preference.
- Added all 21 shared clock systems, all 30 primary calendars, optional secondary calendar, seconds policy and geographic latitude/longitude to Date & Time.
- Removed the global self-referential “Follow system” clock concept; the global conventional default is Standard time (OS locale).
- Added deterministic v1-to-v2 policy migration through Common and retained one authoritative per-user presentation.conf store on Linux and Windows.
- Updated the Linux GTK4 panel, CLI, model tests and Calendar integration contract to use the same stable clock/calendar identifiers.

## 0.1.0 — 2026-09-20

### Architecture

- Established System Settings as one unified shell over focused first-party settings modules.
- Defined static manifest discovery so navigation and search do not require loading every module.
- Chosen a versioned C ABI for the future native module boundary while retaining C/C++ freedom inside modules.
- Defined lazy module/panel loading and first-paint-oriented startup.
- Defined trusted system-owned module locations; arbitrary user-writable in-process plugins are not part of the initial model.
- Defined operation-scoped authorisation with an unprivileged shell.
- Defined authoritative-state read-back/reconciliation after settings writes.
- Defined logical deep-link/search identities independent of widget layout.
- Defined Common as shared presentation/infrastructure while keeping settings-domain policy local.
- Added the initial phased roadmap and validation/security requirements.
- Defined integration boundaries so Software, System Monitor and Defragmenter retain ownership of their specialised domains while Settings provides launch/deep-link entry points where appropriate.
- Defined system-wide presentation policy: System Settings owns the user's policy, Common provides shared formatting, and applications retain canonical data. Temporal Presentation is the first family, including the planned decimal 10-hour clock model.
- Defined Mint/Cinnamon compatibility policy: reuse existing authoritative desktop settings, add Infiltrator extensions only for missing semantics, preserve safe conventional fallbacks, and avoid unnecessary upstream forks.

- Promoted Windows from a future portability concern to a first-class target from Phase 1.
- Added shared platform-compatibility and Windows-specific compatibility contracts.
- Prohibited GTK, HWND, WinUI and other platform UI objects from the public module ABI.
- Generalised trusted module loading to Linux shared objects and Windows DLLs.
- Added native Windows Settings handoff as a supported integration mode where no safe public write API exists.
- Generalised validation, privilege and security contracts for Linux and Windows.

### Implementation

- Added the first native GTK4 System Settings shell on Linux with a visible Date & Time panel.
- Added live clock/date preview, system/12-hour/24-hour/decimal-10 selection, seconds control and current time-zone display.
- Added desktop integration and install rules so the shell appears as System Settings after installation.

- Added the first Date & Time semantic module model and static module manifest.
- Added portable system/12-hour/24-hour/decimal-10 policy selection through Common 1.19.14.
- Added Linux XDG and Windows LocalAppData per-user persistence adapters.
- Added Linux and Windows CI for the first executable integration slice.
- Added a temporary `system-settings-time` integration utility for exercising the policy before the full shell UI exists.
- Kept platform persistence injected behind the model so the core has no Linux/Windows dependency cycle.
- Began Calendar integration as the first consumer of the system temporal policy.

### Status

0.1.0 was the first bootstrap binary release; 0.2.0 replaces its limited temporal model with the complete authority described above.
