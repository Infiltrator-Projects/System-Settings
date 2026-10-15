// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file home-status.c
 * @brief Home dashboard system-status observation and presentation data.
 */

#include "home-status.h"

#include <errno.h>
#include <locale.h>

#include <gio/gio.h>

typedef struct HomeStatusTicker {
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

bool ss_home_status_parse_uptime_seconds(
    const char *contents,
    guint64 *seconds_out)
{
    char *end = NULL;
    guint64 seconds;

    if (contents == NULL || seconds_out == NULL ||
        !g_ascii_isdigit(contents[0])) {
        return false;
    }

    /*
     * /proc/uptime is kernel-generated machine text: its first field is an
     * ASCII decimal number with a period regardless of LC_NUMERIC. Using the
     * process locale here made comma-decimal desktop locales parse only the
     * integer prefix. Parse the integer seconds directly, then validate the
     * optional ASCII fraction and field separator without involving locale.
     */
    errno = 0;
    seconds = g_ascii_strtoull(contents, &end, 10);
    if (errno == ERANGE || end == NULL || end == contents) {
        return false;
    }

    if (*end == '.') {
        ++end;
        if (!g_ascii_isdigit(*end)) {
            return false;
        }
        while (g_ascii_isdigit(*end)) {
            ++end;
        }
    }
    if (!g_ascii_isspace(*end)) {
        return false;
    }

    *seconds_out = seconds;
    return true;
}

static gchar *format_uptime(void)
{
    g_autofree gchar *contents = NULL;
    guint64 total = 0U;

    if (!g_file_get_contents("/proc/uptime", &contents, NULL, NULL) ||
        !ss_home_status_parse_uptime_seconds(contents, &total)) {
        return g_strdup("Unknown");
    }

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
