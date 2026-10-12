// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_BUILTIN_MODULE_IDS_H
#define SYSTEM_SETTINGS_BUILTIN_MODULE_IDS_H

/*
 * Composition-layer identities for built-in modules.
 *
 * This is the single source of truth for identities shared by composition
 * adapters. These constants deliberately live beside those adapters rather
 * than in the generic shell or a domain public header. The host registry,
 * Home routing and any future cross-module composition provider must therefore
 * refer to the same semantic identity without repeating string literals.
 */
#define SS_BUILTIN_PAGE_ID_DATE_TIME "date-time"
#define SS_BUILTIN_MANIFEST_DATE_TIME "date-time.settings-module"

#endif
