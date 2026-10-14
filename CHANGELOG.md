# Changelog

This file tracks the current System Settings maintenance line. The pre-audit
history is preserved unchanged in [`CHANGELOG-legacy.md`](CHANGELOG-legacy.md).
The 0.4.60-0.4.69 overlap is intentional: it closes gaps in the historical file
while keeping the original record immutable.

## 0.4.103 — 2026-10-14

- Stop the Home dashboard from inheriting precision-clock polling cadence: Home now suppresses seconds and refreshes its temporal summary once per minute instead of every 500–1000 ms, eliminating the continuous GTK measure/re-layout cycle behind the visible micro-jump and excessive CPU use.
- Keep precision where it belongs: the Date & Time page still presents the selected clock model at its full configured fidelity, while Home remains a stable settings overview using the same authoritative clock/calendar semantics at coarse precision.
- Make failure behaviour equally bounded by using the same minute-scale cadence when the Home presenter is temporarily unavailable, and add regressions proving conventional and decimal Home presentations suppress seconds and cannot regress to sub-second/one-second polling.

## 0.4.102 — 2026-10-14

- Finish the deterministic native Linux UI composition by replacing Date & Time's remaining `GtkFlowBox` card/manual-control layouts with explicit typed `GtkBox` rows and width-driven breakpoints, so fixed relationships no longer depend on heuristic child wrapping.
- Keep the live preview and Presentation/System Clock surfaces as desktop peers, stack them intentionally on narrow content widths, and stack manual date/time controls only below an explicit compact-width threshold.
- Extend the black-box shell regression to protect the Date & Time hero, peer-card and manual-control containers as explicit boxes, matching Home's structural regression and preventing heuristic flow layout from returning.

## 0.4.101 — 2026-10-14

- Remove the unsupported GTK CSS `max-width` declaration from the Home hero overlay after native PGO qualification correctly rejected the parser warning, while preserving the 0.4.100 visual composition and contrast treatment unchanged.
- Keep the release gate authoritative: normal Linux/Windows tests and sanitizers had already passed, but the native installer remained blocked until its own extracted-source build and shell test could consume the stylesheet without GTK parser diagnostics.

## 0.4.100 — 2026-10-14

- Reconcile the Linux Home stylesheet with the 0.4.98/0.4.99 dashboard structure: the image hero now honours the intended compact desktop height instead of being forced back to the retired 300-pixel composition, and obsolete hero/feature selectors from the superseded layout are removed.
- Strengthen the visual hierarchy without creating a second design authority: Common still supplies palette, typography and geometry, while the Home layer adds restrained image-overlay contrast, card depth and interactive Quick Action hover treatment appropriate to this product composition.
- Style the new InfiltratorOS hero badge explicitly, reduce the System Overview artwork footprint, and stop using the success semantic colour as a permanent Network decoration when network health is not necessarily successful.

## 0.4.99 — 2026-10-14

- Reconcile the black-box Linux shell regression with the deliberate Home redesign: validate the new concise hero copy instead of preserving the retired marketing slogan as an accidental product contract.
- Make the regression protect the structural fix itself by asserting that the primary dashboard row is an explicit `GtkBox` and Quick Actions is an explicit `GtkGrid`, preventing future visual work from silently reintroducing heuristic `GtkFlowBox` wrapping for those fixed relationships.
- Preserve the 0.4.98 Home composition unchanged; this patch corrects validation ownership after CI correctly rejected the stale presentation expectation.

## 0.4.98 — 2026-10-14

- Rebuild the Linux Home dashboard around deterministic responsive peer rows instead of nested `GtkFlowBox` wrapping, so System Overview and Quick Actions remain side-by-side at ordinary desktop widths and the four live status surfaces retain intentional pairings.
- Reduce the hero from competing marketing/feature overlays to one concise product identity surface, replace heuristic quick-action wrapping with a stable 2×2 grid, and tighten the page hierarchy around the documented graphical InfiltratorOS north star.
- Preserve Common as the visual authority and retain live temporal/status observation, installed artwork, trusted external-tool handoffs and an explicit single-column adaptation for genuinely narrow content widths.

## 0.4.97 — 2026-10-14

- Complete the required post-fix forensic pass by bringing Windows rollback clear semantics up to the same durable-absence contract as Linux: the live policy name is now removed through a same-directory `MOVEFILE_WRITE_THROUGH` rename before any cleanup is attempted, rather than relying on bare `DeleteFileW` completion.
- Keep cleanup subordinate to the transaction commit point with one bounded tombstone name, so a transient scanner or sharing hold cannot turn an already-durable rollback into a false transaction failure or create unbounded release debris.
- Add an isolated path-injected Windows clear regression covering existing-policy removal, observable absence, normal tombstone cleanup and idempotent repeated clear without touching the user's real LocalAppData policy.

## 0.4.96 — 2026-10-14

- Complete the forensic persistence pass by consolidating Linux locality directory creation, private atomic metadata publication and durable namespace removal onto Common 1.19.41's tested POSIX primitives instead of maintaining overlapping GLib/local mechanisms.
- Make temporal-policy rollback deletion fulfill the store's durable-clear contract: policy absence now reports success only after Common has published the unlink through the parent directory, while an already-missing policy remains idempotent success.
- Re-audit module ownership, async lifetime, transaction/recovery, platform authority, build/test boundaries and exact-tested-artifact publication; retain deliberate complexity where it protects stronger contracts rather than replacing it with weaker abstractions.

## 0.4.95 — 2026-10-14

- Make the two-file locality transaction restore policy provenance as well as values: the Date & Time model now retains the temporal-policy update lock across locality metadata publication, and rollback removes the explicit policy again when the transaction began from platform defaults instead of materialising a shadow copy of those defaults.
- Apply the same absence-preserving rollback rule to multi-option CLI locality transactions, add platform clear operations for Linux and Windows, and add regressions proving lock lifetime, successful completion, rollback failure release and restoration of the no-policy state.
- Remove stale UI-owned temporal-policy rollback snapshots from both named-locality and custom-coordinate paths; rollback authority now lives at the model/persistence boundary that owns the exact pre-mutation state.

## 0.4.94 — 2026-10-13

- Correct the maintained Date & Time validation contract after the 0.4.92 Common-provider consolidation: Common now explicitly owns trusted Calendar runtime discovery, validation and retry/recovery; System Settings owns one lightweight Common date-provider instance per preview lifetime; and specialised clock formatting is documented and tested as an independent Common clock-formatter contract rather than a Calendar responsibility.

## 0.4.93 — 2026-10-13

- Complete the post-fix forensic documentation pass by indexing the maintained UI north-star document in the canonical documentation ownership map, making its subordinate relationship to Design, accessibility/platform requirements and Common's design contract explicit rather than leaving a maintained policy document outside the map.

## 0.4.92 — 2026-10-13

- Replace the product-local Calendar/GObject runtime loader with Common 1.19.41's stronger POSIX date-provider contract. System Settings now consumes the stable bounded `calendar_plus_format_date_v1` ABI, removing duplicated trusted-root discovery, retry state, object caching, test hooks, direct `dl` ownership and the `RTLD_NODELETE` lifetime workaround.
- Remove stale compatibility and white-box surfaces left by earlier iterations: the hidden GTK constructor alias, two same-translation-unit private headers, unused shell path fallbacks and process-environment module/UI resource overrides. The shell regression now injects its manifest root explicitly and links only through the shell boundary.
- Reconcile lock-budget comments/callers with the actual platform contracts, replace the old Calendar provider regression with a Common/Calendar integration fixture, and re-document the resulting dependency, validation and runtime-ownership boundaries.

## 0.4.91 — 2026-10-13

- Complete the follow-up forensic boundary pass by removing shell-owned theme installation from the module-safe Linux UI helper target; Date & Time now depends only on the reusable GTK helper implementation rather than inheriting shell presentation machinery through the build graph.
- Add a configure-time source-ownership invariant that rejects any future `src/shell/` implementation added to `system-settings-linux-ui`, making the intended shell/module boundary mechanically enforceable instead of merely documented.
- Re-check the transaction, async-lifetime, Common provenance, packaging, reproducibility and exact-tested-artifact release mechanisms and retain them where their complexity protects stronger correctness guarantees.

## 0.4.90 — 2026-10-13

- Complete a second repository-wide brittleness pass against the canonical Infiltrator Engineering Philosophy and remove dead Home status ownership state that had no lifecycle or behavioural contract.
- Reconcile maintained validation documentation with the actual twenty-test Linux CTest suite.
- Revalidate the deliberate transaction, async-lifetime, Common-provenance, packaging and exact-tested-artifact release boundaries; retain them where their complexity protects stronger correctness guarantees rather than replacing them with weaker abstractions.

## 0.4.89 — 2026-10-13

- Qualify the Linux release package under the existing `generic` build profile rather than the source-aware default CMake profile, so installed UI assets and module manifests are the sole runtime authority and published Debian binaries no longer embed the CI checkout path.
- Make same-SHA release reruns preserve published asset immutability: existing assets are byte-verified and never clobbered; only missing expected assets may be added to complete a partial publication.
- Add package/release regression gates and align architecture/validation documentation with those enforced contracts.

## 0.4.88 — 2026-10-13

- Remove the last duplicated module-manifest filename assumption from Debian integration validation: CI now derives the expected manifest set from `modules/` and compares every packaged manifest byte-for-byte.
- Make the release gate enforce the documented repository topology itself: a releasable current `main` must have exactly one remote branch, `main`, so branch debris can no longer coexist with a green published release.
- Re-audit architecture, build profiles, Common provenance, tests, sanitizers, native/source installers and exact-tested-byte release promotion; retain deliberate explicit packaging allowlists and standalone installer duplication where they provide stronger review or bootstrap boundaries rather than replacing them with weaker automatic discovery.

## 0.4.87 — 2026-10-13

- Complete a repository-wide brittleness pass: move reusable Linux UI helpers out of shell ownership, move built-in module composition contracts into `src/app`, and enforce those boundaries at CMake configure time.
- Make the Common gitlink the single dependency-revision authority in Git checkouts; exported source archives retain their gitlink-derived commit marker plus complete SHA-256 byte manifest instead of duplicating the pin in CMake.
- Tighten Debian integration verification to compare source assets byte-for-byte and derive the packaged UI asset set from the source tree instead of maintaining a duplicated filename inventory.
- Simplify repository/release machinery around the documented single-branch direct-to-`main` model, add focused repository ignores, align validation/module documentation with the enforced boundaries, and backfill the changelog so released versions are represented truthfully.

## 0.4.86 — 2026-10-13

- Remove unreachable self-hosted-runner branches and align validation documentation with the hosted-runner release path.
- Simplify the native installer to the single enforced GCC native/LTO/PGO build path without weakening release gates.

## 0.4.85 — 2026-10-12

- Make a green current-`main` workflow imply a published release: publication fails if `VERSION` is already owned by another commit, stale superseded runs retire, and same-SHA reruns remain idempotent and repairable.

## 0.4.84 — 2026-10-12

- Collapse release promotion into the gated CI run itself, removing the separate cross-run `workflow_run` handoff and artifact lookup while retaining current-main protection, idempotent repair and central APT ownership.

## 0.4.83 — 2026-10-12

- Make the tested release bundle the publication authority instead of duplicating release filenames and counts in promotion logic; seal and verify the exact bytes before publishing generically.

## 0.4.82 — 2026-10-12

- Simplify and harden release promotion so verified artifacts move through one smaller, deterministic publication path.

## 0.4.81 — 2026-10-12

- Make release-artifact bundle paths stable and asset-driven, classify stale CI runs explicitly, and tighten the concurrency/retry contract around publication.

## 0.4.80 — 2026-10-12

- Harden delegated executable resolution against symlink/path substitution, expand black-box shell persistence/lifecycle coverage and enforce single-branch `main`-only repository hygiene.
- Fix strict-C portability for the validated `realpath` trust path while retaining the hardened executable-resolution contract.

## 0.4.79 — 2026-10-12

- Bound Linux settings-lock latency, strengthen trusted delegated-program resolution, and release completed timedated connection state so protected-service retries remain possible.

## 0.4.78 — 2026-10-12

- Type timedated outcomes, test semantic error classification, and make locality recovery acquire its own authoritative policy after locking instead of relying on stale caller state.

## 0.4.77 — 2026-10-12

- Add actionable path-sensitive Clang analysis, centralize Linux advisory locking, preserve module-manifest diagnostics, remove analyzer-confirmed transaction/packaging defects and harden release-asset generation.

## 0.4.76 — 2026-10-12

- Centralize built-in module identity, enforce Date & Time model-authority encapsulation, replace direct CLI model mutation with validated drafts, expand CI coverage and decouple verified releases from downstream APT pull latency.

## 0.4.75 — 2026-10-11

- Make Home temporal navigation target Date & Time explicitly rather than depending on registry order, and put protected timezone/NTP/manual-time lifecycle behind a narrow compiler-enforced boundary with regression coverage.

## 0.4.74 — 2026-10-11

- Replace shell and Home magic widget-data contracts with typed state, including the correct closure-destroy signature, so private controller state is no longer coupled through untyped keys.

## 0.4.73 — 2026-10-11

- Split Date & Time preview lifecycle, Home system-status observation and shell module-catalog parsing out of oversized controllers, and enforce the new preview ownership boundary in the compiler.

## 0.4.72 — 2026-10-11

- Separate generic application identity from Date & Time core so the shell/executable no longer link module-core code merely for product identity, with build-time guards against the old coupling.

## 0.4.71 — 2026-10-11

- Make shell composition manifest-driven and add compiler-enforced coupling guards so module growth does not require expanding private shell knowledge.

## 0.4.70 — 2026-10-11

- Publish the validated Date & Time locality-boundary hardening, keeping locality ownership explicit and independently testable.

## 0.4.69 — 2026-10-11

- Finish the Date & Time ownership split: location, protected system-time operations and preview state now use their canonical bounded state objects instead of anonymous compatibility overlays and flat aliases.
- Replace the Linux shell white-box regression with a black-box lifecycle/UI contract so internal refactoring no longer requires preserving private controller fields, source inclusion or test-only compatibility wrappers.
- Keep the built-in module host lifecycle-only by moving Home temporal presentation behind a dedicated composition provider; remove the last shell geometry compatibility key.
- Revalidate the reduced architecture across Linux, Windows, ASan/UBSan, reproducible packaging, native release assets and Debian integration.

## 0.4.68 — 2026-10-10

- Pin Common 1.19.41 so both temporal preview surfaces consume native Edo bell numerals and localized branch wording, plus locale-correct explicit 12-hour markers and ordering.
- Consume Calendar 1.0.86's shared native cyclic-year date presentation for Chinese/Dangi previews; calendar conversion and preference authority remain in their existing owners.
- Add Home regressions for localized 12-hour markers and both Edo clock models without adding application-owned formatting.

## 0.4.67 — 2026-10-10

- Pin Common 1.19.40 and make both Home and Date & Time clock previews follow the desktop UI language. English Chinese-time labels use transliterated branch names and first/second-half wording; Chinese-language labels retain native script.
- Preserve the selected calendar, traditional clock units, Roman numerals and phase boundaries. Calendar's optional date provider follows the same UI-language preference.
- Add an English/Chinese preview regression without changing temporal policy storage.

## 0.4.66 — 2026-10-10

- Consume Common's corrected document-style Traditional Chinese double-hour presentation: Earthly-Branch time now renders as forms such as `申時`, with native `初`/`正` half-shí precision when finer detail is enabled instead of pinyin/English zodiac teaching labels.
- Consume Common's native Roman daylight precision so `Show seconds` means the closest historically meaningful subdivision (`unciae`) of the current unequal `hora`, while the four-watch night convention remains coarse rather than inventing modern minutes.
- Keep clock/calendar policy ownership unchanged: System Settings selects the presentation and Common remains the single formatter used by participating applications.

## 0.4.65 — 2026-10-07

- Make the built-in module host registry-driven so shell composition no longer hard-codes module lifecycle details.

## 0.4.64 — 2026-10-07

- Reduce shell/Date & Time coupling, restore pull-based central APT publication and stabilize shell teardown lifetime handling.

## 0.4.63 — 2026-10-06

- Decouple the Linux shell further, split Home presentation from its controller, centralize clock refresh cadence and keep native PGO/runtime paths on the same production policy.

## 0.4.62 — 2026-10-06

- Recover and harden the repository: deduplicate Linux setting-control presentation, restore efficient Home clock cadence and trusted Calendar validation, remove obsolete audit/PR workflow residue, restore the Common geometry contract and make the direct-to-`main` policy explicit.

## 0.4.61 — 2026-10-06

- Reduce Date & Time/shell coupling, centralize conventional clock-profile mapping, add configure-time shell-boundary guards and qualify authoritative builds on hosted runners.

## 0.4.60 — 2026-10-03

- Enforce MB Corpo typography throughout the packaged application and make the required font assets part of the validated release contract.
