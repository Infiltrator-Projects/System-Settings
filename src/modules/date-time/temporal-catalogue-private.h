// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_TEMPORAL_CATALOGUE_PRIVATE_H
#define SYSTEM_SETTINGS_TEMPORAL_CATALOGUE_PRIVATE_H

#include <infiltratr/temporal.h>
#include <infiltratr/temporal_runtime.h>

/* Settings enumerates validated installed implementations. OS-following
 * standard remains a policy selector; every explicit clock needs a module. */
#define infiltratr_temporal_calendar_count() \
    infiltratr_temporal_installed_calendar_count()
#define infiltratr_temporal_calendar_at(index_) \
    infiltratr_temporal_installed_calendar_at((index_))
#define infiltratr_temporal_calendar_find(id_) \
    infiltratr_temporal_installed_calendar_find((id_))

#define infiltratr_temporal_clock_mode_count() \
    infiltratr_temporal_installed_clock_mode_count()
#define infiltratr_temporal_clock_mode_at(index_) \
    infiltratr_temporal_installed_clock_mode_at((index_))
#define infiltratr_temporal_clock_mode_find(id_) \
    infiltratr_temporal_installed_clock_mode_find((id_))

#endif
