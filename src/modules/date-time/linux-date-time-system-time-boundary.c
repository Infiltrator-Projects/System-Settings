// SPDX-License-Identifier: GPL-3.0-or-later
#include "linux-date-time-panel-private.h"

SsLinuxDateTimeSystemTimeState *ss_linux_date_time_system_time_state(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? &panel->system_time : NULL;
}

const SsLinuxDateTimeSystemTimeState *ss_linux_date_time_system_time_state_const(
    const SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? &panel->system_time : NULL;
}

const InfiltratrTemporalPolicyV3 *ss_linux_date_time_system_time_policy(
    const SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? ss_date_time_model_policy(&panel->model) : NULL;
}

SsLinuxDateTimeLifetime *ss_linux_date_time_system_time_lifetime(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? panel->lifetime : NULL;
}
