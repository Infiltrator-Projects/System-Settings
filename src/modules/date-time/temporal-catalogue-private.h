// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_TEMPORAL_CATALOGUE_PRIVATE_H
#define SYSTEM_SETTINGS_TEMPORAL_CATALOGUE_PRIVATE_H

#include <infiltratr/temporal.h>
#include <infiltratr/temporal_runtime.h>

/*
 * Date & Time consumes the installed calendar catalogue, not Common's
 * compatibility-only built-in array.  This private remapping keeps the two
 * large GTK controller units source-compatible while making filesystem
 * resources first-class selectors.  It is intentionally calendar-only:
 * Common can discover clock metadata, but an executable clock behaviour ABI
 * has not yet been qualified, so exposing metadata-only clocks would allow a
 * user to select something the formatter cannot execute.
 */
#define infiltratr_temporal_calendar_count() \
    infiltratr_temporal_installed_calendar_count()
#define infiltratr_temporal_calendar_at(index_) \
    infiltratr_temporal_installed_calendar_at((index_))
#define infiltratr_temporal_calendar_find(id_) \
    infiltratr_temporal_installed_calendar_find((id_))

#endif
