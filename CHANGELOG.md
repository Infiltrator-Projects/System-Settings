<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Changelog

## 0.4.34 — 2026-09-26 — remove mountain hero halo

- Remove the cyan outer box-shadow from the full-width mountain hero image on
  the Home dashboard.
- Preserve the hero border, raster artwork, overlays and internal styling; only
  the unwanted glow outside the mountain image is removed.

## 0.4.33 — 2026-09-26 — finish temporal selector clarity audit

- Pin System Settings to released Infiltratr Common 1.19.36 at `5e129851bbd7ac0b94bd8c2f48f32924016bdbda`.
- Replace specialist shorthand in ancient/historical clock and calendar selectors with plain-language descriptions of the actual model, epoch or day boundary.
- Keep all persisted temporal IDs and chronology behaviour unchanged; this is a clarity/accuracy-of-description release.

## 0.4.32 — 2026-09-26 — complete temporal clarity audit

- Pin System Settings to released Infiltratr Common 1.19.35 at `7cc5de3de0e94ed2cfcff0840bbb5346eb5c9c9f`.
- Expose clearer model/epoch labels for the full clock and calendar catalogue, especially ancient and historical systems.
- Consume native elapsed hierarchies for Chinese shíchén, Edo unequal hours and ancient Babylonian ūmu/bēru/UŠ.

## 0.4.31 — 2026-09-26 — audited temporal models

- Pin System Settings to released Infiltratr Common 1.19.34 at `1467755d088d740b873660a8f0c9a515e9a39046`.
- Present historically distinct early-Edo sunrise/sunset and late-Edo 1797 twilight clocks as separate choices rather than one ambiguous Edo mode.
- Use clearer clock/calendar labels that expose approximations, continuation rules, computational models and bounded data ranges where they materially affect interpretation.

## 0.4.30 — 2026-09-26 — remove redundant modern Italian mode

- Pin System Settings to released Infiltratr Common 1.19.32 at `ba9386fad1944d3e575a28346a46f85108326051`.
- Remove the duplicate Modern Italian civil-time entry from the clock-system selector; standard 24-hour time already represents modern Italian civil time.
- Retain Historical Italian hours only for the sunset-origin historical clock system.

## 0.4.29 — 2026-09-26 — separate historical clock systems

- Pin System Settings to Infiltratr Common 1.19.31 at `fd51905f3cea1f051e1163fbdeea8b62b7069833`.
- Add an explicit Modern Italian civil-time choice, leaving historical Italian hours clearly labelled as the sunset-origin 24-hour system.
- Separate Renaissance European “Babylonian hours” from ancient Babylonian bēru/UŠ timekeeping.
- Split the historical Nürnberg Great Clock with its fixed Wendetage from the location-aware Nuremberg-style solar reconstruction.

## 0.4.28 — 2026-09-26 — clarify and correct historical clocks

- Pin the Date & Time clock catalogue to released Infiltratr Common 1.19.30 at `9a9fae5b3f0d133d400310cdd316b21129631429`.
- Rename the old Italian option to make clear that it is the historical 24-equal-hour sunset-origin system, not modern Italian civil time.
- Rename the sunrise-origin European "Babylonian hours" option as the Renaissance convention and add a separate Ancient Babylonian seasonal-hours option with twelve daylight and twelve night simānu.
- Correct Nuremberg time to the discrete Wendetag day/night allocation instead of resetting its equal-hour count at every actual sunrise and sunset.
- Modern Italian time remains ordinary standard civil time under the Europe/Rome time zone rather than a duplicate clock system.

## 0.4.27 — 2026-09-26 — historical time hierarchy correction

- Pin the System Settings Common submodule to released Infiltratr Common 1.19.29 at `13b824e4c4e266426590d86597249a89d18ec2f0`.
- Present Chinese hundred-kè time as native `NN刻` rather than redundant `刻 NN/100` notation.
- Supply the corrected shared Roman elapsed-duration hierarchy to Common-aware consumers, where complete seasonal day/night cycles collapse into `dies` before residual horae, vigiliae and unciae.
- Keep the generic hundred-kè mode at kè precision rather than fabricating a dynasty-specific finer subdivision.

## 0.4.26 — 2026-09-25 — remove hero logo halo

- Remove the cyan outer glow from the top-right Infiltrator OS hero-brand panel.
- Keep the dark translucent panel, artwork and border intact so the mark remains
  crisp against the hero image without looking like a separate light source.

## 0.4.25 — 2026-09-25 — correct header control order

- Correct the title-bar trailing controls to the conventional visual order:
  Search, Minimize, Maximize/Restore, Close.
- Replace independent GtkHeaderBar trailing packing with one explicit ordered
  container so GTK cannot reverse the apparent control sequence.
- Add a Linux shell regression that verifies the exact child order of the
  search field and three window controls.

## 0.4.24 — 2026-09-25 — live Home clock

- Make the Home Date & Time clock a genuinely live display instead of a
  construction-time snapshot.
- Refresh the Home clock, selected-calendar date and System Overview timestamp
  from the authoritative temporal policy every 250 ms, fast enough to represent
  French Republican decimal seconds without appearing frozen.
- Bind the timer lifetime to the Home scroller so closing System Settings
  removes the GLib source cleanly and cannot leave a callback targeting dead GTK
  widgets.
- Extend the Linux shell regression to require the live Home temporal source.

## 0.4.23 — 2026-09-25 — Home temporal policy and hero spacing polish

- Route the Home dashboard clock, date and System Overview timestamp through
  the Date & Time module's authoritative temporal-policy presentation bridge
  instead of formatting them independently with GLib civil-time defaults.
- Make French Republican decimal time and every other explicit Common clock
  mode render on Home using the same system-wide policy selected in Date & Time,
  including seconds and location-aware modes.
- Make the Home date follow the selected calendar; Gregorian remains native
  locale presentation while alternate calendars use the Calendar runtime
  bridge with an explicit unavailable state rather than silently showing the
  wrong chronology.
- Tighten the Simple / Secure / Beautiful feature strip: reduce vertical
  pressure in the hero, give all three cards equal fill behaviour, centre their
  icon/text groups and regularise inter-card and internal spacing.
- Add a deterministic regression test proving the Home presentation renders
  French decimal noon as 5:00:00 and conventional 24-hour noon as 12:00.

## 0.4.22 — 2026-09-25 — uploaded raster assets wired correctly

- Replace the temporary/generated JPEG and SVG placeholder artwork with the nine
  full-resolution PNG assets uploaded after the 0.4.21 tag.
- Rename the uploaded ChatGPT-generated files to stable runtime names and map
  them in generation order to hero, overview, date/time, region, network,
  Light, Dark, Follow OS and Mercedes Grey presentation slots.
- Point the native GTK shell and package install rules at the PNG artwork so the
  actual raster assets are displayed instead of the old placeholders.
- Remove the obsolete placeholder JPEG/SVG files and the generic upload names
  from the runtime asset directory.
- Keep Cairo/CSS rendering only as a missing-asset fallback rather than the
  normal visual path.


## 0.4.21 — 2026-09-25 — dedicated north-star artwork

- Replace the procedural SVG dashboard illustrations with nine independent
  raster artwork assets taken from the approved north-star visual language.
  The application does not ship or display a screenshot of the complete
  prototype: hero, overview, date/time, region, network and each appearance
  preview are separate runtime files.
- Wire the native GTK shell directly to the new JPEG artwork while retaining
  the existing live widgets, system data, navigation, window controls and
  scrolling behaviour.
- Make Date & Time use its own dedicated city/night artwork instead of falling
  through to the generic Cairo scene.
- Keep the Cairo/vector renderers only as an emergency missing-asset fallback;
  packaged Linux builds now install the photographic artwork as the normal
  presentation path.
- Remove the temporary asset-generation workflow and script. They depended on
  the old committed WebP north-star file, which is not a reliable decodable
  source and must not be part of the release path.

## 0.4.20 — 2026-09-25 — runtime visual-asset finish pass

- Add a packaged native SVG visual set derived from the approved north-star
  composition instead of relying on the deliberately simple fallback Cairo
  sketches for the primary Home dashboard.
- Give the Home hero, System Overview, Date & Time, Region & Language and
  Network cards dedicated high-detail visual artwork while preserving the Cairo
  fallback when runtime assets are unavailable.
- Replace flat theme swatches with miniature graphical desktop/window previews
  for Light, Dark, Follow OS and Mercedes Grey.
- Add the north-star-style Check for updates affordance to System Overview and
  wire it to the real Infiltrator Software application.
- Reduce the heavy hero text panel to a translucent gradient and add deeper
  blue/grey card gradients and subtle glow without changing native controls.
- Install the visual pack under /usr/share/infiltrator/system-settings/ui.

## 0.4.19 — 2026-09-25 — north-star composition pass

- Recompose the Home hero as one full-width visual surface with the scenic
  artwork behind the welcome copy, feature chips and Infiltrator OS identity,
  matching the north-star hierarchy instead of keeping text and art in separate
  halves.
- Strengthen the navigation rail with larger icon wells, cyan/gold glow,
  tighter spacing and a vivid selected-state gradient.
- Give all four Quick Actions larger icon wells, directional affordances and
  cyan/gold visual identities instead of flat generic buttons.
- Rebuild the Date & Time dashboard card around the real saved locality metadata
  used by the Date & Time module: locality name, country, coordinates and the
  local time are now shown directly on Home alongside a visual scene.
- Present the System Overview OS line as Infiltrator OS over the underlying
  Linux distribution and show a full human-readable system date/time.
- Enrich Australian Region & Language presentation with currency and metric-unit
  context while keeping the live locale as authority.
- Preserve the repaired scrolling/window controls from 0.4.18 and keep all
  visual changes inside the real native GTK program.

## 0.4.18 — 2026-09-25 — window controls and scrolling repair

- Replace the unreliable implicit title-bar minimize/maximize controls with
  explicit GTK buttons wired directly to minimize, maximize/restore and close.
- Make the long navigation rail independently scrollable so Software & Updates,
  About and later categories remain reachable on shorter displays.
- Make Home and Date & Time use non-overlay GTK scrollbars, kinetic scrolling,
  full expansion and a natural-height Home child so wheel/trackpad and scrollbar
  scrolling work reliably instead of clipping the lower dashboard.
- Add visible scrollbar styling consistent with the cyan/gold system palette.
- Extend Linux shell qualification to require all three window controls and all
  three scrollers, including non-overlay scrolling.

## 0.4.17 — 2026-09-25 — graphical dashboard content pass

- Replace the generic hero monitor block with a real programmatic scenic
  dashboard illustration: sunset sky, mountains, water reflection and tree
  silhouette, rendered natively with Cairo rather than a static mockup image.
- Enrich System Overview with its own visual thumbnail plus live uptime and
  system time, removing more of the spreadsheet-like feel.
- Recompose Date & Time around a large clock plus the real local time-zone
  identifier instead of leaving a large empty card.
- Give Region & Language a graphical Australian flag treatment on en_AU systems,
  with human-readable Australia / English (Australia) presentation.
- Add four visual theme-preview tiles to Display & Appearance while retaining
  the real current GTK theme and light/dark state.
- Add a live network visualization driven by GIO connectivity state.
- Remove the accidental vertical expansion that made the lower dashboard cards
  look mostly empty, so the Home page becomes denser and closer to the committed
  north-star composition.

## 0.4.16 — 2026-09-25 — full graphical navigation rail

- Expand the left rail to the full north-star category density instead of
  leaving the shell with only Home and Date & Time.
- Keep Home and Date & Time native inside System Settings while wiring the
  remaining visible categories to real installed system tools: region/language,
  themes, sound, network, Bluetooth, power, users, privacy and hardware.
- Route Software & Updates to the real Infiltrator Software application and
  move About into the same visual navigation language as the target.
- Alternate warm-gold and cyan icon accents, enlarge the navigation treatment
  and keep unavailable external tools visibly disabled rather than pretending
  they work.
- Make the Home quick-action block match the north-star structure more closely:
  Set Date & Time, Change Region, Configure Display and Software & Updates.
- Extend Linux shell qualification to require the expanded navigation rail.

## 0.4.15 — 2026-09-25 — live dashboard density polish

- Push the Home page materially closer to the committed north-star dashboard
  with a denser two-column information layout and a more purposeful hero visual.
- Expand Quick Actions to a real 2x2 surface: Date & Time, Software & Updates,
  Search Settings and About. Software launches the installed Infiltrator
  Software application and disables cleanly when it is absent.
- Add four real system-state cards for Date & Time, Region & Language, Display &
  Appearance and Network rather than placeholder modules.
- Populate those cards from the running system: local date/time, locale, GTK
  theme/dark preference and GIO network connectivity/metering state.
- Add architecture to System Overview and retain OS, kernel, desktop and
  hostname reporting.
- Extend the Linux shell regression to prove Home is the default stack page and
  Date & Time navigation remains intact.

## 0.4.14 — 2026-09-25 — home dashboard and shell convergence

- Add a real Home landing page so System Settings opens as a graphical control
  centre instead of dropping directly into a form-oriented module.
- Move application identity into a proper top header and add a working settings
  search that filters the currently available navigation pages.
- Add a real System Overview card backed by the running OS, kernel, desktop and
  hostname rather than mock data.
- Add Quick Actions for the implemented Date & Time module and About surface.
- Add a large visual welcome hero with Simple, Secure and Beautiful product
  cues, bringing the shell materially closer to the committed north-star image
  without shipping placeholder settings or non-functional category buttons.
- Keep Date & Time fully intact behind the new Home / Date & Time navigation.

## 0.4.13 — 2026-09-25 — icon-led control surface polish

- Replace the remaining text-form setting rows with icon-led visual control
  tiles while retaining the same live GTK controls and backend behaviour.
- Give locality, clock presentation and system-clock cards distinct accent
  identities so the page reads as a graphical control surface rather than a
  long administrative form.
- Promote manual date/time editing into the same visual tile language.
- Increase hover, border and icon feedback across real editable controls.
- Preserve all existing setting semantics, persistence and operating-system
  authority boundaries.

## 0.4.12 — 2026-09-25 — graphical system snapshot polish

- Make the Date & Time hero substantially more graphical by adding a live
  system snapshot beside the clock preview.
- Surface the real active clock system, calendar, operating-system time zone and
  automatic/manual time-sync state in four icon-led summary tiles.
- Keep snapshot values live as policy or timedated state changes, including
  graceful unavailable-state presentation.
- Strengthen the hero/card hierarchy and compact the visual language further
  toward the committed UI north star without adding placeholder modules.
- Preserve all existing settings behaviour and backend ownership.

## 0.4.11 — 2026-09-25 — second UI north-star polish

- Replace the text-only page heading with a graphical Date & Time identity block.
- Recompose the live preview into a stronger hero surface with an explicit live
  state badge and clearer visual balance.
- Turn locality state into an icon-led information strip instead of loose text.
- Increase section-icon scale and reinforce section/card separation.
- Style operational status and error messages as visible state surfaces rather
  than incidental footer text.
- Keep the pass presentation-only: no placeholder modules or fake controls were
  added, and existing Date & Time behaviour remains unchanged.

## 0.4.10 — 2026-09-25 — first UI north-star polish

- Apply the first working-program polish pass toward the committed UI north star
  without introducing placeholder controls or fake functionality.
- Strengthen the shell with a wider graphical sidebar, larger brand treatment,
  cyan/gold icon emphasis, rounded active navigation and a deeper dark surface
  hierarchy.
- Give the live Date & Time preview a stronger visual anchor with larger type,
  richer card geometry and restrained depth.
- Round and visually separate settings cards and rows, increase control height,
  and make section icons more prominent.
- Cut explanatory copy substantially so the GUI communicates state and actions
  visually instead of reading like an administrative form.
- Preserve all existing Date & Time behaviour and backend authority boundaries.

## 0.4.9 — 2026-09-25 — forensic follow-up hardening

- Add a private-D-Bus regression for timedated owner loss and reacquisition,
  proving unavailable state is surfaced and authoritative state returns.
- Add a native Windows cross-process persistence race test; competing writers
  must leave one complete parseable policy and no stale sibling temporary file.
- Retry transient Windows destination sharing/lock collisions while publishing
  each writer's private staged file, preserving last-completed-writer semantics.
- Enforce mode 0700 on the Linux locality-metadata directory even when the
  directory already existed with broader permissions, with regression coverage.
- Make normal Linux and Windows CI builds use total logical processors minus one,
  matching the project's build-parallelism rule used by sanitizer/release jobs.
- Preserve the complete 0.4.7/0.4.8 forensic implementation and documentation.

## 0.4.8 — 2026-09-25 — forensic audit closure

- Preserve the complete 0.4.7 forensic implementation unchanged.
- Reconcile the audit record with the final successful Linux, Windows and
  sanitizer CI run and the successful immutable release publication.
- Record that the audited implementation commit is the direct parent of the
  published release commit, making the preservation chain explicit.
## 0.4.7 — forensic correctness and documentation audit

- Publish a Debian package, hardware-native source installer and complete source
  ZIP containing the exact pinned Common source.
- Fix GTK dropdown model ownership, repeated activation and close-time cleanup.
- Keep timedated services alive through pending completions and report missing
  service/property state without enabling manual writes on an unknown NTP state.
- Reject non-finite coordinates, malformed locality metadata, oversized manual
  clock fields, DST-gap normalisation and embedded-NUL Windows policies.
- Prevent re-entrant saves, partial CLI writes and Windows temporary-file races.
- Prefer the active localtime zone over stale legacy configuration; retain newly
  reported aliases and skip malformed zone rows safely.
- Add model/parser/metadata regressions, private D-Bus and GTK lifecycle tests,
  transaction tests and precise ownership/failure comments.
- Correct documentation that overstated implemented features and preview owners;
  document build instructions, evidence limits and remaining milestones.


All notable user-visible and architectural changes are recorded here.

## Unreleased

No unreleased changes.

## 0.4.6 — 2026-09-25

- Align the Linux page summary with the suite-wide 12 px hero-subtitle scale.
- Preserve settings behaviour, clock/calendar authority, dependencies and Common APIs unchanged.

## 0.4.5 — 2026-09-25

- Align the persistent sidebar brand tile with the suite-wide 10 px control radius.
- Preserve settings behaviour, clock/calendar authority, dependencies and Common APIs unchanged.

## 0.4.4 — 2026-09-24

- Align the Date & Time hero card with the 18 px publisher panel radius.
- Preserve settings behaviour, clock/calendar authority, dependencies and Common APIs unchanged.

## 0.4.3 — 2026-09-24

- Pin Infiltratr Common 1.19.25 and make its complete shared clock formatter the Date & Time preview authority for every explicit clock mode.
- Remove Calendar as a requirement for specialised clock previews; Calendar remains the optional chronology provider for non-Gregorian date previews.
- Preserve the platform-owned legacy Standard mode by resolving it through Cinnamon's current 12/24-hour choice before entering the shared formatter.

## 0.4.2 — 2026-09-24

- Continue the cautious publisher-wide UI alignment with five small Linux shell changes only.
- Match Software's 6 px navigation-row radius and 3 px / 8 px navigation margins.
- Match the 6 px compact About-button radius, 24 px content bottom padding, and 10 px ordinary settings-card radius.
- Preserve all settings behaviour, page structure, backends, dependencies and Common APIs unchanged.

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
