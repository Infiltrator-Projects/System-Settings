## 0.4.59 — 2026-10-03

- Restore the Home dashboard's intended use of horizontal space: Overview/Quick Actions and the four status cards return to two columns from 720 logical pixels instead of waiting for the overly conservative 1240 px breakpoint that left scaled desktops in a giant one-column layout.
- Stop the four Home status cards from sharing one global homogeneous height. Date & Time and Region & Language now keep row-local natural height instead of inheriting the tallest card's vertical allocation and producing large empty panels.
- Add shell regressions for the 720 px desktop breakpoint and for non-homogeneous status-card height so this wasted-space layout cannot silently return.

## 0.4.58 — 2026-10-03

- Restore the Software-standard 38 px surface-backed navigation icon wells after 0.4.57 incorrectly flattened them, while retaining the compact scrollbar and expanded Home hero spacing.

## 0.4.57 — 2026-10-03

- Replace the oversized page scrollbars with the compact InfiltratorOS treatment and give the Home hero enough internal/vertical room for its title, subtitle and feature row.
- The attempted sidebar icon flattening in this release was corrected immediately in 0.4.58.

## 0.4.56 — 2026-10-03

- Defer decoding of the large Home artwork until after first paint so the shell becomes responsive before loading decorative assets.
- Tighten ordinary setting-row spacing to Common control/compact metrics instead of treating every row as a full section.

## 0.4.55 — 2026-10-03

- Standardise the visible InfiltratorOS shell with Software without flattening System Settings' graphical Home identity: both applications now share the same 58 px header treatment, 205 px navigation rail, 34 px search control and 16 px primary content gutter.
- Preserve the existing System Settings cinematic artwork, overlays, graphical cards, icon wells and product-specific Home composition; this release changes suite framing and rhythm rather than replacing either application's visual personality.
- Retain the exact Common 1.19.38 pin `7070c5812b50821fd7580101cb2289a3184f6b2c`, matching Software's current shared foundation.

## 0.4.54 — 2026-10-03

- Make the built-in Date & Time panel genuinely lazy: Home first paint no longer constructs the panel, parses the time-zone catalogue, installs its policy/locality observers or starts timedated setup until Date & Time is first selected.
- Remove the obsolete one-shot `ss_home_temporal_presentation_now()` path that constructed and destroyed a full presenter for a single value; Home now has one long-lived presenter path only.
- Route the Home “Check for updates” action through the same fixed trusted-system executable resolver used by navigation and Quick Actions instead of falling back to `PATH` lookup.
- Align Date & Time row/tile/page spacing directly with Common `InfiltratrDesignMetrics`, and reduce shell page crossfade to the 110 ms cadence already used by the InfiltratorOS desktop family.
- Correct maintained architecture/presentation documentation left behind by earlier iterations: Common owns all clock rendering, Calendar supplies only non-Gregorian date previews, and failed optional-runtime discovery backs off from 5 seconds to a 60-second ceiling.
- Re-verified the exact Common 1.19.38 pin `7070c5812b50821fd7580101cb2289a3184f6b2c` against current `Infiltrator-Libraries` main; no gitlink movement is required.

## 0.4.53 — 2026-10-03

- Remove the obsolete Calendar-owned clock-preview ABI and test fixture surface; Common is now the sole formatter for every supported clock mode, while the optional Calendar runtime is retained only for non-Gregorian calendar-date previews.
- Align shell typography colours with Common semantic heading, summary, kicker, detail-label and note roles instead of approximating those roles with generic title/muted colours.
- Normalise remaining shell control geometry to the Common 6/10/12/18 radius vocabulary and keep the current Common 1.19.38 pin `7070c5812b50821fd7580101cb2289a3184f6b2c`.

## 0.4.52 — 2026-10-03

- Forensically remove remaining UI residue from the 0.4.45–0.4.49 iteration cycle: delete dead cyan/gold quick-action classes, remove the obsolete Mercedes Grey appearance preview/asset, and use one trusted executable-resolution path for delegated actions.
- Reduce live Date & Time allocation churn by sharing one `GDateTime` per preview tick and replacing the per-tick heap-allocated date cache key with explicit cached civil-day/calendar state.
- Back off failed optional Calendar-runtime discovery exponentially to a 60-second ceiling instead of rescanning trusted library roots every five seconds on the GTK thread.
- Bring Home and Date & Time geometry back onto the current Common design values (18 px panel radius, 12 px card radius, 10 px control radius, 20 px screen padding) and reserve Common warning gold for actual warning semantics instead of decorative accents.
- Align the UI vision document with the canonical Infiltrator design contract: System/Day/Night follow Common semantic palettes and metrics, while product identity remains in genuine product assets rather than ad-hoc colour splits.
- Re-verified that the pinned Common 1.19.38 commit `7070c5812b50821fd7580101cb2289a3184f6b2c` is the current `Infiltrator-Libraries` main head; no submodule movement is required.

## 0.4.51 — 2026-10-03

- Compact the Date & Time page: prevent GtkSwitch controls from expanding into row-width sliders, keep Latitude/Longitude paired in a fixed two-column grid, use Common success green for LIVE state, and reduce excess card/control spacing.

## 0.4.50 — 2026-10-03

- Forensically remove high-frequency refresh-all behaviour left by the iterative Home and Date & Time UI work: unchanged GTK labels are no longer rewritten, successful calendar formatting is cached by calendar/day, and static overview policy labels are no longer asserted on every 250 ms clock tick.
- Stop Home resize feedback from repeatedly reapplying identical FlowBox constraints for every `notify::width`; adaptive geometry now changes only when the one/two-column breakpoint is actually crossed.
- Keep fast extended-clock cadence intact so decimal/French and other non-SI clock displays remain accurate while moving the expensive static work off that cadence.
- Bring the shell closer to the restrained InfiltratorOS surface language by removing the leftover Date & Time hero drop shadow and removing the obsolete gold/cyan navigation class split and styling navigation icons directly with Common's neutral accent.
- Advance the pinned Common 1.19.38 source to current Common head `7070c5812b50821fd7580101cb2289a3184f6b2c`, including the latest portable-build and hosted-CI corrections.
- Make Common-backed GTK theme installation idempotent so reopening the shell no longer removes and re-adds an equivalent display-global CSS provider during the previous window's teardown; add a repeated-activation regression for stable theme generation.

## 0.4.49 — 2026-09-29

- Make the location-concurrency regression deterministic in the native PGO validation path. Writer A now signals after durable publication while still holding the lock; Writer B signals immediately before contention; the parent releases A without scheduler-dependent sleeps. This removes the CI-only 250 ms lock-timeout race without weakening the production lock timeout.
- Fix the persistent 21 px hero/card overlap at its real source: the hero foreground intentionally paints slightly below the bitmap edge, so the hero now reserves a 28 px bottom layout margin. Compact hero height returns to 390 px and wide mode stays 270 px instead of growing the image to chase an invariant overflow.
- Raise compact Home hero containment from 480 to 520 logical pixels after the allocation regression measured the wrapped feature row ending 21 px beyond the 480 px hero; wide desktop mode remains 270 px.
- Increase the compact Home hero allocation to 480 logical pixels so the wrapped feature row has a conservative containment margin on GTK/X11 test desktops; wide desktop mode remains at the original 270 px height.
- Fix the allocation regression test itself after exercising the wide breakpoint: restore compact mode and drain GTK's pending relayout before checking feature/card bounds, so the assertion inspects the 390 px compact hero rather than the stale 270 px wide allocation.
- Fix the CI-only GTK4 API mistake in the new hero-height regression by querying the size request through gtk_widget_get_size_request(), allowing warnings-as-errors builds to compile the test on GTK 4.14.
- Correct the fourth failed 0.4.49 attempt: the shell width budget now passes, but the compact hero could still be too short once its three feature tiles wrapped. The hero now grows to 390 logical pixels in compact mode and returns to the original 270 px proportion on wide desktops, preventing the feature row from painting into System Overview.
- Correct the third failed 0.4.49 attempt: the adaptive outer rows reduced the shell minimum to roughly 1100 px, but the permanently horizontal hero feature row still kept the Home page wider than the 960/1024 logical desktop budget.
- Put the three hero features and four appearance previews under the same wide-desktop breakpoint as the dashboard rows: compact windows may wrap them, while wide desktops explicitly restore the intended 3-up and 4-up presentation.
- Correct the second failed 0.4.49 attempt: fixed two-column GtkGrid containers still forced a roughly 1440 px shell minimum and therefore could not satisfy the 960/1024 logical desktop budget.
- Make the two desktop dashboard rows genuinely adaptive. They request one column at compact widths, then explicitly lock to two columns once the actual Home viewport reaches 1240 logical pixels, restoring Overview/Quick Actions and both status-card pairs side-by-side on ordinary wide desktops without making that width a global minimum.
- Restore Quick Actions to an internally responsive two-column FlowBox so its own minimum width does not force the outer dashboard wider.
- Keep the three hero features and four appearance previews as deliberate horizontal strips; those compact fixed rows fit within the narrow-shell budget and should not become vertical lists.
- Restore the original structural containers for the Home composition: horizontal hero feature strip, two-column Overview/Quick Actions grid, 2x2 Quick Actions grid, 2x2 status grid, and horizontal four-preview appearance strip. Keep FlowBox only inside card bodies where reflow is genuinely useful.
- Fix the failed first 0.4.49 CI attempt, whose forced FlowBox minimum child counts raised the shell minimum width to about 1480 px and correctly prevented publication.
- Restore the intended Home dashboard composition after tracing the regression to the 0.4.38 responsive-layout conversion: System Overview and Quick Actions are again a two-column row, and the four status cards are again a 2x2 grid.
- Keep the responsive FlowBox implementation and its narrow-content wrapping, but set the structural minimum child counts to the original design instead of allowing homogeneous top-level cards to collapse into one full-width column on normal desktops.
- Restore the Display & Appearance preview strip to four thumbnails on one row and keep the three hero feature tiles on one row.
- Add allocation-level shell regressions that verify the top-card pair, both status-card pairs and the appearance preview strip actually occupy their intended rows, preventing another visually broken layout from passing tests.

## 0.4.48 — 2026-09-29

- Fix the remaining Home hero allocation regression visible at the compact default window: the Simple / Secure / Beautiful feature tiles no longer paint underneath the System Overview card.
- Raise the hero scene and overlay allocation floor to 270 logical pixels so the complete foreground remains inside the mountain artwork at ordinary and 2× display scale.
- Replace the previous preferred-size-only overlap regression with an actual allocated-geometry assertion proving the feature row finishes before the Home card grid begins.

## 0.4.47 — 2026-09-29

- Fix the shell allocation regression that let navigation-row label expansion propagate through the sidebar, causing the left pane to consume a large share of maximised-window width.
- Make the sidebar an explicit non-expanding boundary so spare horizontal space belongs to the main settings stack; this restores the intended Home proportions and prevents System Overview / Quick Actions from wrapping simply because the sidebar grew.
- Add a permanent shell regression proving the sidebar does not horizontally expand while the main stack does, including the existing 2× display-scale run.

## 0.4.46 — 2026-09-29

- Fix the packaged Home artwork regression by making the Linux runtime asset path and Debian package installation prefix identical.
- Default Linux system installs to /usr before GNUInstallDirs is evaluated, so the executable now looks for the shipped bitmap assets in /usr/share/infiltrator/system-settings/ui instead of the generic /usr/local/share path.
- Force CPack staging to use the same CMAKE_INSTALL_PREFIX as the compiled runtime, eliminating future package/runtime path drift.
- Add runtime diagnostics for missing UI assets.
- Strengthen Debian integration validation so CI proves the packaged executable contains the /usr/share asset path, rejects the stale /usr/local path, and byte-compares every installed UI bitmap against its source asset.

## 0.4.45 — 2026-09-29

- Fix the Home dashboard hero measurement so the System Overview and Quick Actions cards cannot be allocated over the Welcome/System Settings banner.
- Make the hero foreground participate in GtkOverlay preferred-size calculation instead of relying on the decorative image's fixed minimum height.
- Add a permanent shell regression that proves the hero measures at least as tall as its foreground content at normal and 2× GTK scale.

## 0.4.44 — 2026-09-29

- Preserve the distinction between missing and unreadable locality metadata during Date & Time startup, and prevent uncertain metadata authority from being silently replaced by a time-zone reference approximation.
- Re-read both timedated state and regional time-zone context after taking the locality lock before publishing automatic reference coordinates, closing the remaining stale-zone race.
- Make transaction-journal deletion parent-directory durable while retaining the rule that cleanup failure after metadata publication never rolls committed policy back.
- Reduce Linux and Windows cross-process lock contention waits from multi-second UI stalls to a bounded quarter-second retry window, with explicit contention regressions.
- Use the same 1 Hz cadence for conventional seconds clocks on Date & Time as Home while retaining 250 ms sampling for extended temporal systems.
- Validate geocoder coordinates, country-code capacity, UTF-8 and locality-query length before search results enter the settings UI.
- Resolve delegated Linux settings tools only from fixed system executable roots and sanitize the native/source packaging tool search path.
- Add regressions for locality I/O classification, preservation of unreadable recovery journals, lock timeout behaviour and Date & Time preview cadence.
- Update current documentation to the pinned Infiltratr Common 1.19.38 and the hardened Calendar runtime-discovery contract.

## 0.4.43 — 2026-09-29

- Distinguish missing, invalid and I/O-failed locality metadata so transient filesystem and permission failures cannot masquerade as deletion or corruption.
- Preserve unreadable locality journals for retry, make journal discard success observable, and never roll the temporal policy back merely because directory durability confirmation failed after metadata was already unlinked.
- Replace error-blind locality existence probes in Date & Time with typed authoritative load results.
- Back off repeated locality recovery exponentially from two seconds to a capped sixty seconds instead of polling forever at a fixed cadence.
- Bound Linux and Windows cross-process policy/locality lock acquisition to two seconds so a stalled peer cannot freeze the GTK main thread indefinitely.
- Add a real forked two-process locality transaction race regression and assert the accessibility labels that the dedicated accessibility CI job is intended to protect.
- Require actual GCC .gcda training data before a build may identify itself as native, in addition to the existing native/LTO/profile-use flag checks.
- Restrict the optional Calendar preview bridge to explicit system library roots and exercise the full native build/test/package path in CI.
- Pin Infiltratr Common 1.19.38 for the latest temporal coordinate validation and POSIX hardening.

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Changelog

## 0.4.42 — 2026-09-28 — locality recovery and shell hardening

- Make CLI location changes participate in the same durable locality journal, temporal-policy publication and metadata-finalization transaction as the GUI, including rollback when metadata publication fails.
- Reload both authoritative temporal policy and authoritative locality metadata after taking the locality lock before snapshotting rollback state, preventing concurrent locality updates from being restored to stale values.
- Preserve an interrupted locality journal whenever the post-lock authoritative policy cannot be read, clear stale locality metadata when location authority is explicitly removed, and retry recovery without taking the Date & Time panel offline.
- Remove the Linux temporal-policy transaction lock's fixed PATH_MAX buffer so long valid XDG configuration paths remain fully transactional.
- Require the final CPU-native installer build to have complete PGO training data instead of suppressing missing-profile diagnostics.
- Size the shell against the mapped monitor where available, reduce fixed logical-width floors, and keep delegated settings destinations retryable after transient child-process failures.
- Move Home theme and network status to native change notifications while retaining a low-frequency refresh only for uptime and other time-derived environment state.
- Add explicit accessibility metadata to navigation, search and Date & Time status surfaces and run a GTK shell regression with accessibility enabled.
- Prefer a qualified BigBedroom runner for trusted release commits as well as ordinary trusted pushes, while retaining hosted publication and Windows validation.
- Describe GitHub release publication as verified rather than platform-enforced immutable, matching the actual release configuration.

## 0.4.41 — 2026-09-28 — locality transaction and high-DPI correctness

- Serialize the complete locality journal/policy/metadata transaction with a dedicated interprocess lock and make GUI and CLI coordinate writers use the same locality-before-policy lock order.
- Reload authoritative temporal state after acquiring the locality lock before snapshotting rollback coordinates, preventing failed metadata publication from restoring another process's older location.
- Treat post-commit journal unlink failure as cleanup debt rather than transaction failure, so a successfully published location.ini never triggers an incorrect policy rollback or prevents Date & Time from opening.
- Recover interrupted locality publication when an already-open panel observes the matching external policy change instead of waiting for the panel to be reconstructed.
- Compare persisted coordinates at their six-decimal storage precision instead of exact floating-point equality, eliminating false external-change notifications after System Settings saves its own location.
- Refresh every visible seconds-enabled clock at 250 ms so decimal, Internet and sidereal-rate displays cannot skip visible units; seconds-disabled clocks remain at 1 Hz.
- Clamp the initial window to 90% of logical monitor geometry and exercise the actual 1920x1080-at-2x 960x540 budget in CI.
- Expose policy-vs-native compatibility verification through the persistence boundary and make system-settings-time return a distinct failure when the policy committed but Cinnamon compatibility synchronization did not.
- Reject truncated Linux presentation.lock paths instead of silently locking the wrong pathname.
- Make the About native identity enforceable: CMake now refuses a native profile unless GCC, -march=native, LTO and trained -fprofile-use flags are present; the PGO training pass identifies itself as development and only the final optimized pass becomes native.

## 0.4.40 — 2026-09-28 — cross-process and release-integrity hardening

- Serialize temporal-policy read/modify/write transactions across processes on Linux and Windows. Setters now lock, reload the latest authoritative document, mutate one semantic field and publish atomically, preventing unrelated GUI/CLI edits from being lost.
- Put multi-option `system-settings-time` commands behind the same transaction lock and add a cross-process regression that proves GUI/model and CLI writers preserve one another's independent changes.
- Observe `location.ini` through the same atomic-replacement-safe file observer as the temporal policy, recompute locality/time-zone-follow authority after external writes and retain the last known-good locality across transient read failures.
- Prevent time-zone reconciliation from writing stale locality metadata whose coordinates no longer match the active temporal policy.
- Add a private durable `location.pending` write-ahead journal so a crash between coordinate-policy publication and richer locality-metadata publication is recovered deterministically at the next Date & Time startup.
- Preserve typed manual wall-time drafts across unrelated external policy changes; only a changed clock/calendar interpretation invalidates the draft.
- Verify the conventional Cinnamon clock/seconds mirror after policy publication and surface partial synchronization instead of reporting unconditional success.
- Remove source-tree UI/module paths from generic/native production binaries; retain source fallbacks only for development/CMake profiles.
- Observe delegated settings-tool termination so an executable that rejects its requested module no longer masquerades as a successful handoff.
- Reduce visible temporal polling: ordinary clocks use a one-second cadence while the faster 250 ms cadence is reserved for decimal seconds that need it; hidden-page timers remain stopped.
- Expand responsive regression coverage to include the complete content-plus-custom-titlebar minimum and a dedicated `GDK_SCALE=2` shell run.
- Keep LeakSanitizer enabled for the GTK shell and suppress only process-global Fontconfig cache allocations rather than disabling GUI leak detection.
- Replace mutable/deprecated GitHub Action tags with reviewed immutable Node-24 action commits and use the exact pinned Common Git submodule object on BigBedroom instead of constructing provenance around a downloaded archive.
- Make release/source generation reproducible from the release commit epoch, rebuild Debian/source/native artifacts twice in CI for byte comparison, and compare the complete extracted native-installer source tree against the exported source tree.
- Strengthen the native installer profile to require GCC CPU-native code generation, LTO and a trained PGO generate/test/use cycle before packaging and installation.
- Extend the shell ownership guard across every Linux shell translation unit and expand the Linux suite to seventeen project tests.

## 0.4.39 — 2026-09-28 — complete forensic repair pass

- Reconcile external temporal-policy writes into an already-open Date & Time panel through an atomic-replacement-safe policy observer, preventing later GUI edits from publishing stale whole-policy snapshots.
- Keep Home temporal presentation alive through transient policy read/permission failures, observe policy permission/atomic-replacement changes, and retry failed reloads with bounded exponential backoff instead of a permanent tight retry loop.
- Make locality metadata and temporal coordinates one truthful transaction: failed metadata publication restores the previous in-memory metadata as well as the persisted policy, while failed system-time-zone reference-coordinate persistence is surfaced explicitly.
- Debounce latitude/longitude edits into one coordinate transaction instead of durably publishing every spin-button pulse or an intermediate half-edited pair.
- Move locality search, coordinate persistence, rollback and advisory zone inference into a dedicated Date & Time locality controller, reducing the main Date & Time controller and clarifying domain ownership.
- Keep nearest tzdata reference points advisory only; explicit Time zone selection remains the sole path that changes the authoritative operating-system zone.
- Strengthen protected timedated coverage with private-bus SetTimezone, SetTime, NTP denial and outstanding-call lifetime tests while retaining independent cancellation/generation ownership.
- Expose visible setting titles/descriptions as GTK accessibility properties, add explicit labels for locality/coordinate/manual controls, and move Date & Time operation status to a page-level surface rather than hiding unrelated failures inside the System Clock card.
- Start and stop Home temporal/status timers with page map/unmap lifecycle, eliminating four-Hz hidden-page wakeups, and keep Date & Time's fine-grained preview similarly visible-only.
- Follow live desktop theme preference changes through a dedicated Common-backed Linux theme component; consolidate the previously layered shell CSS into one authoritative selector owner per component.
- Improve narrow-screen layout and validate both width and height against a 1024×768 allocation budget.
- Drive Date & Time navigation filtering from the module manifest's actual Search/Target metadata, while presenting the current field honestly as a navigation filter until full multi-module deep-link search lands.
- Correct delegated navigation labels so Cinnamon/Blueman handoffs describe the destination they actually open.
- Report CLI policy provenance correctly after a first successful save and regress that first-save path.
- Replace the release-only Bash source-asset builder with a native C builder and exercise it in ordinary CI.
- Bind exported Common source to the exact 1.19.35 pin with a complete per-file SHA-256 manifest; CMake verifies every listed byte and rejects unlisted extra files when Git metadata is absent.
- Route trusted non-release jobs to an online, idle BigBedroom runner only after a hosted availability probe; pull requests never execute repository-controlled code on the home runner, and offline/busy/unprovisioned states fall back to hosted Ubuntu.
- Make immutable release verification require the exact expected asset-name set as well as byte-for-byte equality and checksum verification.
- Expand the Linux System Settings suite to fifteen project tests, covering policy observation, Home resilience, external-policy reload, locality rollback, live theme following, manifest-derived search, protected timedated paths, 1024×768 responsiveness and both native release builders.

## 0.4.38 — 2026-09-28 — forensic correctness and lifecycle hardening

- Stop treating nearest tzdata reference points as authoritative time-zone boundaries. Named locality selection now saves locality/coordinates and presents any nearest same-country IANA zone as an advisory suggestion; the operating-system zone changes only through the explicit Time zone control.
- Keep locality metadata bound to the real timedated zone. Inferred zones are never persisted as accepted state, failed metadata reconciliation remains retryable, and successful authoritative changes reconcile metadata explicitly.
- Make locality/custom-coordinate rollback truthful: rollback persistence is checked, authoritative policy reload is attempted when rollback itself fails, and the UI no longer claims restoration that did not occur.
- Serialize semantically conflicting protected timezone, NTP and manual-clock mutations while retaining independent cancellables/generations for stale-completion safety.
- Clear/reseed dirty manual wall-time drafts when clock/calendar/NTP/time-zone context changes, and disable conflicting controls while a protected operation is unresolved.
- Surface failed Cinnamon-to-Infiltrator reconciliation instead of silently leaving native and shared temporal policy divergent.
- Run the Date & Time fine-grained preview at 250 ms only while the panel is mapped, eliminating the old one-second under-sampling of decimal/French-style finer units and avoiding hidden-page formatting work.
- Harden the Home temporal presenter against first-run policy-directory absence with a retrying monitor path, and make temporal presentation output a true output-only API so uninitialised caller storage is never freed.
- Replace rigid Home card grids plus coordinate/manual control rows with wrapping FlowBoxes, reduce shell padding/search minimums, and add a regression that enforces a 1024-pixel shell minimum-width budget.
- Restore keyboard accessibility to the custom Minimize, Maximize/Restore and Close controls and give the icon-only Date & Time card action an explicit accessible label.
- Correct Home regional reporting so interface language and LC_TIME formatting locale are no longer conflated, and stop falsely marking generic Light/Dark previews as the active theme when Follow OS or another theme may be authoritative.
- Avoid background Home status/temporal formatting work while Home is unmapped, retain source-tree artwork fallback for development builds, and keep missing installed artwork as a neutral deterministic surface.
- Fix shell construction order so Home is not selected before its GtkStack page exists, and remove invalid GTK CSS declarations that were producing parser warnings.
- Strengthen the Linux shell regression with navigation/search, advisory locality-zone semantics, timer map/unmap ownership, keyboard focusability, missing-art handling and responsive-width checks.
- Make BigBedroom the preferred non-release Linux probe/build environment without requiring passwordless sudo; if its compiler/CMake/GTK/geocode prerequisites are absent, CI falls back to hosted Ubuntu. Release validation remains hosted.
- Require exact Common 1.19.35 provenance even in exported source trees by embedding and validating the pinned commit marker when Git metadata is unavailable.
- Make release reruns verify every immutable DEB, native installer, source archive and SHA256SUMS asset byte-for-byte instead of checking only the Debian-package filename.

## 0.4.37 — 2026-09-28 — native C installer

- Replace the distributed Bash self-extracting .run with a compiled C executable.
- Preserve the existing native install contract: extract the exact release source, configure with -march=native/-mtune=native, build, run the full test suite, create the Debian package and install it through APT.
- Store the source archive as an authenticated-by-structure trailing payload with a fixed-size length footer; the native executable extracts only that bounded payload.
- Keep the release-only source-asset builder as packaging infrastructure, while removing the Bash runtime installer from the published source tree.
- Compile the native installer under the project's strict warning policy during ordinary Linux test builds and exercise its --help path in CTest.
- Make the release asset builder prove that the generated .run is an ELF executable and that its embedded source payload round-trips correctly before publication.

## 0.4.36 — 2026-09-28 — forensic shell and Date & Time repair

- Remove the remaining Cairo-generated mountain, Australian flag and network artwork fallbacks; missing packaged art now degrades to a neutral surface instead of inventing substitute imagery.
- Resolve UI artwork from the configured installation data directory rather than assuming /usr/share, and verify all nine runtime PNG assets inside the Debian package.
- Remove the late hard-coded cyan/glow styling overrides and return those surfaces to Common semantic palette roles.
- Reduce fixed widths and large minimums across Home and Date & Time so the native window can fit substantially smaller desktops without inheriting the former 1420×900 assumption.
- Replace Home's repeated policy/GSettings/Calendar reconstruction with one long-lived temporal presenter that watches the policy directory and native clock preference; retain the fast display tick without rereading policy or rediscovering Calendar four times per second.
- Add live Home refresh for uptime, local time-zone identity, locale, theme and network state instead of freezing those cards at application construction.
- Give SsHomeTemporalPresentation an explicit initialisation contract and regression coverage rather than freeing unspecified caller memory.
- Separate timedated timezone, NTP and manual-clock cancellables/generations so independent user operations cannot cancel one another.
- Protect dirty manual date/time entry from unrelated timedated property refreshes until the manual write succeeds.
- Make locality/custom-coordinate persistence rollback the temporal policy when metadata persistence fails, and keep locality metadata aligned to the authoritative current system zone until a requested zone change is actually observed.
- Remove Australia-only Home presentation assumptions and display the active locale truthfully.
- Keep Home quick navigation and the selected sidebar row in sync, disable the updates affordance when Infiltrator Software is absent, and avoid duplicate uname calls.
- Reassert the shell/module boundary by removing locality-metadata ownership from main.c and extending the configure-time ownership guard.
- Expand shell regression coverage for navigation synchronisation, search filtering, live Home sources and neutral missing-asset behaviour.
- Prefer BigBedroom for non-release Linux CI, with REST/pinned-Common checkout support for runners without Git; immutable release validation remains hosted.
- Correct the architecture documentation's stale Common 1.19.25 reference to the actual 1.19.35 pin.
- Run leak detection across the non-GTK sanitizer suite while keeping ASan/UBSan active for the GTK shell without treating GTK/Pango/fontconfig process-global caches as application leaks.

## 0.4.35 — 2026-09-26 — remove mountain hero cyan edge

- Remove the explicit cyan border from the full-width mountain hero panel.
- Keep the raster mountain artwork and internal overlays unchanged; the hero now has no outer cyan edge and no outer glow.

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
