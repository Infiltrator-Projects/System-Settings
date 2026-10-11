#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def remove_struct_typedef(text: str, tag: str, label: str) -> str:
    start = text.find(f"typedef struct {tag} {{")
    if start < 0:
        raise RuntimeError(f"{label}: typedef start not found")
    end_token = f"}} {tag};"
    end = text.find(end_token, start)
    if end < 0:
        raise RuntimeError(f"{label}: typedef end not found")
    end += len(end_token)
    while end < len(text) and text[end] == "\n":
        end += 1
    return text[:start] + text[end:]


def _matching_brace(text: str, open_index: int) -> int:
    depth = 0
    state = "normal"
    i = open_index
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if state == "normal":
            if ch == '"':
                state = "string"
            elif ch == "'":
                state = "char"
            elif ch == "/" and nxt == "/":
                state = "line-comment"
                i += 1
            elif ch == "/" and nxt == "*":
                state = "block-comment"
                i += 1
            elif ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return i
        elif state == "string":
            if ch == "\\":
                i += 1
            elif ch == '"':
                state = "normal"
        elif state == "char":
            if ch == "\\":
                i += 1
            elif ch == "'":
                state = "normal"
        elif state == "line-comment":
            if ch == "\n":
                state = "normal"
        elif state == "block-comment":
            if ch == "*" and nxt == "/":
                state = "normal"
                i += 1
        i += 1
    raise RuntimeError("unterminated C function body")


def function_span(text: str, name: str) -> tuple[int, int]:
    pattern = re.compile(rf"(?m)^[ \t]*(?:static[ \t]+)?[^\n;{{}}]*\b{re.escape(name)}[ \t]*\(")
    for match in pattern.finditer(text):
        brace = text.find("{", match.end())
        semi = text.find(";", match.end())
        if brace < 0:
            continue
        if semi >= 0 and semi < brace:
            continue
        end = _matching_brace(text, brace) + 1
        start = match.start()
        while start > 0 and text[start - 1] == "\n":
            start -= 1
            if start > 0 and text[start - 1] != "\n":
                break
        while end < len(text) and text[end] == "\n":
            end += 1
        return start, end
    raise RuntimeError(f"function not found: {name}")


def remove_function(text: str, name: str) -> tuple[str, str]:
    start, end = function_span(text, name)
    return text[:start] + text[end:], text[start:end].strip()


def remove_functions(text: str, names: list[str]) -> tuple[str, dict[str, str]]:
    extracted: dict[str, str] = {}
    for name in names:
        text, body = remove_function(text, name)
        extracted[name] = body
    return text, extracted


def refactor_preview() -> None:
    panel_path = "src/modules/date-time/linux-date-time-panel.c"
    private_path = "src/modules/date-time/linux-date-time-panel-private.h"
    panel = read(panel_path)
    private = read(private_path)

    preview_names = [
        "ensure_calendar_preview_provider",
        "preview_coordinates",
        "format_preview_at",
        "refresh_preview",
        "refresh_preview_once",
        "stop_preview_timer",
        "ss_linux_date_time_panel_preview_interval_ms",
        "restart_preview_timer",
        "on_panel_mapped",
        "on_panel_unmapped",
    ]
    panel, extracted = remove_functions(panel, preview_names)

    panel = panel.replace("static gboolean refresh_preview(gpointer user_data);\n", "")
    panel = panel.replace("static void restart_preview_timer(SsLinuxDateTimePanel *state);\n", "")
    panel = panel.replace("restart_preview_timer(state)", "ss_linux_date_time_preview_restart(state)")
    panel = panel.replace("refresh_preview(state)", "ss_linux_date_time_preview_refresh(state)")

    panel = replace_once(
        panel,
        "static bool desktop_uses_24h(const SsLinuxDateTimePanel *state)",
        "bool ss_linux_date_time_panel_desktop_uses_24h(const SsLinuxDateTimePanel *state)",
        "rename desktop clock helper",
    )
    panel = panel.replace("desktop_uses_24h(state)", "ss_linux_date_time_panel_desktop_uses_24h(state)")

    panel = replace_once(
        panel,
        "    state->preview.calendar_provider =\n        ss_calendar_preview_provider_new();\n\n",
        "    ss_linux_date_time_preview_init(state);\n\n",
        "preview init",
    )
    panel = panel.replace(
        "            ss_calendar_preview_provider_free(\n                state->preview.calendar_provider);\n",
        "            ss_linux_date_time_preview_dispose(state);\n",
    )
    panel = replace_once(
        panel,
        "    g_signal_connect(\n        state->root, \"map\",\n        G_CALLBACK(on_panel_mapped), state);\n    g_signal_connect(\n        state->root, \"unmap\",\n        G_CALLBACK(on_panel_unmapped), state);\n",
        "    ss_linux_date_time_preview_attach(state);\n",
        "preview signal attachment",
    )
    panel = replace_once(
        panel,
        "    state->preview.idle_id =\n        g_idle_add(refresh_preview_once, state);\n",
        "    ss_linux_date_time_preview_schedule_initial(state);\n",
        "preview deferred start",
    )
    panel = replace_once(
        panel,
        "    stop_preview_timer(state);\n",
        "    ss_linux_date_time_preview_dispose(state);\n",
        "preview teardown entry",
    )
    panel = re.sub(
        r"\n    if \(state->preview\.idle_id != 0U\) \{\n"
        r"        g_source_remove\(state->preview\.idle_id\);\n"
        r"        state->preview\.idle_id = 0U;\n"
        r"    \}\n",
        "\n",
        panel,
        count=1,
    )
    panel = re.sub(
        r"\n    g_clear_pointer\(&state->preview\.date_calendar, g_free\);\n"
        r"    ss_calendar_preview_provider_free\(\n"
        r"        state->preview\.calendar_provider\);\n"
        r"    state->preview\.calendar_provider = NULL;\n",
        "\n",
        panel,
        count=1,
    )

    guard = "#ifdef SYSTEM_SETTINGS_DATE_TIME_PREVIEW_BOUNDARY\n#error \"Preview must use linux-date-time-preview-private.h, not the complete Date & Time panel controller.\"\n#endif\n\n"
    private = replace_once(
        private,
        "#define SYSTEM_SETTINGS_LINUX_DATE_TIME_PANEL_PRIVATE_H\n\n",
        "#define SYSTEM_SETTINGS_LINUX_DATE_TIME_PANEL_PRIVATE_H\n\n" + guard,
        "preview boundary guard",
    )
    private = private.replace('#include "calendar-preview-provider.h"\n', '')
    private = replace_once(
        private,
        '#include "linux-date-time-location-private.h"\n',
        '#include "linux-date-time-location-private.h"\n#include "linux-date-time-preview-private.h"\n',
        "preview private include",
    )
    private = remove_struct_typedef(private, "SsLinuxDateTimePreviewState", "preview state")
    private = private.replace(
        "guint ss_linux_date_time_panel_preview_interval_ms(\n    const SsLinuxDateTimePanel *state);\n",
        "",
    )
    private = replace_once(
        private,
        "bool ss_linux_date_time_panel_read_system_time(\n    const SsLinuxDateTimePanel *state,\n    SsSystemTimeState *out);\n",
        "bool ss_linux_date_time_panel_read_system_time(\n    const SsLinuxDateTimePanel *state,\n    SsSystemTimeState *out);\nbool ss_linux_date_time_panel_desktop_uses_24h(\n    const SsLinuxDateTimePanel *state);\n",
        "desktop clock helper declaration",
    )

    preview_header = r'''// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_LINUX_DATE_TIME_PREVIEW_PRIVATE_H
#define SYSTEM_SETTINGS_LINUX_DATE_TIME_PREVIEW_PRIVATE_H

#include "calendar-preview-provider.h"
#include "linux-date-time-panel.h"

#include "system-settings/date-time-model.h"
#include "system-settings/regional-context.h"

#include <gtk/gtk.h>
#include <stdbool.h>

typedef struct SsLinuxDateTimePreviewState {
    GtkWidget *clock_label;
    GtkWidget *date_label;
    GtkWidget *overview_clock;
    GtkWidget *overview_calendar;

    SsCalendarPreviewProvider *calendar_provider;
    guint timer_id;
    guint idle_id;
    gint date_year;
    gint date_month;
    gint date_day;
    gchar *date_calendar;
    bool date_cache_valid;
} SsLinuxDateTimePreviewState;

/* Narrow compiler-visible projection used by linux-date-time-preview.c. */
SsLinuxDateTimePreviewState *ss_linux_date_time_preview_state(
    SsLinuxDateTimePanel *panel);
SsDateTimeModel *ss_linux_date_time_preview_model(
    SsLinuxDateTimePanel *panel);
SsRegionalContext *ss_linux_date_time_preview_regional_context(
    SsLinuxDateTimePanel *panel);
GtkWidget *ss_linux_date_time_preview_root(
    SsLinuxDateTimePanel *panel);
bool ss_linux_date_time_preview_desktop_uses_24h(
    const SsLinuxDateTimePanel *panel);

void ss_linux_date_time_preview_init(SsLinuxDateTimePanel *panel);
void ss_linux_date_time_preview_attach(SsLinuxDateTimePanel *panel);
void ss_linux_date_time_preview_schedule_initial(SsLinuxDateTimePanel *panel);
gboolean ss_linux_date_time_preview_refresh(gpointer panel);
void ss_linux_date_time_preview_restart(SsLinuxDateTimePanel *panel);
void ss_linux_date_time_preview_dispose(SsLinuxDateTimePanel *panel);
guint ss_linux_date_time_panel_preview_interval_ms(
    const SsLinuxDateTimePanel *panel);

#endif
'''

    preview_boundary = r'''// SPDX-License-Identifier: GPL-3.0-or-later
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
'''

    preview_code = "\n\n".join(extracted[name] for name in preview_names)
    preview_code = preview_code.replace(
        "state->preview.", "ss_linux_date_time_preview_state(state)->")
    preview_code = preview_code.replace(
        "&state->model", "ss_linux_date_time_preview_model(state)")
    preview_code = preview_code.replace(
        "state->regional_context.",
        "ss_linux_date_time_preview_regional_context(state)->",
    )
    preview_code = preview_code.replace(
        "state->root", "ss_linux_date_time_preview_root(state)")
    preview_code = preview_code.replace(
        "desktop_uses_24h(state)",
        "ss_linux_date_time_preview_desktop_uses_24h(state)",
    )
    preview_code = preview_code.replace(
        "static gboolean refresh_preview(",
        "gboolean ss_linux_date_time_preview_refresh(",
    )
    preview_code = preview_code.replace(
        "refresh_preview(", "ss_linux_date_time_preview_refresh(")
    preview_code = preview_code.replace(
        "static void restart_preview_timer(",
        "void ss_linux_date_time_preview_restart(",
    )
    preview_code = preview_code.replace(
        "restart_preview_timer(", "ss_linux_date_time_preview_restart(")

    preview_source = r'''// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-date-time-preview.c
 * @brief Live Date & Time presentation isolated from the main controller.
 *
 * This unit compiles against a narrow preview facade. It cannot include the
 * complete panel-private controller and therefore cannot reach locality,
 * protected system-time or unrelated widget/controller state by accident.
 */

#include "linux-date-time-preview-private.h"

#include "system-settings/native-clock-policy.h"

#include <infiltratr/temporal.h>

#include <stdint.h>
#include <string.h>

''' + preview_code + r'''

void ss_linux_date_time_preview_init(SsLinuxDateTimePanel *panel)
{
    (void)ensure_calendar_preview_provider(panel);
}

void ss_linux_date_time_preview_attach(SsLinuxDateTimePanel *panel)
{
    GtkWidget *root = ss_linux_date_time_preview_root(panel);

    if (root == NULL) {
        return;
    }
    g_signal_connect(root, "map", G_CALLBACK(on_panel_mapped), panel);
    g_signal_connect(root, "unmap", G_CALLBACK(on_panel_unmapped), panel);
}

void ss_linux_date_time_preview_schedule_initial(SsLinuxDateTimePanel *panel)
{
    SsLinuxDateTimePreviewState *preview =
        ss_linux_date_time_preview_state(panel);

    if (preview != NULL && preview->idle_id == 0U) {
        preview->idle_id = g_idle_add(refresh_preview_once, panel);
    }
}

void ss_linux_date_time_preview_dispose(SsLinuxDateTimePanel *panel)
{
    SsLinuxDateTimePreviewState *preview =
        ss_linux_date_time_preview_state(panel);

    if (preview == NULL) {
        return;
    }
    stop_preview_timer(panel);
    if (preview->idle_id != 0U) {
        g_source_remove(preview->idle_id);
        preview->idle_id = 0U;
    }
    g_clear_pointer(&preview->date_calendar, g_free);
    ss_calendar_preview_provider_free(preview->calendar_provider);
    preview->calendar_provider = NULL;
    preview->date_cache_valid = false;
}
'''

    write(panel_path, panel)
    write(private_path, private)
    write("src/modules/date-time/linux-date-time-preview-private.h", preview_header)
    write("src/modules/date-time/linux-date-time-preview-boundary.c", preview_boundary)
    write("src/modules/date-time/linux-date-time-preview.c", preview_source)


def refactor_home_status() -> None:
    path = "src/shell/linux/home-page.c"
    private_path = "src/shell/linux/home-page-private.h"
    text = read(path)
    private = read(private_path)

    status_functions = [
        "network_connectivity_text",
        "format_uptime",
        "current_language_label",
        "current_format_locale_label",
        "refresh_home_status",
        "home_status_source_changed",
        "stop_home_status_ticker",
        "start_home_status_ticker",
        "home_status_mapped",
        "home_status_unmapped",
        "home_status_ticker_free",
    ]
    text, _ = remove_functions(text, status_functions)
    text = replace_once(
        text,
        '#include "home-page-private.h"\n',
        '#include "home-page-private.h"\n#include "home-status.h"\n',
        "home status include",
    )
    private = remove_struct_typedef(private, "HomeStatusTicker", "home status ticker")

    start = text.find("    const char *format_locale = current_format_locale_label();\n")
    end_token = (
        "    const char *timezone_name =\n"
        "        local_zone != NULL ? g_time_zone_get_identifier(local_zone) : \"Unknown\";\n"
    )
    end = text.find(end_token, start)
    if start < 0 or end < 0:
        raise RuntimeError("home initial status declarations not found")
    end += len(end_token)
    text = text[:start] + "    SsHomeStatusSnapshot status;\n" + text[end:]

    temporal_end = (
        "        temporal.system_time_text = g_strdup(\"Unknown\");\n"
        "    }\n"
    )
    text = replace_once(
        text,
        temporal_end,
        temporal_end +
        "\n    ss_home_status_snapshot_init(&status);\n"
        "    ss_home_status_snapshot_read(&status);\n",
        "home status snapshot read",
    )

    initial_start = text.find("    if (gtk_settings != NULL) {\n")
    ui_start = text.find("    gtk_widget_add_css_class(page, \"home-page\");\n", initial_start)
    if initial_start < 0 or ui_start < 0:
        raise RuntimeError("home duplicated initial status block not found")
    text = text[:initial_start] + text[ui_start:]

    replacements = {
        "uptime_text": "status.uptime",
        "timezone_name": "status.timezone",
        "format_locale": "status.region_value",
        "region_detail": "status.region_detail",
        "theme_name != NULL ? theme_name : \"System theme\"": "status.appearance_value",
        "appearance_detail": "status.appearance_detail",
        "online ? \"Connected\" : \"Offline\"": "status.network_value",
        "network_detail": "status.network_detail",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)

    ticker_start = text.find("    HomeStatusTicker *status_ticker = g_new0(HomeStatusTicker, 1);\n")
    ticker_end_token = (
        "    g_object_set_data_full(\n"
        "        G_OBJECT(scroller),\n"
        "        \"system-settings-home-status-source\",\n"
        "        status_ticker,\n"
        "        home_status_ticker_free);\n"
    )
    ticker_end = text.find(ticker_end_token, ticker_start)
    if ticker_start < 0 or ticker_end < 0:
        raise RuntimeError("home status ticker attachment block not found")
    ticker_end += len(ticker_end_token)
    attach = '''    ss_home_status_attach(\n        scroller,\n        GTK_LABEL(uptime_label),\n        GTK_LABEL(date_timezone_label),\n        GTK_LABEL(region_value_label),\n        GTK_LABEL(region_detail_label),\n        GTK_LABEL(appearance_value_label),\n        GTK_LABEL(appearance_detail_label),\n        GTK_LABEL(network_value_label),\n        GTK_LABEL(network_detail_label));\n'''
    text = text[:ticker_start] + attach + text[ticker_end:]
    text = replace_once(
        text,
        "    g_free(theme_name);\n",
        "    ss_home_status_snapshot_clear(&status);\n",
        "home status snapshot clear",
    )

    header = r'''// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_HOME_STATUS_H
#define SYSTEM_SETTINGS_HOME_STATUS_H

#include <gtk/gtk.h>

typedef struct SsHomeStatusSnapshot {
    gchar *uptime;
    gchar *timezone;
    gchar *region_value;
    gchar *region_detail;
    gchar *appearance_value;
    gchar *appearance_detail;
    gchar *network_value;
    gchar *network_detail;
} SsHomeStatusSnapshot;

void ss_home_status_snapshot_init(SsHomeStatusSnapshot *snapshot);
void ss_home_status_snapshot_clear(SsHomeStatusSnapshot *snapshot);
void ss_home_status_snapshot_read(SsHomeStatusSnapshot *snapshot);

void ss_home_status_attach(
    GtkWidget *owner,
    GtkLabel *uptime,
    GtkLabel *date_timezone,
    GtkLabel *region_value,
    GtkLabel *region_detail,
    GtkLabel *appearance_value,
    GtkLabel *appearance_detail,
    GtkLabel *network_value,
    GtkLabel *network_detail);

#endif
'''

    source = r'''// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file home-status.c
 * @brief Home dashboard system-status observation and presentation data.
 */

#include "home-status.h"

#include <locale.h>
#include <stdio.h>

#include <gio/gio.h>

typedef struct HomeStatusTicker {
    GtkWidget *owner;
    GtkLabel *uptime;
    GtkLabel *date_timezone;
    GtkLabel *region_value;
    GtkLabel *region_detail;
    GtkLabel *appearance_value;
    GtkLabel *appearance_detail;
    GtkLabel *network_value;
    GtkLabel *network_detail;
    guint source_id;
    GtkSettings *settings;
    GNetworkMonitor *network;
    gulong theme_name_id;
    gulong theme_dark_id;
    gulong network_available_id;
    gulong network_metered_id;
    gulong network_connectivity_id;
} HomeStatusTicker;

static void label_set_text_if_changed(GtkLabel *label, const char *text)
{
    const char *next = text != NULL ? text : "";

    if (label != NULL && g_strcmp0(gtk_label_get_text(label), next) != 0) {
        gtk_label_set_text(label, next);
    }
}

static const char *network_connectivity_text(
    GNetworkConnectivity connectivity)
{
    switch (connectivity) {
    case G_NETWORK_CONNECTIVITY_LOCAL:
        return "Local network only";
    case G_NETWORK_CONNECTIVITY_LIMITED:
        return "Limited connectivity";
    case G_NETWORK_CONNECTIVITY_PORTAL:
        return "Sign-in required";
    case G_NETWORK_CONNECTIVITY_FULL:
        return "Full internet access";
    default:
        return "Connectivity unknown";
    }
}

static gchar *format_uptime(void)
{
    g_autofree gchar *contents = NULL;
    gsize length = 0U;
    double seconds = 0.0;

    if (!g_file_get_contents("/proc/uptime", &contents, &length, NULL) ||
        contents == NULL || sscanf(contents, "%lf", &seconds) != 1 ||
        seconds < 0.0) {
        return g_strdup("Unknown");
    }

    const guint64 total = (guint64)seconds;
    const guint64 days = total / 86400U;
    const guint64 hours = (total % 86400U) / 3600U;
    const guint64 minutes = (total % 3600U) / 60U;
    if (days > 0U) {
        return g_strdup_printf(
            "%" G_GUINT64_FORMAT "d %" G_GUINT64_FORMAT "h", days, hours);
    }
    if (hours > 0U) {
        return g_strdup_printf(
            "%" G_GUINT64_FORMAT "h %" G_GUINT64_FORMAT "m", hours, minutes);
    }
    return g_strdup_printf("%" G_GUINT64_FORMAT "m", minutes);
}

static const char *current_language_label(void)
{
    const gchar *const *languages = g_get_language_names();
    return languages != NULL && languages[0] != NULL
        ? languages[0]
        : "Unknown";
}

static const char *current_format_locale_label(void)
{
    const char *locale_name = setlocale(LC_TIME, NULL);
    return locale_name != NULL && locale_name[0] != '\0'
        ? locale_name
        : "Unknown";
}

void ss_home_status_snapshot_init(SsHomeStatusSnapshot *snapshot)
{
    if (snapshot != NULL) {
        *snapshot = (SsHomeStatusSnapshot){0};
    }
}

void ss_home_status_snapshot_clear(SsHomeStatusSnapshot *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    g_clear_pointer(&snapshot->uptime, g_free);
    g_clear_pointer(&snapshot->timezone, g_free);
    g_clear_pointer(&snapshot->region_value, g_free);
    g_clear_pointer(&snapshot->region_detail, g_free);
    g_clear_pointer(&snapshot->appearance_value, g_free);
    g_clear_pointer(&snapshot->appearance_detail, g_free);
    g_clear_pointer(&snapshot->network_value, g_free);
    g_clear_pointer(&snapshot->network_detail, g_free);
}

void ss_home_status_snapshot_read(SsHomeStatusSnapshot *snapshot)
{
    GtkSettings *settings;
    GNetworkMonitor *network;
    gchar *theme_name = NULL;
    gboolean prefer_dark = FALSE;
    gboolean online = FALSE;
    gboolean metered = FALSE;
    GNetworkConnectivity connectivity = G_NETWORK_CONNECTIVITY_LOCAL;
    const char *format_locale;
    const char *language_name;
    g_autoptr(GTimeZone) local_zone = NULL;

    if (snapshot == NULL) {
        return;
    }
    ss_home_status_snapshot_clear(snapshot);

    format_locale = current_format_locale_label();
    language_name = current_language_label();
    settings = gtk_settings_get_default();
    network = g_network_monitor_get_default();
    local_zone = g_time_zone_new_local();

    if (settings != NULL) {
        g_object_get(
            settings,
            "gtk-theme-name", &theme_name,
            "gtk-application-prefer-dark-theme", &prefer_dark,
            NULL);
    }
    if (network != NULL) {
        online = g_network_monitor_get_network_available(network);
        metered = g_network_monitor_get_network_metered(network);
        connectivity = g_network_monitor_get_connectivity(network);
    }

    snapshot->uptime = format_uptime();
    snapshot->timezone = g_strdup(
        local_zone != NULL ? g_time_zone_get_identifier(local_zone) : "Unknown");
    snapshot->region_value = g_strdup(format_locale);
    snapshot->region_detail = g_strdup_printf(
        "Date/time format locale • %s • Interface language • %s",
        format_locale,
        language_name);
    snapshot->appearance_value = g_strdup(
        theme_name != NULL ? theme_name : "System theme");
    snapshot->appearance_detail = g_strdup_printf(
        "%s presentation", prefer_dark ? "Dark" : "Light");
    snapshot->network_value = g_strdup(online ? "Connected" : "Offline");
    snapshot->network_detail = g_strdup_printf(
        "%s%s",
        network_connectivity_text(connectivity),
        metered ? " • Metered" : "");
    g_free(theme_name);
}

static gboolean refresh_home_status(gpointer user_data)
{
    HomeStatusTicker *ticker = user_data;
    SsHomeStatusSnapshot snapshot;

    if (ticker == NULL) {
        return G_SOURCE_REMOVE;
    }
    ss_home_status_snapshot_init(&snapshot);
    ss_home_status_snapshot_read(&snapshot);
    label_set_text_if_changed(ticker->uptime, snapshot.uptime);
    label_set_text_if_changed(ticker->date_timezone, snapshot.timezone);
    label_set_text_if_changed(ticker->region_value, snapshot.region_value);
    label_set_text_if_changed(ticker->region_detail, snapshot.region_detail);
    label_set_text_if_changed(ticker->appearance_value, snapshot.appearance_value);
    label_set_text_if_changed(ticker->appearance_detail, snapshot.appearance_detail);
    label_set_text_if_changed(ticker->network_value, snapshot.network_value);
    label_set_text_if_changed(ticker->network_detail, snapshot.network_detail);
    ss_home_status_snapshot_clear(&snapshot);
    return G_SOURCE_CONTINUE;
}

static void home_status_source_changed(
    GObject *object G_GNUC_UNUSED,
    GParamSpec *pspec G_GNUC_UNUSED,
    gpointer user_data)
{
    (void)refresh_home_status(user_data);
}

static void stop_home_status_ticker(HomeStatusTicker *ticker)
{
    if (ticker == NULL) {
        return;
    }
    if (ticker->source_id != 0U) {
        g_source_remove(ticker->source_id);
        ticker->source_id = 0U;
    }
    if (ticker->settings != NULL) {
        if (ticker->theme_name_id != 0U) {
            g_signal_handler_disconnect(ticker->settings, ticker->theme_name_id);
        }
        if (ticker->theme_dark_id != 0U) {
            g_signal_handler_disconnect(ticker->settings, ticker->theme_dark_id);
        }
    }
    if (ticker->network != NULL) {
        if (ticker->network_available_id != 0U) {
            g_signal_handler_disconnect(
                ticker->network, ticker->network_available_id);
        }
        if (ticker->network_metered_id != 0U) {
            g_signal_handler_disconnect(
                ticker->network, ticker->network_metered_id);
        }
        if (ticker->network_connectivity_id != 0U) {
            g_signal_handler_disconnect(
                ticker->network, ticker->network_connectivity_id);
        }
    }
    ticker->theme_name_id = 0U;
    ticker->theme_dark_id = 0U;
    ticker->network_available_id = 0U;
    ticker->network_metered_id = 0U;
    ticker->network_connectivity_id = 0U;
    ticker->settings = NULL;
    ticker->network = NULL;
}

static void start_home_status_ticker(HomeStatusTicker *ticker)
{
    if (ticker == NULL || ticker->source_id != 0U) {
        return;
    }

    ticker->settings = gtk_settings_get_default();
    ticker->network = g_network_monitor_get_default();
    if (ticker->settings != NULL) {
        ticker->theme_name_id = g_signal_connect(
            ticker->settings,
            "notify::gtk-theme-name",
            G_CALLBACK(home_status_source_changed),
            ticker);
        ticker->theme_dark_id = g_signal_connect(
            ticker->settings,
            "notify::gtk-application-prefer-dark-theme",
            G_CALLBACK(home_status_source_changed),
            ticker);
    }
    if (ticker->network != NULL) {
        ticker->network_available_id = g_signal_connect(
            ticker->network,
            "notify::network-available",
            G_CALLBACK(home_status_source_changed),
            ticker);
        ticker->network_metered_id = g_signal_connect(
            ticker->network,
            "notify::network-metered",
            G_CALLBACK(home_status_source_changed),
            ticker);
        ticker->network_connectivity_id = g_signal_connect(
            ticker->network,
            "notify::connectivity",
            G_CALLBACK(home_status_source_changed),
            ticker);
    }

    (void)refresh_home_status(ticker);
    ticker->source_id = g_timeout_add_seconds(
        60U, refresh_home_status, ticker);
    g_source_set_name_by_id(
        ticker->source_id,
        "[system-settings] visible Home system status");
}

static void home_status_mapped(
    GtkWidget *widget G_GNUC_UNUSED,
    gpointer user_data)
{
    start_home_status_ticker(user_data);
}

static void home_status_unmapped(
    GtkWidget *widget G_GNUC_UNUSED,
    gpointer user_data)
{
    stop_home_status_ticker(user_data);
}

static void home_status_ticker_free(gpointer data)
{
    HomeStatusTicker *ticker = data;

    if (ticker == NULL) {
        return;
    }
    stop_home_status_ticker(ticker);
    g_free(ticker);
}

void ss_home_status_attach(
    GtkWidget *owner,
    GtkLabel *uptime,
    GtkLabel *date_timezone,
    GtkLabel *region_value,
    GtkLabel *region_detail,
    GtkLabel *appearance_value,
    GtkLabel *appearance_detail,
    GtkLabel *network_value,
    GtkLabel *network_detail)
{
    HomeStatusTicker *ticker;

    if (owner == NULL) {
        return;
    }
    ticker = g_new0(HomeStatusTicker, 1);
    ticker->owner = owner;
    ticker->uptime = uptime;
    ticker->date_timezone = date_timezone;
    ticker->region_value = region_value;
    ticker->region_detail = region_detail;
    ticker->appearance_value = appearance_value;
    ticker->appearance_detail = appearance_detail;
    ticker->network_value = network_value;
    ticker->network_detail = network_detail;
    g_signal_connect(owner, "map", G_CALLBACK(home_status_mapped), ticker);
    g_signal_connect(owner, "unmap", G_CALLBACK(home_status_unmapped), ticker);
    g_object_set_data_full(
        G_OBJECT(owner),
        "system-settings-home-status-source",
        ticker,
        home_status_ticker_free);
}
'''

    write(path, text)
    write(private_path, private)
    write("src/shell/linux/home-status.h", header)
    write("src/shell/linux/home-status.c", source)


def refactor_shell_catalog() -> None:
    path = "src/shell/linux/shell-window.c"
    text = read(path)
    text, _ = remove_functions(
        text,
        [
            "load_module_manifest",
            "append_manifest_value",
            "manifest_string_or_fallback",
            "module_search_text_from_manifest",
        ],
    )
    text = replace_once(
        text,
        '#include "shell-command.h"\n',
        '#include "shell-command.h"\n#include "shell-module-catalog.h"\n',
        "shell catalog include",
    )

    declaration_start = text.find("        g_autoptr(GKeyFile) manifest =\n")
    declaration_end_token = "        g_autofree gchar *search_text = NULL;\n"
    declaration_end = text.find(declaration_end_token, declaration_start)
    if declaration_start < 0 or declaration_end < 0:
        raise RuntimeError("module metadata declarations not found")
    declaration_end += len(declaration_end_token)
    text = (
        text[:declaration_start]
        + "        SsShellModuleMetadata metadata = {0};\n"
        + text[declaration_end:]
    )

    load_start = text.find("        title = manifest_string_or_fallback(")
    load_end_token = "        search_text = module_search_text_from_manifest(\n            manifest, fallback_search);\n"
    load_end = text.find(load_end_token, load_start)
    if load_start < 0 or load_end < 0:
        raise RuntimeError("module metadata load block not found")
    load_end += len(load_end_token)
    load = (
        "        ss_shell_module_metadata_load(\n"
        "            manifest_filename, page_id, &metadata);\n"
    )
    text = text[:load_start] + load + text[load_end:]

    text = text.replace(
        "            icon,\n            title,\n            summary,\n            page_id,\n            search_text);",
        "            metadata.icon,\n            metadata.title,\n            metadata.summary,\n            page_id,\n            metadata.search_text);",
        1,
    )
    text = text.replace(
        "            title,\n            GTK_LIST_BOX_ROW(row));",
        "            metadata.title,\n            GTK_LIST_BOX_ROW(row));",
        1,
    )
    marker = (
        "        if (search_state->primary_module_page_id == NULL) {\n"
        "            search_state->primary_module_page_id = g_strdup(page_id);\n"
        "        }\n"
    )
    text = replace_once(
        text,
        marker,
        marker + "        ss_shell_module_metadata_clear(&metadata);\n",
        "module metadata cleanup",
    )

    header = r'''// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_SHELL_MODULE_CATALOG_H
#define SYSTEM_SETTINGS_SHELL_MODULE_CATALOG_H

#include <glib.h>

typedef struct SsShellModuleMetadata {
    gchar *title;
    gchar *summary;
    gchar *icon;
    gchar *search_text;
} SsShellModuleMetadata;

void ss_shell_module_metadata_load(
    const char *manifest_filename,
    const char *page_id,
    SsShellModuleMetadata *out);
void ss_shell_module_metadata_clear(SsShellModuleMetadata *metadata);

#endif
'''

    source = r'''// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file shell-module-catalog.c
 * @brief Manifest discovery and searchable presentation metadata for modules.
 */

#include "shell-module-catalog.h"

#ifndef SYSTEM_SETTINGS_MODULE_DIR
#define SYSTEM_SETTINGS_MODULE_DIR "/usr/share/infiltrator/system-settings/modules"
#endif
#ifndef SYSTEM_SETTINGS_MODULE_SOURCE_DIR
#define SYSTEM_SETTINGS_MODULE_SOURCE_DIR ""
#endif

static GKeyFile *load_module_manifest(const char *manifest_filename)
{
    const char *override_dir =
        g_getenv("SYSTEM_SETTINGS_MODULE_DIR_OVERRIDE");
    const bool override_active =
        override_dir != NULL && override_dir[0] != '\0';
    const char *directories[] = {
        override_active ? override_dir : SYSTEM_SETTINGS_MODULE_SOURCE_DIR,
        override_active ? NULL : SYSTEM_SETTINGS_MODULE_DIR,
        NULL
    };

    if (manifest_filename == NULL || manifest_filename[0] == '\0') {
        return NULL;
    }

    for (size_t directory_index = 0U;
         directories[directory_index] != NULL;
         ++directory_index) {
        g_autofree gchar *path = NULL;
        g_autoptr(GKeyFile) key_file = NULL;

        if (directories[directory_index][0] == '\0') {
            continue;
        }
        path = g_build_filename(
            directories[directory_index], manifest_filename, NULL);
        key_file = g_key_file_new();
        if (g_key_file_load_from_file(
                key_file, path, G_KEY_FILE_NONE, NULL)) {
            return g_steal_pointer(&key_file);
        }
    }
    return NULL;
}

static void append_manifest_value(
    GString *search,
    GKeyFile *key_file,
    const char *group,
    const char *key)
{
    g_autofree gchar *value = NULL;

    if (search == NULL || key_file == NULL || group == NULL || key == NULL) {
        return;
    }
    value = g_key_file_get_string(key_file, group, key, NULL);
    if (value != NULL && value[0] != '\0') {
        g_string_append_c(search, ' ');
        g_string_append(search, value);
    }
}

static gchar *manifest_string_or_fallback(
    GKeyFile *key_file,
    const char *key,
    const char *fallback)
{
    g_autofree gchar *value = NULL;

    if (key_file != NULL && key != NULL) {
        value = g_key_file_get_string(key_file, "Module", key, NULL);
        if (value != NULL && value[0] != '\0') {
            return g_steal_pointer(&value);
        }
    }
    return g_strdup(fallback != NULL ? fallback : "");
}

static gchar *module_search_text_from_manifest(
    GKeyFile *key_file,
    const char *fallback)
{
    g_auto(GStrv) groups = NULL;
    gsize group_count = 0U;
    GString *search;

    if (key_file == NULL) {
        return g_strdup(fallback != NULL ? fallback : "");
    }

    search = g_string_new("");
    append_manifest_value(search, key_file, "Module", "Title");
    append_manifest_value(search, key_file, "Module", "Summary");
    append_manifest_value(search, key_file, "Search", "Keywords");
    groups = g_key_file_get_groups(key_file, &group_count);
    for (gsize group_index = 0U;
         group_index < group_count;
         ++group_index) {
        if (g_str_has_prefix(groups[group_index], "Target ")) {
            append_manifest_value(
                search, key_file, groups[group_index], "Title");
            append_manifest_value(
                search, key_file, groups[group_index], "Keywords");
        }
    }
    if (search->len > 0U) {
        return g_string_free(search, FALSE);
    }
    g_string_free(search, TRUE);
    return g_strdup(fallback != NULL ? fallback : "");
}

void ss_shell_module_metadata_clear(SsShellModuleMetadata *metadata)
{
    if (metadata == NULL) {
        return;
    }
    g_clear_pointer(&metadata->title, g_free);
    g_clear_pointer(&metadata->summary, g_free);
    g_clear_pointer(&metadata->icon, g_free);
    g_clear_pointer(&metadata->search_text, g_free);
}

void ss_shell_module_metadata_load(
    const char *manifest_filename,
    const char *page_id,
    SsShellModuleMetadata *out)
{
    g_autoptr(GKeyFile) manifest = NULL;
    g_autofree gchar *fallback_search = NULL;

    if (out == NULL) {
        return;
    }
    ss_shell_module_metadata_clear(out);
    manifest = load_module_manifest(manifest_filename);
    out->title = manifest_string_or_fallback(manifest, "Title", page_id);
    out->summary = manifest_string_or_fallback(
        manifest, "Summary", "Settings module");
    out->icon = manifest_string_or_fallback(
        manifest, "Icon", "preferences-system-symbolic");
    fallback_search = g_strdup_printf(
        "%s %s",
        out->title != NULL ? out->title : "",
        out->summary != NULL ? out->summary : "");
    out->search_text = module_search_text_from_manifest(
        manifest, fallback_search);
}
'''

    write(path, text)
    write("src/shell/linux/shell-module-catalog.h", header)
    write("src/shell/linux/shell-module-catalog.c", source)


def update_build_and_docs() -> None:
    cmake = read("CMakeLists.txt")
    cmake = replace_once(
        cmake,
        "        src/modules/date-time/linux-date-time-panel.c\n"
        "        src/modules/date-time/linux-date-time-location-boundary.c\n",
        "        src/modules/date-time/linux-date-time-panel.c\n"
        "        src/modules/date-time/linux-date-time-preview.c\n"
        "        src/modules/date-time/linux-date-time-preview-boundary.c\n"
        "        src/modules/date-time/linux-date-time-location-boundary.c\n",
        "Date & Time preview sources",
    )
    locality_property = (
        "    set_source_files_properties(\n"
        "        src/modules/date-time/linux-date-time-location.c\n"
        "        PROPERTIES COMPILE_DEFINITIONS\n"
        "            SYSTEM_SETTINGS_DATE_TIME_LOCATION_BOUNDARY=1)\n"
    )
    preview_property = (
        "    set_source_files_properties(\n"
        "        src/modules/date-time/linux-date-time-preview.c\n"
        "        PROPERTIES COMPILE_DEFINITIONS\n"
        "            SYSTEM_SETTINGS_DATE_TIME_PREVIEW_BOUNDARY=1)\n"
    )
    cmake = replace_once(
        cmake,
        locality_property,
        locality_property + preview_property,
        "preview compile boundary",
    )
    cmake = replace_once(
        cmake,
        "        src/shell/linux/shell-command.c\n"
        "        src/shell/linux/home-page.c\n"
        "        src/shell/linux/shell-window.c)\n",
        "        src/shell/linux/shell-command.c\n"
        "        src/shell/linux/home-status.c\n"
        "        src/shell/linux/home-page.c\n"
        "        src/shell/linux/shell-module-catalog.c\n"
        "        src/shell/linux/shell-window.c)\n",
        "shell responsibility sources",
    )
    write("CMakeLists.txt", cmake)

    architecture = read("docs/ARCHITECTURE.md")
    architecture = replace_once(
        architecture,
        "  linux-date-time-panel.c\n"
        "    temporal-policy/native-setting/timedated coordination\n"
        "    async lifetime/cancellation\n"
        "  linux-date-time-location.c\n",
        "  linux-date-time-panel.c\n"
        "    policy reconciliation / protected system-time composition\n"
        "    async lifetime/cancellation\n"
        "  linux-date-time-preview.c\n"
        "    isolated live clock/calendar preview lifecycle\n"
        "  linux-date-time-location.c\n",
        "architecture preview split",
    )
    needle = (
        "The shell constructs Home first and creates the built-in Date & Time panel only\n"
        "on first selection. Time-zone catalogue parsing, policy observers and timedated\n"
        "proxy setup therefore do not participate in shell first paint.\n"
    )
    replacement = needle + (
        "\nHome system-status observation now lives behind `home-status.c`, while module\n"
        "manifest discovery/search metadata lives behind `shell-module-catalog.c`. The\n"
        "Home constructor and shell window therefore compose those concerns instead of\n"
        "owning their platform observation and manifest parsing machinery. Date & Time\n"
        "live preview similarly compiles through a narrow preview facade; it cannot\n"
        "include the complete panel-private controller or reach locality/protected\n"
        "system-time state by accident.\n"
    )
    architecture = replace_once(
        architecture, needle, replacement, "architecture responsibility note")
    write("docs/ARCHITECTURE.md", architecture)


def main() -> None:
    refactor_preview()
    refactor_home_status()
    refactor_shell_catalog()
    update_build_and_docs()


if __name__ == "__main__":
    main()
