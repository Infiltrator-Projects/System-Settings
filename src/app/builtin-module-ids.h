// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_BUILTIN_MODULE_IDS_H
#define SYSTEM_SETTINGS_BUILTIN_MODULE_IDS_H

/*
 * Composition-layer identities for built-in modules.
 *
 * These constants deliberately live beside the composition adapters rather
 * than in the generic shell or a domain public header. The host registry and
 * any cross-module composition provider must refer to the same semantic page
 * identity without independently repeating string literals.
 */
#define SS_BUILTIN_PAGE_ID_DATE_TIME "date-time"
#define SS_BUILTIN_MANIFEST_DATE_TIME "date-time.settings-module"

#endif
