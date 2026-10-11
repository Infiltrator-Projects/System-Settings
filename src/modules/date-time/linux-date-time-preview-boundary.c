// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-date-time-preview-boundary.c
 * @brief Narrow projection from the Date & Time controller to live preview.
 */

#include "linux-date-time-panel-private.h"

SsLinuxDateTimePreviewState *ss_linux_date_time_preview_state(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? &panel->preview : NULL;
}

SsDateTimeModel *ss_linux_date_time_preview_model(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? &panel->model : NULL;
}

SsRegionalContext *ss_linux_date_time_preview_regional_context(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? &panel->regional_context : NULL;
}

GtkWidget *ss_linux_date_time_preview_root(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? panel->root : NULL;
}

bool ss_linux_date_time_preview_desktop_uses_24h(
    const SsLinuxDateTimePanel *panel)
{
    return ss_linux_date_time_panel_desktop_uses_24h(panel);
}
