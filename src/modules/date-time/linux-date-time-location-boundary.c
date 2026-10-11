// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-date-time-location-boundary.c
 * @brief Compiler-enforced facade between locality and the Date & Time panel.
 *
 * This is the only translation unit allowed to project the complete panel
 * controller into the locality sub-domain. linux-date-time-location.c compiles
 * against linux-date-time-location-private.h and therefore cannot dereference
 * unrelated system-time, preview or controller state.
 */

#include "linux-date-time-panel-private.h"

SsLinuxDateTimeLocationState *ss_linux_date_time_location_state(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? &panel->location : NULL;
}

const SsLinuxDateTimeLocationState *ss_linux_date_time_location_state_const(
    const SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? &panel->location : NULL;
}

SsDateTimeModel *ss_linux_date_time_location_model(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? &panel->model : NULL;
}

const SsDateTimeModel *ss_linux_date_time_location_model_const(
    const SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? &panel->model : NULL;
}

SsRegionalContext *ss_linux_date_time_location_regional_context(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? &panel->regional_context : NULL;
}

const SsRegionalContext *ss_linux_date_time_location_regional_context_const(
    const SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? &panel->regional_context : NULL;
}

SsLinuxDateTimeLifetime *ss_linux_date_time_location_lifetime(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? panel->lifetime : NULL;
}
