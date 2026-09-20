<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Security

System Settings is security-sensitive because it reads and changes persistent operating-system configuration.

## Supported source

Before the first numbered release, security fixes target current `main`. After release, fixes target current `main` and, where appropriate, the latest published release. Older releases should not be assumed to receive backports.

## Reporting

Do not open a public issue for a vulnerability that could expose credentials, permit privilege escalation, execute untrusted module code, bypass authorisation, corrupt persistent system configuration or compromise release/package integrity.

Use GitHub private vulnerability reporting when available. Otherwise contact `infiltratr@yandex.com` with the subject `System Settings security report`.

Include the affected commit/version, Linux Mint/Cinnamon environment, privilege context, affected module/backend, impact and a reliable reproduction. Sanitise unrelated private data.

## Trust boundaries

### Module loading

Initial module libraries are first-party code installed into trusted system-owned directories.

The loader must reject:

- user-writable arbitrary module locations;
- path traversal;
- unexpected absolute library paths;
- incompatible ABI versions;
- malformed manifests that exceed declared bounds.

An in-process shared library is not a sandbox. Future third-party modules require a separately designed isolation model.

### Privilege

The GUI shell runs as the normal user.

Protected operations use narrow native authorised interfaces. The application does not elevate the whole settings process merely because one control is privileged.

The project must not:

- store administrator passwords;
- implement a generic privileged command runner;
- pass arbitrary shell fragments through a privileged boundary;
- expose unrestricted file-write primitives through a privileged helper;
- silently convert an authentication cancellation into success.

If a project-owned privileged component is ever unavoidable, its public operation set must be small, typed, validated and independently auditable.

### Backend input

D-Bus responses, configuration files, service data, device metadata and kernel-exposed strings are external input. They can be malformed, stale, unavailable or changed concurrently.

Parsing, allocation and path handling must be bounded. State can change between validation and apply and must be rechecked where the consequence matters.

### Persistent writes

A settings write is security/correctness sensitive even when it is not privileged.

Modules should use atomic/transactional native facilities where available. Partial failure must not be hidden. A requested write should be reconciled with authoritative resulting state.

### Module manifests

Manifests are data, never scripts.

They cannot contain commands to execute. Library names are resolved only within trusted directories according to loader policy.

### Release integrity

Published releases must be built from the exact verified source revision. Tags/assets are immutable after publication. Dependency pins and Common revisions are part of the source/release identity.

## Authentication UX

Opening System Settings or a protected panel must not itself request administrator credentials.

Authentication is triggered only by the specific protected operation. Credential UI should remain owned by the platform authorisation agent rather than by System Settings.

## Response

Security defects are correctness defects. Reproduce the exact boundary, add a regression test where practical, fix the underlying contract and verify the environment actually affected.

## Disclosure

Public technical detail should follow a fix or clear mitigation. Testing must not damage or access third-party systems or data without authorisation.
