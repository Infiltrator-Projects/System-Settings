// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file home-page.c
 * @brief Linux Home dashboard presentation and live status lifecycle.
 */

#include "home-page.h"
#include "home-page-private.h"
#include "linux-ui-helpers.h"
#include "shell-command.h"

#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/utsname.h>

#ifndef SYSTEM_SETTINGS_UI_ASSET_DIR
#define SYSTEM_SETTINGS_UI_ASSET_DIR "/usr/share/infiltrator/system-settings/ui"
#endif
#ifndef SYSTEM_SETTINGS_UI_SOURCE_DIR
#define SYSTEM_SETTINGS_UI_SOURCE_DIR ""
#endif

static GtkWidget *make_feature(
    const char *icon_name,
    const char *title,
    const char *copy)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

    gtk_widget_add_css_class(box, "home-feature");
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_FILL);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(text, GTK_ALIGN_CENTER);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 22);
    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(
        GTK_BOX(text),
        ss_linux_ui_make_label(title, "home-feature-title"));
    gtk_box_append(
        GTK_BOX(text),
        ss_linux_ui_make_label(copy, "home-feature-copy"));
    gtk_box_append(GTK_BOX(box), text);
    return box;
}

static GtkWidget *append_overview_row(
    GtkGrid *grid,
    int row,
    const char *label,
    const char *value)
{
    GtkWidget *key = ss_linux_ui_make_label(label, "overview-key");
    GtkWidget *data = ss_linux_ui_make_label(
        value != NULL && value[0] != '\0' ? value : "Unknown",
        "overview-data");

    gtk_widget_set_halign(key, GTK_ALIGN_START);
    gtk_widget_set_halign(data, GTK_ALIGN_START);
    gtk_grid_attach(grid, key, 0, row, 1, 1);
    gtk_grid_attach(grid, data, 1, row, 1, 1);
    return data;
}

static void label_set_text_if_changed(GtkLabel *label, const char *text)
{
    const char *next = text != NULL ? text : "";

    if (label != NULL && g_strcmp0(gtk_label_get_text(label), next) != 0) {
        gtk_label_set_text(label, next);
    }
}

static gboolean refresh_home_temporal(gpointer user_data)
{
    HomeTemporalTicker *ticker = user_data;
    SsHomeTemporalSnapshot temporal;
    guint desired_interval;

    if (ticker == NULL || ticker->clock == NULL || ticker->date == NULL ||
        ticker->system_time == NULL || ticker->module_host == NULL) {
        return G_SOURCE_REMOVE;
    }

    ss_home_temporal_snapshot_init(&temporal);
    if (ss_builtin_module_host_format_home_temporal(
            ticker->module_host, &temporal)) {
        label_set_text_if_changed(ticker->clock, temporal.clock_text);
        label_set_text_if_changed(ticker->date, temporal.date_text);
        label_set_text_if_changed(
            ticker->system_time, temporal.system_time_text);
    }
    ss_home_temporal_snapshot_clear(&temporal);

    desired_interval =
        ss_builtin_module_host_home_temporal_refresh_interval_ms(
            ticker->module_host);
    if (desired_interval != ticker->interval_ms) {
        ticker->source_id = 0U;
        ticker->interval_ms = desired_interval;
        ticker->source_id = g_timeout_add(
            desired_interval, refresh_home_temporal, ticker);
        g_source_set_name_by_id(
            ticker->source_id,
            "[system-settings] visible Home temporal presentation");
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static void stop_home_temporal_ticker(HomeTemporalTicker *ticker)
{
    if (ticker != NULL && ticker->source_id != 0U) {
        g_source_remove(ticker->source_id);
        ticker->source_id = 0U;
    }
}

static void start_home_temporal_ticker(HomeTemporalTicker *ticker)
{
    if (ticker == NULL || ticker->source_id != 0U ||
        ticker->module_host == NULL) {
        return;
    }

    ticker->interval_ms =
        ss_builtin_module_host_home_temporal_refresh_interval_ms(
            ticker->module_host);
    (void)refresh_home_temporal(ticker);
    if (ticker->source_id == 0U) {
        ticker->source_id = g_timeout_add(
            ticker->interval_ms, refresh_home_temporal, ticker);
        g_source_set_name_by_id(
            ticker->source_id,
            "[system-settings] visible Home temporal presentation");
    }
}

static void home_temporal_mapped(
    GtkWidget *widget G_GNUC_UNUSED,
    gpointer user_data)
{
    start_home_temporal_ticker(user_data);
}

static void home_temporal_unmapped(
    GtkWidget *widget G_GNUC_UNUSED,
    gpointer user_data)
{
    stop_home_temporal_ticker(user_data);
}

static void home_temporal_ticker_free(gpointer data)
{
    HomeTemporalTicker *ticker = data;

    if (ticker == NULL) {
        return;
    }
    stop_home_temporal_ticker(ticker);
    g_free(ticker);
}

static void open_date_time_from_home(GtkButton *button, gpointer user_data)
{
    SsHomePageContext *context = user_data;

    (void)button;
    if (context != NULL && context->open_date_time != NULL) {
        context->open_date_time(context->open_date_time_data);
    }
}

static void launch_external_program(GtkButton *button, gpointer user_data)
{
    (void)user_data;
    (void)launch_command(
        GTK_WIDGET(button),
        g_object_get_data(G_OBJECT(button), "action-program"),
        g_object_get_data(G_OBJECT(button), "action-argument"));
}

static GtkWidget *make_quick_action(
    const char *icon_name,
    const char *title,
    const char *copy)
{
    GtkWidget *button = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 11);
    GtkWidget *icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *arrow = gtk_image_new_from_icon_name("go-next-symbolic");

    gtk_widget_add_css_class(button, "quick-action");
    gtk_widget_add_css_class(icon_wrap, "quick-action-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 30);
    gtk_box_append(GTK_BOX(icon_wrap), icon);
    gtk_box_append(GTK_BOX(box), icon_wrap);
    gtk_box_append(
        GTK_BOX(text),
        ss_linux_ui_make_label(title, "quick-action-title"));
    gtk_box_append(
        GTK_BOX(text),
        ss_linux_ui_make_label(copy, "quick-action-copy"));
    gtk_widget_set_hexpand(text, TRUE);
    gtk_box_append(GTK_BOX(box), text);
    gtk_image_set_pixel_size(GTK_IMAGE(arrow), 17);
    gtk_widget_add_css_class(arrow, "quick-action-arrow");
    gtk_box_append(GTK_BOX(box), arrow);
    gtk_button_set_child(GTK_BUTTON(button), box);
    return button;
}

static GtkWidget *make_program_action(
    const char *icon_name,
    const char *title,
    const char *copy,
    const char *program,
    const char *argument)
{
    GtkWidget *button = make_quick_action(icon_name, title, copy);
    g_autofree gchar *path = find_trusted_system_program(program);

    g_object_set_data_full(
        G_OBJECT(button), "action-program", g_strdup(path), g_free);
    g_object_set_data_full(
        G_OBJECT(button), "action-argument", g_strdup(argument), g_free);

    if (path == NULL) {
        gtk_widget_set_sensitive(button, FALSE);
        gtk_widget_set_tooltip_text(
            button, "This application is not installed.");
    } else {
        g_signal_connect(
            button, "clicked", G_CALLBACK(launch_external_program), NULL);
    }
    return button;
}

static GtkWidget *make_status_card(
    const char *css_class,
    const char *icon_name,
    const char *title,
    const char *value,
    const char *detail)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *value_label = ss_linux_ui_make_label(
        value != NULL && value[0] != '\0' ? value : "Unknown",
        "status-card-value");
    GtkWidget *detail_label = ss_linux_ui_make_label(
        detail != NULL ? detail : "",
        "status-card-detail");

    gtk_widget_add_css_class(card, "status-card");
    if (css_class != NULL && css_class[0] != '\0') {
        gtk_widget_add_css_class(card, css_class);
    }
    gtk_widget_add_css_class(heading, "status-card-heading");
    gtk_widget_add_css_class(icon_wrap, "status-card-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 21);
    gtk_box_append(GTK_BOX(icon_wrap), icon);
    gtk_box_append(GTK_BOX(heading), icon_wrap);
    gtk_box_append(
        GTK_BOX(heading),
        ss_linux_ui_make_label(title, "home-card-title"));
    gtk_box_append(GTK_BOX(card), heading);
    gtk_label_set_ellipsize(GTK_LABEL(value_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_wrap(GTK_LABEL(detail_label), TRUE);
    gtk_box_append(GTK_BOX(card), value_label);
    gtk_box_append(GTK_BOX(card), detail_label);
    g_object_set_data(G_OBJECT(card), "status-value-label", value_label);
    g_object_set_data(G_OBJECT(card), "status-detail-label", detail_label);
    return card;
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

static GtkWidget *make_ui_asset_picture(
    const char *filename,
    int width,
    int height,
    const char *css_class)
{
    g_autofree gchar *path = NULL;
    GtkWidget *picture;
    const char *override_dir;
    bool override_active;
    const char *asset_dir;

    if (filename == NULL || filename[0] == '\0') {
        return NULL;
    }

    override_dir = g_getenv("SYSTEM_SETTINGS_UI_ASSET_DIR_OVERRIDE");
    override_active = override_dir != NULL && override_dir[0] != '\0';
    asset_dir = override_active ? override_dir : SYSTEM_SETTINGS_UI_ASSET_DIR;

    path = g_build_filename(asset_dir, filename, NULL);
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR) &&
        !override_active && SYSTEM_SETTINGS_UI_SOURCE_DIR[0] != '\0') {
        g_clear_pointer(&path, g_free);
        path = g_build_filename(
            SYSTEM_SETTINGS_UI_SOURCE_DIR, filename, NULL);
    }
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        g_warning(
            "System Settings UI asset is missing: %s",
            path != NULL ? path : "(null)");
        return NULL;
    }

    picture = gtk_picture_new_for_filename(path);
    g_object_set_data_full(
        G_OBJECT(picture),
        "system-settings-ui-asset-path",
        g_strdup(path),
        g_free);
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
#if GTK_CHECK_VERSION(4, 8, 0)
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_COVER);
#else
    gtk_picture_set_keep_aspect_ratio(GTK_PICTURE(picture), FALSE);
#endif
    gtk_widget_set_size_request(picture, width, height);
    if (css_class != NULL) {
        gtk_widget_add_css_class(picture, css_class);
    }
    return picture;
}

GtkWidget *make_visual_panel(
    const char *filename,
    int width,
    int height,
    const char *css_class)
{
    GtkWidget *picture = make_ui_asset_picture(
        filename, width, height, css_class);
    GtkWidget *fallback;

    if (picture != NULL) {
        return picture;
    }

    fallback = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(fallback, "asset-missing");
    if (css_class != NULL) {
        gtk_widget_add_css_class(fallback, css_class);
    }
    gtk_widget_set_size_request(fallback, width, height);
    gtk_widget_set_tooltip_text(
        fallback, "Installed visual asset is unavailable.");
    return fallback;
}

static GtkWidget *make_theme_preview(
    const char *label,
    const char *class_name,
    gboolean selected)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    g_autofree gchar *asset_name = g_strdup_printf("%s.png", class_name);
    GtkWidget *preview = make_ui_asset_picture(
        asset_name, 76, 44, "theme-preview-window");

    if (preview == NULL) {
        preview = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(preview, "theme-preview-window");
        gtk_widget_add_css_class(preview, "asset-missing");
        gtk_widget_set_size_request(preview, 76, 44);
    }

    gtk_widget_add_css_class(box, "theme-preview");
    if (selected) {
        gtk_widget_add_css_class(box, "theme-preview-selected");
    }
    gtk_box_append(GTK_BOX(box), preview);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_box_append(
        GTK_BOX(box),
        ss_linux_ui_make_label(label, "theme-preview-label"));
    return box;
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

static gboolean refresh_home_status(gpointer user_data)
{
    HomeStatusTicker *ticker = user_data;
    g_autofree gchar *uptime = NULL;
    GtkSettings *settings;
    gchar *theme_name = NULL;
    gboolean prefer_dark = FALSE;
    GNetworkMonitor *network;
    gboolean online = FALSE;
    gboolean metered = FALSE;
    GNetworkConnectivity connectivity = G_NETWORK_CONNECTIVITY_LOCAL;
    const char *format_locale = current_format_locale_label();
    const char *language_name = current_language_label();
    g_autofree gchar *region_detail = NULL;
    g_autofree gchar *appearance_detail = NULL;
    g_autofree gchar *network_detail = NULL;
    g_autoptr(GTimeZone) local_zone = g_time_zone_new_local();

    if (ticker == NULL) {
        return G_SOURCE_REMOVE;
    }
    settings = ticker->settings != NULL
        ? ticker->settings
        : gtk_settings_get_default();
    network = ticker->network != NULL
        ? ticker->network
        : g_network_monitor_get_default();
    uptime = format_uptime();
    label_set_text_if_changed(ticker->uptime, uptime);
    if (local_zone != NULL) {
        label_set_text_if_changed(
            ticker->date_timezone, g_time_zone_get_identifier(local_zone));
    }
    label_set_text_if_changed(ticker->region_value, format_locale);
    region_detail = g_strdup_printf(
        "Date/time format locale • %s • Interface language • %s",
        format_locale,
        language_name);
    label_set_text_if_changed(ticker->region_detail, region_detail);
    if (settings != NULL) {
        g_object_get(
            settings,
            "gtk-theme-name", &theme_name,
            "gtk-application-prefer-dark-theme", &prefer_dark,
            NULL);
    }
    label_set_text_if_changed(
        ticker->appearance_value,
        theme_name != NULL ? theme_name : "System theme");
    appearance_detail = g_strdup_printf(
        "%s presentation", prefer_dark ? "Dark" : "Light");
    label_set_text_if_changed(
        ticker->appearance_detail, appearance_detail);
    if (network != NULL) {
        online = g_network_monitor_get_network_available(network);
        metered = g_network_monitor_get_network_metered(network);
        connectivity = g_network_monitor_get_connectivity(network);
    }
    label_set_text_if_changed(
        ticker->network_value, online ? "Connected" : "Offline");
    network_detail = g_strdup_printf(
        "%s%s",
        network_connectivity_text(connectivity),
        metered ? " • Metered" : "");
    label_set_text_if_changed(ticker->network_detail, network_detail);
    g_free(theme_name);
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

guint home_layout_columns_for_width(int width)
{
    return width >= 720 ? 2U : 1U;
}

void home_layout_apply_width(HomeAdaptiveLayout *layout, int width)
{
    const guint columns = home_layout_columns_for_width(width);

    if (layout == NULL || layout->applied_columns == columns) {
        return;
    }
    layout->applied_columns = columns;
    if (layout->hero != NULL) {
        gtk_widget_set_size_request(
            layout->hero, -1, columns == 2U ? 270 : 390);
    }
    if (layout->features != NULL) {
        gtk_flow_box_set_min_children_per_line(
            layout->features, columns == 2U ? 3U : 1U);
    }
    if (layout->primary_grid != NULL) {
        gtk_flow_box_set_min_children_per_line(
            layout->primary_grid, columns);
    }
    if (layout->status_grid != NULL) {
        gtk_flow_box_set_min_children_per_line(
            layout->status_grid, columns);
    }
    if (layout->appearance_previews != NULL) {
        gtk_flow_box_set_min_children_per_line(
            layout->appearance_previews, columns == 2U ? 4U : 1U);
    }
}

static void home_layout_width_changed(
    GObject *object,
    GParamSpec *pspec G_GNUC_UNUSED,
    gpointer user_data)
{
    const int width = gtk_widget_get_width(GTK_WIDGET(object));

    if (width > 0) {
        home_layout_apply_width(user_data, width);
    }
}

static void home_layout_mapped(GtkWidget *widget, gpointer user_data)
{
    const int width = gtk_widget_get_width(widget);

    if (width > 0) {
        home_layout_apply_width(user_data, width);
    }
}

GtkWidget *ss_linux_home_page_new(const SsHomePageContext *context)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *hero = gtk_overlay_new();
    GtkWidget *hero_scene = make_visual_panel(
        "hero-asset.png", -1, 270, "hero-scene");
    GtkWidget *hero_foreground = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    GtkWidget *hero_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *hero_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *hero_brand = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *hero_icon = gtk_image_new_from_icon_name(
        "video-display-symbolic");
    GtkWidget *features = gtk_flow_box_new();
    GtkWidget *grid = gtk_flow_box_new();
    GtkWidget *overview = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *overview_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *overview_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *overview_icon = gtk_image_new_from_icon_name(
        "video-display-symbolic");
    GtkWidget *overview_data = gtk_grid_new();
    GtkWidget *overview_body = gtk_flow_box_new();
    GtkWidget *system_time_label;
    GtkWidget *overview_scene = make_visual_panel(
        "overview-asset.png", 120, 96, "overview-scene");
    GtkWidget *quick = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *quick_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *quick_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *quick_icon = gtk_image_new_from_icon_name(
        "system-run-symbolic");
    GtkWidget *quick_grid = gtk_flow_box_new();
    GtkWidget *status_grid = gtk_flow_box_new();
    GtkWidget *date_card;
    GtkWidget *region_card;
    GtkWidget *appearance_card;
    GtkWidget *network_card;
    GtkWidget *date_open;
    GtkWidget *date_meta;
    GtkWidget *date_location;
    GtkWidget *region_body;
    GtkWidget *region_copy;
    GtkWidget *appearance_previews;
    GtkWidget *network_body;
    GtkWidget *scroller = gtk_scrolled_window_new();
    SsHomePageContext *stored_context = g_new0(SsHomePageContext, 1);
    struct utsname uts;
    g_autofree gchar *os_name = g_get_os_info(G_OS_INFO_KEY_PRETTY_NAME);
    const char *desktop = g_getenv("XDG_CURRENT_DESKTOP");
    const char *host_name = g_get_host_name();
    const int uname_result = uname(&uts);
    const char *kernel = uname_result == 0 ? uts.release : "Unknown";
    const char *architecture = uname_result == 0 ? uts.machine : "Unknown";
    const char *format_locale = current_format_locale_label();
    const char *language_name = current_language_label();
    GtkSettings *gtk_settings = gtk_settings_get_default();
    gchar *theme_name = NULL;
    gboolean prefer_dark = FALSE;
    GNetworkMonitor *network = g_network_monitor_get_default();
    gboolean online = FALSE;
    gboolean metered = FALSE;
    GNetworkConnectivity connectivity = G_NETWORK_CONNECTIVITY_LOCAL;
    SsHomeTemporalSnapshot temporal;
    g_autofree gchar *region_detail = NULL;
    g_autofree gchar *appearance_detail = NULL;
    g_autofree gchar *network_detail = NULL;
    g_autofree gchar *uptime_text = format_uptime();
    g_autoptr(GTimeZone) local_zone = g_time_zone_new_local();
    const char *timezone_name =
        local_zone != NULL ? g_time_zone_get_identifier(local_zone) : "Unknown";
    g_autofree gchar *os_display = g_strdup_printf(
        "Infiltrator OS (%s)", os_name != NULL ? os_name : "Linux");

    if (context != NULL) {
        *stored_context = *context;
    }
    g_object_set_data_full(
        G_OBJECT(scroller),
        "system-settings-home-context",
        stored_context,
        g_free);

    ss_home_temporal_snapshot_init(&temporal);
    if (stored_context->module_host == NULL ||
        !ss_builtin_module_host_format_home_temporal(
            stored_context->module_host, &temporal)) {
        temporal.clock_text = g_strdup("Unknown");
        temporal.date_text = g_strdup("");
        temporal.system_time_text = g_strdup("Unknown");
    }

    if (gtk_settings != NULL) {
        g_object_get(
            gtk_settings,
            "gtk-theme-name", &theme_name,
            "gtk-application-prefer-dark-theme", &prefer_dark,
            NULL);
    }
    if (network != NULL) {
        online = g_network_monitor_get_network_available(network);
        metered = g_network_monitor_get_network_metered(network);
        connectivity = g_network_monitor_get_connectivity(network);
    }

    region_detail = g_strdup_printf(
        "Date/time format locale • %s • Interface language • %s",
        format_locale,
        language_name);
    appearance_detail = g_strdup_printf(
        "%s presentation", prefer_dark ? "Dark" : "Light");
    network_detail = g_strdup_printf(
        "%s%s",
        network_connectivity_text(connectivity),
        metered ? " • Metered" : "");

    gtk_widget_add_css_class(page, "home-page");
    gtk_widget_add_css_class(hero, "home-hero");
    gtk_widget_set_margin_bottom(hero, 28);
    gtk_widget_set_size_request(hero, -1, 270);
    gtk_overlay_set_child(GTK_OVERLAY(hero), hero_scene);

    gtk_widget_add_css_class(hero_copy, "hero-copy-overlay");
    gtk_widget_set_valign(hero_copy, GTK_ALIGN_CENTER);
    gtk_box_append(
        GTK_BOX(hero_copy),
        ss_linux_ui_make_label("SYSTEM CONTROL", "home-hero-eyebrow"));
    gtk_box_append(
        GTK_BOX(hero_copy),
        ss_linux_ui_make_label("Welcome to", "home-hero-title"));
    gtk_box_append(
        GTK_BOX(hero_copy),
        ss_linux_ui_make_label("System Settings", "home-hero-accent"));
    gtk_box_append(
        GTK_BOX(hero_copy),
        ss_linux_ui_make_label(
            "Configure your system, your way.", "home-hero-subtitle"));

    gtk_widget_add_css_class(features, "home-feature-row");
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(features), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(features), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(features), 3U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(features), 10U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(features), 8U);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(features), TRUE);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(features),
        make_feature("emblem-ok-symbolic", "Simple", "Easy to use"), -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(features),
        make_feature("security-high-symbolic", "Secure", "Built for privacy"), -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(features),
        make_feature(
            "video-display-symbolic", "Beautiful", "A desktop you’ll love"), -1);
    gtk_box_append(GTK_BOX(hero_copy), features);

    gtk_widget_set_hexpand(hero_spacer, TRUE);
    gtk_widget_set_halign(hero_brand, GTK_ALIGN_END);
    gtk_widget_set_valign(hero_brand, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(hero_brand, "hero-brand-overlay");
    gtk_image_set_pixel_size(GTK_IMAGE(hero_icon), 48);
    gtk_box_append(GTK_BOX(hero_brand), hero_icon);
    gtk_box_append(
        GTK_BOX(hero_brand),
        ss_linux_ui_make_label("INFILTRATOR OS", "home-hero-mark-title"));
    gtk_box_append(
        GTK_BOX(hero_brand),
        ss_linux_ui_make_label(
            "GRAPHICAL SYSTEM CONTROL", "home-hero-mark-copy"));
    gtk_widget_set_hexpand(hero_foreground, TRUE);
    gtk_widget_set_vexpand(hero_foreground, TRUE);
    gtk_box_append(GTK_BOX(hero_foreground), hero_copy);
    gtk_box_append(GTK_BOX(hero_foreground), hero_spacer);
    gtk_box_append(GTK_BOX(hero_foreground), hero_brand);
    gtk_overlay_add_overlay(GTK_OVERLAY(hero), hero_foreground);
    gtk_overlay_set_measure_overlay(GTK_OVERLAY(hero), hero_foreground, TRUE);
    gtk_box_append(GTK_BOX(page), hero);

    gtk_widget_add_css_class(grid, "home-grid");
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(grid), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(grid), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(grid), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(grid), 12U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(grid), 12U);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(grid), TRUE);

    gtk_widget_add_css_class(overview, "home-card");
    gtk_widget_add_css_class(overview_icon_wrap, "home-card-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(overview_icon), 22);
    gtk_box_append(GTK_BOX(overview_icon_wrap), overview_icon);
    gtk_box_append(GTK_BOX(overview_heading), overview_icon_wrap);
    gtk_box_append(
        GTK_BOX(overview_heading),
        ss_linux_ui_make_label("System Overview", "home-card-title"));
    GtkWidget *overview_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *updates_button = gtk_button_new_with_label("Check for updates →");
    gtk_widget_set_hexpand(overview_spacer, TRUE);
    gtk_box_append(GTK_BOX(overview_heading), overview_spacer);
    gtk_widget_add_css_class(updates_button, "overview-link-button");
    g_autofree gchar *software_path =
        find_trusted_system_program("infiltrator-software");
    g_object_set_data_full(
        G_OBJECT(updates_button),
        "action-program",
        g_strdup(software_path),
        g_free);
    if (software_path == NULL) {
        gtk_widget_set_sensitive(updates_button, FALSE);
        gtk_widget_set_tooltip_text(
            updates_button, "Infiltrator Software is not installed.");
    } else {
        g_signal_connect(
            updates_button, "clicked", G_CALLBACK(launch_external_program), NULL);
    }
    gtk_box_append(GTK_BOX(overview_heading), updates_button);
    gtk_box_append(GTK_BOX(overview), overview_heading);
    gtk_grid_set_column_spacing(GTK_GRID(overview_data), 18);
    gtk_grid_set_row_spacing(GTK_GRID(overview_data), 8);
    append_overview_row(
        GTK_GRID(overview_data), 0, "Operating system", os_display);
    append_overview_row(GTK_GRID(overview_data), 1, "Kernel", kernel);
    append_overview_row(
        GTK_GRID(overview_data), 2, "Architecture", architecture);
    append_overview_row(GTK_GRID(overview_data), 3, "Desktop", desktop);
    append_overview_row(GTK_GRID(overview_data), 4, "Hostname", host_name);
    GtkWidget *uptime_label = append_overview_row(
        GTK_GRID(overview_data), 5, "Uptime", uptime_text);
    system_time_label = append_overview_row(
        GTK_GRID(overview_data), 6, "System time", temporal.system_time_text);
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(overview_body), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(overview_body), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(overview_body), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(overview_body), 15U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(overview_body), 10U);
    gtk_flow_box_insert(GTK_FLOW_BOX(overview_body), overview_scene, -1);
    gtk_widget_set_hexpand(overview_data, TRUE);
    gtk_flow_box_insert(GTK_FLOW_BOX(overview_body), overview_data, -1);
    gtk_box_append(GTK_BOX(overview), overview_body);
    gtk_flow_box_insert(GTK_FLOW_BOX(grid), overview, -1);

    gtk_widget_add_css_class(quick, "home-card");
    gtk_widget_add_css_class(quick_icon_wrap, "home-card-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(quick_icon), 22);
    gtk_box_append(GTK_BOX(quick_icon_wrap), quick_icon);
    gtk_box_append(GTK_BOX(quick_heading), quick_icon_wrap);
    gtk_box_append(
        GTK_BOX(quick_heading),
        ss_linux_ui_make_label("Quick Actions", "home-card-title"));
    gtk_box_append(GTK_BOX(quick), quick_heading);
    gtk_widget_add_css_class(quick_grid, "quick-action-grid");
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(quick_grid), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(quick_grid), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(quick_grid), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(quick_grid), 8U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(quick_grid), 8U);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(quick_grid), TRUE);

    GtkWidget *date_action = make_quick_action(
        "preferences-system-time-symbolic",
        "Set Date & Time",
        "Time zone, clock & calendar");
    g_signal_connect(
        date_action,
        "clicked",
        G_CALLBACK(open_date_time_from_home),
        stored_context);
    gtk_flow_box_insert(GTK_FLOW_BOX(quick_grid), date_action, -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(quick_grid),
        make_program_action(
            "preferences-desktop-locale-symbolic",
            "Change Region",
            "Language, formats & location",
            "mintlocale",
            NULL),
        -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(quick_grid),
        make_program_action(
            "video-display-symbolic",
            "Configure Display",
            "Scaling, layout & monitors",
            "cinnamon-settings",
            "display"),
        -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(quick_grid),
        make_program_action(
            "system-software-install-symbolic",
            "Software & Updates",
            "Apps, packages & updates",
            "infiltrator-software",
            NULL),
        -1);
    gtk_box_append(GTK_BOX(quick), quick_grid);
    gtk_flow_box_insert(GTK_FLOW_BOX(grid), quick, -1);
    gtk_box_append(GTK_BOX(page), grid);

    gtk_widget_add_css_class(status_grid, "status-grid");
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(status_grid), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(status_grid), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(status_grid), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(status_grid), 12U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(status_grid), 12U);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(status_grid), FALSE);

    date_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 9);
    gtk_widget_add_css_class(date_card, "status-card");
    gtk_widget_add_css_class(date_card, "date-status-card");
    GtkWidget *date_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *date_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *date_icon = gtk_image_new_from_icon_name(
        "preferences-system-time-symbolic");
    gtk_widget_add_css_class(date_icon_wrap, "status-card-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(date_icon), 22);
    gtk_box_append(GTK_BOX(date_icon_wrap), date_icon);
    gtk_box_append(GTK_BOX(date_heading), date_icon_wrap);
    gtk_box_append(
        GTK_BOX(date_heading),
        ss_linux_ui_make_label("Date & Time", "home-card-title"));
    date_open = gtk_button_new_from_icon_name("go-next-symbolic");
    gtk_widget_set_tooltip_text(date_open, "Open Date & Time");
    gtk_accessible_update_property(
        GTK_ACCESSIBLE(date_open),
        GTK_ACCESSIBLE_PROPERTY_LABEL,
        "Open Date & Time",
        -1);
    gtk_widget_add_css_class(date_open, "card-arrow");
    gtk_widget_set_halign(date_open, GTK_ALIGN_END);
    gtk_widget_set_hexpand(date_open, TRUE);
    g_signal_connect(
        date_open,
        "clicked",
        G_CALLBACK(open_date_time_from_home),
        stored_context);
    gtk_box_append(GTK_BOX(date_heading), date_open);
    gtk_box_append(GTK_BOX(date_card), date_heading);

    GtkWidget *date_body = gtk_flow_box_new();
    GtkWidget *date_clock = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *date_clock_label = ss_linux_ui_make_label(
        temporal.clock_text, "home-clock-value");
    GtkWidget *date_calendar_label = ss_linux_ui_make_label(
        temporal.date_text, "home-clock-date");
    gtk_box_append(GTK_BOX(date_clock), date_clock_label);
    gtk_label_set_wrap(GTK_LABEL(date_calendar_label), TRUE);
    gtk_box_append(GTK_BOX(date_clock), date_calendar_label);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(date_body), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(date_body), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(date_body), 3U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(date_body), 16U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(date_body), 10U);
    gtk_flow_box_insert(GTK_FLOW_BOX(date_body), date_clock, -1);

    date_meta = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *pin_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *pin = gtk_image_new_from_icon_name("mark-location-symbolic");
    gtk_widget_add_css_class(pin_wrap, "location-pin-well");
    gtk_image_set_pixel_size(GTK_IMAGE(pin), 24);
    gtk_box_append(GTK_BOX(pin_wrap), pin);
    gtk_box_append(GTK_BOX(date_meta), pin_wrap);
    date_location = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *date_timezone_label = ss_linux_ui_make_label(
        timezone_name, "location-primary");
    gtk_box_append(GTK_BOX(date_location), date_timezone_label);
    gtk_box_append(
        GTK_BOX(date_location),
        ss_linux_ui_make_label(
            "Authoritative local time zone", "status-card-detail"));
    gtk_box_append(GTK_BOX(date_meta), date_location);
    gtk_flow_box_insert(GTK_FLOW_BOX(date_body), date_meta, -1);
    GtkWidget *date_scene = make_visual_panel(
        "date-asset.png", 120, 78, "date-scene");
    gtk_widget_set_hexpand(date_scene, TRUE);
    gtk_widget_set_halign(date_scene, GTK_ALIGN_END);
    gtk_flow_box_insert(GTK_FLOW_BOX(date_body), date_scene, -1);
    gtk_box_append(GTK_BOX(date_card), date_body);
    gtk_flow_box_insert(GTK_FLOW_BOX(status_grid), date_card, -1);

    region_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(region_card, "status-card");
    gtk_widget_add_css_class(region_card, "region-status-card");
    GtkWidget *region_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *region_icon = gtk_image_new_from_icon_name(
        "preferences-desktop-locale-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(region_icon), 23);
    gtk_box_append(GTK_BOX(region_heading), region_icon);
    gtk_box_append(
        GTK_BOX(region_heading),
        ss_linux_ui_make_label("Region & Language", "home-card-title"));
    gtk_box_append(GTK_BOX(region_card), region_heading);
    region_body = gtk_flow_box_new();
    region_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *region_value_label = ss_linux_ui_make_label(
        format_locale, "region-country");
    GtkWidget *region_detail_label = ss_linux_ui_make_label(
        region_detail, "status-card-detail");
    gtk_label_set_ellipsize(
        GTK_LABEL(region_value_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_wrap(GTK_LABEL(region_detail_label), TRUE);
    gtk_box_append(GTK_BOX(region_copy), region_value_label);
    gtk_box_append(GTK_BOX(region_copy), region_detail_label);
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(region_body), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(region_body), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(region_body), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(region_body), 13U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(region_body), 10U);
    gtk_flow_box_insert(GTK_FLOW_BOX(region_body), region_copy, -1);
    GtkWidget *region_scene = make_ui_asset_picture(
        "region-asset.png", 220, 96, "region-scene");
    if (region_scene != NULL) {
        gtk_widget_set_hexpand(region_scene, TRUE);
        gtk_widget_set_halign(region_scene, GTK_ALIGN_END);
        gtk_flow_box_insert(GTK_FLOW_BOX(region_body), region_scene, -1);
    }
    gtk_box_append(GTK_BOX(region_card), region_body);
    gtk_flow_box_insert(GTK_FLOW_BOX(status_grid), region_card, -1);

    appearance_card = make_status_card(
        "appearance-status-card",
        "preferences-desktop-theme-symbolic",
        "Display & Appearance",
        theme_name != NULL ? theme_name : "System theme",
        appearance_detail);
    GtkWidget *appearance_value_label = g_object_get_data(
        G_OBJECT(appearance_card), "status-value-label");
    GtkWidget *appearance_detail_label = g_object_get_data(
        G_OBJECT(appearance_card), "status-detail-label");
    appearance_previews = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(appearance_previews), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(
        GTK_FLOW_BOX(appearance_previews), 1U);
    gtk_flow_box_set_max_children_per_line(
        GTK_FLOW_BOX(appearance_previews), 4U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(appearance_previews), 6U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(appearance_previews), 6U);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(appearance_previews), TRUE);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(appearance_previews),
        make_theme_preview("Day", "theme-light", FALSE),
        -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(appearance_previews),
        make_theme_preview("Night", "theme-dark", FALSE),
        -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(appearance_previews),
        make_theme_preview("System", "theme-follow", FALSE),
        -1);
    gtk_box_append(GTK_BOX(appearance_card), appearance_previews);
    gtk_flow_box_insert(GTK_FLOW_BOX(status_grid), appearance_card, -1);

    network_card = make_status_card(
        "network-status-card",
        "network-wired-symbolic",
        "Network",
        online ? "Connected" : "Offline",
        network_detail);
    GtkWidget *network_value_label = g_object_get_data(
        G_OBJECT(network_card), "status-value-label");
    GtkWidget *network_detail_label = g_object_get_data(
        G_OBJECT(network_card), "status-detail-label");
    network_body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *network_visual = make_ui_asset_picture(
        "network-asset.png", 220, 100, "network-visual");
    if (network_visual == NULL) {
        network_visual = make_visual_panel(
            "network-asset.png", 220, 100, "network-visual");
    }
    gtk_widget_set_hexpand(network_visual, TRUE);
    gtk_widget_set_halign(network_visual, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(network_body), network_visual);
    gtk_box_append(GTK_BOX(network_card), network_body);
    gtk_flow_box_insert(GTK_FLOW_BOX(status_grid), network_card, -1);

    gtk_widget_set_valign(status_grid, GTK_ALIGN_START);
    gtk_widget_set_vexpand(status_grid, FALSE);
    gtk_box_append(GTK_BOX(page), status_grid);

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_overlay_scrolling(
        GTK_SCROLLED_WINDOW(scroller), FALSE);
    gtk_scrolled_window_set_kinetic_scrolling(
        GTK_SCROLLED_WINDOW(scroller), TRUE);
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_widget_set_valign(page, GTK_ALIGN_START);
    gtk_widget_set_vexpand(page, FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), page);

    g_object_set_data(
        G_OBJECT(scroller), "system-settings-home-hero", hero);
    g_object_set_data(
        G_OBJECT(scroller), "system-settings-home-features", features);
    g_object_set_data(
        G_OBJECT(scroller), "system-settings-home-grid", grid);
    g_object_set_data(
        G_OBJECT(scroller), "system-settings-home-overview", overview);
    g_object_set_data(
        G_OBJECT(scroller), "system-settings-home-quick", quick);
    g_object_set_data(
        G_OBJECT(scroller), "system-settings-home-status-grid", status_grid);
    g_object_set_data(
        G_OBJECT(scroller), "system-settings-home-date-card", date_card);
    g_object_set_data(
        G_OBJECT(scroller), "system-settings-home-region-card", region_card);
    g_object_set_data(
        G_OBJECT(scroller),
        "system-settings-home-appearance-card",
        appearance_card);
    g_object_set_data(
        G_OBJECT(scroller), "system-settings-home-network-card", network_card);
    g_object_set_data(
        G_OBJECT(scroller),
        "system-settings-home-appearance-previews",
        appearance_previews);

    HomeAdaptiveLayout *adaptive_layout = g_new0(HomeAdaptiveLayout, 1);
    adaptive_layout->hero = hero;
    adaptive_layout->features = GTK_FLOW_BOX(features);
    adaptive_layout->primary_grid = GTK_FLOW_BOX(grid);
    adaptive_layout->status_grid = GTK_FLOW_BOX(status_grid);
    adaptive_layout->appearance_previews = GTK_FLOW_BOX(appearance_previews);
    g_object_set_data_full(
        G_OBJECT(scroller),
        "system-settings-home-adaptive-layout",
        adaptive_layout,
        g_free);
    g_signal_connect(
        scroller,
        "notify::width",
        G_CALLBACK(home_layout_width_changed),
        adaptive_layout);
    g_signal_connect(
        scroller, "map", G_CALLBACK(home_layout_mapped), adaptive_layout);

    HomeTemporalTicker *ticker = g_new0(HomeTemporalTicker, 1);
    ticker->owner = scroller;
    ticker->clock = GTK_LABEL(date_clock_label);
    ticker->date = GTK_LABEL(date_calendar_label);
    ticker->system_time = GTK_LABEL(system_time_label);
    ticker->module_host = stored_context->module_host;
    g_signal_connect(
        scroller, "map", G_CALLBACK(home_temporal_mapped), ticker);
    g_signal_connect(
        scroller, "unmap", G_CALLBACK(home_temporal_unmapped), ticker);
    g_object_set_data_full(
        G_OBJECT(scroller),
        "system-settings-home-temporal-source",
        ticker,
        home_temporal_ticker_free);

    HomeStatusTicker *status_ticker = g_new0(HomeStatusTicker, 1);
    status_ticker->owner = scroller;
    status_ticker->uptime = GTK_LABEL(uptime_label);
    status_ticker->date_timezone = GTK_LABEL(date_timezone_label);
    status_ticker->region_value = GTK_LABEL(region_value_label);
    status_ticker->region_detail = GTK_LABEL(region_detail_label);
    status_ticker->appearance_value = GTK_LABEL(appearance_value_label);
    status_ticker->appearance_detail = GTK_LABEL(appearance_detail_label);
    status_ticker->network_value = GTK_LABEL(network_value_label);
    status_ticker->network_detail = GTK_LABEL(network_detail_label);
    g_signal_connect(
        scroller, "map", G_CALLBACK(home_status_mapped), status_ticker);
    g_signal_connect(
        scroller, "unmap", G_CALLBACK(home_status_unmapped), status_ticker);
    g_object_set_data_full(
        G_OBJECT(scroller),
        "system-settings-home-status-source",
        status_ticker,
        home_status_ticker_free);

    g_free(theme_name);
    ss_home_temporal_snapshot_clear(&temporal);
    return scroller;
}
