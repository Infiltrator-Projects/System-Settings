<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Roadmap

This is an ordered direction document, not a dated promise. Source, tests and release notes define what is actually supported.

## Phase 0 — Foundation

Current work:

- establish product architecture and documentation ownership;
- define shell/module separation;
- define trust and privilege model;
- define static manifest + lazy module ABI direction;
- define search/deep-link model;
- define validation criteria before code begins.

Phase 0 is complete when the documents agree and no core design question is hidden inside assumed implementation.

## Phase 1 — Shell and module framework

Build the minimum real product skeleton:

- native application shell;
- exact Common pin;
- Day/Night/Follow-system presentation through Common;
- category navigation;
- back/forward navigation;
- manifest parser and validator;
- module catalogue;
- stable v1 module ABI header;
- lazy loading;
- deep-link routing;
- global static search index;
- module load diagnostics;
- CI, sanitizer and packaging foundations.

The phase should include at least one deliberately small real module so the ABI is validated by use rather than by headers alone.

A System Information module is a good candidate because it can prove loading, search, navigation and read-only backend behaviour without forcing privilege design to carry the first milestone.

## Phase 2 — User/session settings

Implement settings that can establish the UX without broad system privilege:

- Appearance/theme;
- Fonts/desktop preferences where authoritative APIs permit;
- Keyboard;
- Mouse/Touchpad;
- Startup/session-facing settings where appropriate.

Requirements:

- external change reconciliation;
- per-setting search/deep links;
- no startup-wide module loading;
- no project shadow database.

## Phase 3 — Hardware and service domains

Add domains whose authoritative state is provided by major desktop/system services:

- Displays;
- Sound;
- Network;
- Bluetooth;
- Printers;
- Power.

Each domain begins with backend research and fixtures/tests before the UI is declared complete.

Special acceptance includes:

- display rollback after unsafe mode changes where supported;
- network staged transactions and disconnect consequences;
- device hotplug handling;
- service-unavailable states;
- cancellation/destruction during discovery.

## Phase 4 — Protected/system-wide domains

Add settings requiring explicit authorisation or higher-risk writes:

- Users;
- Date & Time system changes;
- selected storage/system operations;
- security/firewall surfaces that can be represented safely.

This phase must prove:

- shell remains unprivileged;
- read-only viewing does not prompt for credentials;
- authorisation is operation-scoped;
- cancellation/denial is handled cleanly;
- privileged backend input is narrow and validated;
- no project component stores administrator passwords.

## Phase 5 — Mint/Cinnamon replacement audit

Perform a forensic capability audit against the target Linux Mint/Cinnamon Settings environment.

The goal is functional coverage, not visual cloning.

For each target setting:

- identify the authoritative backend;
- decide whether System Settings owns it, deep-links to another specialised application, or intentionally excludes it;
- implement/test missing owned behaviour;
- document exclusions with rationale;
- test migration/side-by-side behaviour with Mint defaults.

Existing project ownership is preserved during this audit: package/update workflows belong to Infiltrator Software, live monitoring belongs to System Monitor, and filesystem defragmentation/recovery belongs to Defragmenter. Settings may supply launch/deep-link entries instead of duplicate engines.

Feature parity is not declared from a count of panels. Important behaviour, recovery, accessibility and privilege correctness must be equivalent or stronger for the supported target.

## Phase 6 — Release hardening

Before 1.0:

- complete dependency audit;
- complete Common bidirectional reuse pass;
- fuzz/boundary-test manifest parsing and other external data parsers;
- run ASan/UBSan and lifecycle stress;
- test supported Mint/Cinnamon versions and representative hardware;
- validate packaging ownership and upgrade/uninstall paths;
- validate search catalogue completeness;
- validate all deep links;
- validate keyboard/accessibility paths;
- verify no settings-open authentication prompt;
- verify release artifacts derive from the exact tested commit.

## Longer-term possibilities

Only after the first-party product is stable:

- richer dynamic search results from loaded modules;
- out-of-process third-party settings providers;
- additional Linux desktop backends;
- additional operating-system backends if the shell/module contract remains meaningful;
- settings export/diagnostic reports that do not become a shadow state store.

These are not commitments and must pass normal admission criteria.

## Admission rule

A roadmap capability is admitted when:

- ownership is clear;
- an authoritative backend exists or can be implemented;
- failure/permission semantics are understood;
- the feature can be validated;
- adding it does not force unrelated domain policy into the shell.

## Completion rule

A roadmap item is complete when implementation, tests, user-visible behaviour and maintained documentation agree. Compilation alone is not completion.
