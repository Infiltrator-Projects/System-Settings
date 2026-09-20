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
