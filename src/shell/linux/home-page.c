// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file home-page.c
 * @brief Linux Home dashboard presentation and live status lifecycle.
 *
 * Home is deliberately composed from deterministic rows rather than nested
 * GtkFlowBox containers. Major dashboard regions have product-defined
 * relationships: System Overview and Quick Actions are peers, and the four
 * status surfaces form two peer rows. Those relationships must remain stable at
 * ordinary desktop widths instead of changing because a flow layout happens to
 * choose a different wrap point for a particular font, locale or monitor.
 */

#include "home-page.h"
#include "home-status.h"
#include "linux-ui-helpers.h"
#include "shell-command.h"

#include <stdbool.h>
#include <sys/utsname.h>

#ifndef SYSTEM_SETTINGS_UI_ASSET_DIR
#define SYSTEM_SETTINGS_UI_ASSET_DIR "/usr/share/infiltrator/system-settings/ui"
#endif
#ifndef SYSTEM_SETTINGS_UI_SOURCE_DIR
#define SYSTEM_SETTINGS_UI_SOURCE_DIR ""
#endif

typedef struct HomeTemporalTicker {
    GtkWidget *owner;
    GtkLabel *clock;
    GtkLabel *date;
    GtkLabel *system_time;
    SsHomeTemporalProvider *temporal_provider;
    guint source_id;
    guint interval_ms;
} HomeTemporalTicker;

typedef struct HomeAdaptiveLayout {
    GtkWidget *hero;
    GtkBox *primary_row;
    GtkBox *overview_body;
    GtkBox *status_row_top;
    GtkBox *status_row_bottom;
    GtkFlowBox *appearance_previews;
    guint applied_columns;
} HomeAdaptiveLayout;

typedef struct HomeProgramAction {
    gchar *program;
    gchar *argument;
} HomeProgramAction;

static void home_program_action_free(gpointer data, GClosure *closure)
{
    HomeProgramAction *action = data;

    (void)closure;
    if (action == NULL) {
        return;
    }
    g_free(action->program);
    g_free(action->argument);
    g_free(action);
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
        ticker->system_time == NULL || ticker->temporal_provider == NULL) {
        return G_SOURCE_REMOVE;
    }

    ss_home_temporal_snapshot_init(&temporal);
    if (ss_home_temporal_provider_format(
            ticker->temporal_provider, &temporal)) {
        label_set_text_if_changed(ticker->clock, temporal.clock_text);
        label_set_text_if_changed(ticker->date, temporal.date_text);
        label_set_text_if_changed(
            ticker->system_time, temporal.system_time_text);
    }
    ss_home_temporal_snapshot_clear(&temporal);

    desired_interval = ss_home_temporal_provider_refresh_interval_ms(
        ticker->temporal_provider);
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
        ticker->temporal_provider == NULL) {
        return;
    }

    ticker->interval_ms = ss_home_temporal_provider_refresh_interval_ms(
        ticker->temporal_provider);
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
    const char *page_id = context != NULL
        ? ss_home_temporal_provider_page_id(context->temporal_provider)
        : NULL;

    (void)button;
    if (context != NULL && context->navigate != NULL &&
        page_id != NULL && page_id[0] != '\0') {
        context->navigate(page_id, context->navigate_data);
    }
}

static void launch_external_program(GtkButton *button, gpointer user_data)
{
    HomeProgramAction *action = user_data;

    if (action == NULL || action->program == NULL) {
        return;
    }
    (void)launch_command(
        GTK_WIDGET(button), action->program, action->argument);
}

static bool connect_program_action(
    GtkWidget *button,
    const char *program,
    const char *argument,
    const char *missing_tooltip)
{
    g_autofree gchar *path = find_trusted_system_program(program);
    HomeProgramAction *action;

    if (button == NULL || path == NULL) {
        if (button != NULL) {
            gtk_widget_set_sensitive(button, FALSE);
            gtk_widget_set_tooltip_text(
                button,
                missing_tooltip != NULL
                    ? missing_tooltip
                    : "This application is not installed.");
        }
        return false;
    }

    action = g_new0(HomeProgramAction, 1);
    action->program = g_strdup(path);
    action->argument = g_strdup(argument);
    g_signal_connect_data(
        button,
        "clicked",
        G_CALLBACK(launch_external_program),
        action,
        home_program_action_free,
        0);
    return true;
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
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
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
    gtk_widget_set_valign(text, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), text);
    gtk_image_set_pixel_size(GTK_IMAGE(arrow), 17);
    gtk_widget_add_css_class(arrow, "quick-action-arrow");
    gtk_widget_set_valign(arrow, GTK_ALIGN_CENTER);
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

    (void)connect_program_action(
        button,
        program,
        argument,
        "This application is not installed.");
    return button;
}

static GtkWidget *make_status_card(
    const char *css_class,
    const char *icon_name,
    const char *title,
    const char *value,
    const char *detail,
    GtkWidget **value_label_out,
    GtkWidget **detail_label_out)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
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
    gtk_widget_set_hexpand(card, TRUE);

    if (value_label_out != NULL) {
        *value_label_out = value_label;
    }
    if (detail_label_out != NULL) {
        *detail_label_out = detail_label;
    }
    return card;
}

static GtkWidget *make_ui_asset_picture(
    const char *filename,
    int width,
    int height,
    const char *css_class)
{
    g_autofree gchar *path = NULL;
    GtkWidget *picture;

    if (filename == NULL || filename[0] == '\0') {
        return NULL;
    }

    path = g_build_filename(SYSTEM_SETTINGS_UI_ASSET_DIR, filename, NULL);
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR) &&
        SYSTEM_SETTINGS_UI_SOURCE_DIR[0] != '\0') {
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

    picture = ss_linux_ui_picture_new_for_filename(path);
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

static GtkWidget *make_visual_panel(
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
    const char *class_name)
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
    gtk_box_append(GTK_BOX(box), preview);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_box_append(
        GTK_BOX(box),
        ss_linux_ui_make_label(label, "theme-preview-label"));
    return box;
}

static GtkWidget *make_dashboard_row(const char *css_class)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);

    gtk_box_set_homogeneous(GTK_BOX(row), TRUE);
    if (css_class != NULL) {
        gtk_widget_add_css_class(row, css_class);
    }
    return row;
}

static guint home_layout_columns_for_width(int width)
{
    return width >= 760 ? 2U : 1U;
}

static void home_layout_apply_width(HomeAdaptiveLayout *layout, int width)
{
    const guint columns = home_layout_columns_for_width(width);
    const GtkOrientation row_orientation =
        columns == 2U ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    const GtkOrientation overview_orientation =
        columns == 2U ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;

    if (layout == NULL || layout->applied_columns == columns) {
        return;
    }

    layout->applied_columns = columns;

    /*
     * These are semantic rows, not a masonry surface. Switching orientation at
     * one explicit breakpoint keeps peer cards aligned at desktop widths while
     * retaining a readable single-column layout on genuinely narrow windows.
     */
    if (layout->hero != NULL) {
        gtk_widget_set_size_request(
            layout->hero, -1, columns == 2U ? 238 : 300);
    }
    if (layout->primary_row != NULL) {
        gtk_orientable_set_orientation(
            GTK_ORIENTABLE(layout->primary_row), row_orientation);
    }
    if (layout->overview_body != NULL) {
        gtk_orientable_set_orientation(
            GTK_ORIENTABLE(layout->overview_body), overview_orientation);
    }
    if (layout->status_row_top != NULL) {
        gtk_orientable_set_orientation(
            GTK_ORIENTABLE(layout->status_row_top), row_orientation);
    }
    if (layout->status_row_bottom != NULL) {
        gtk_orientable_set_orientation(
            GTK_ORIENTABLE(layout->status_row_bottom), row_orientation);
    }
    if (layout->appearance_previews != NULL) {
        gtk_flow_box_set_min_children_per_line(
            layout->appearance_previews, columns == 2U ? 3U : 1U);
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
        "hero-asset.png", -1, 238, "hero-scene");
    GtkWidget *hero_foreground = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *hero_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *hero_badge = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
    GtkWidget *hero_badge_icon = gtk_image_new_from_icon_name(
        "computer-symbolic");
    GtkWidget *primary_row = make_dashboard_row("home-grid");
    GtkWidget *overview = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *overview_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *overview_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *overview_icon = gtk_image_new_from_icon_name(
        "video-display-symbolic");
    GtkWidget *overview_data = gtk_grid_new();
    GtkWidget *overview_body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *overview_scene = make_visual_panel(
        "overview-asset.png", 150, 122, "overview-scene");
    GtkWidget *system_time_label;
    GtkWidget *quick = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *quick_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *quick_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *quick_icon = gtk_image_new_from_icon_name(
        "system-run-symbolic");
    GtkWidget *quick_grid = gtk_grid_new();
    GtkWidget *status_stack = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkWidget *status_row_top = make_dashboard_row(NULL);
    GtkWidget *status_row_bottom = make_dashboard_row(NULL);
    GtkWidget *date_card;
    GtkWidget *region_card;
    GtkWidget *appearance_card;
    GtkWidget *network_card;
    GtkWidget *appearance_value_label;
    GtkWidget *appearance_detail_label;
    GtkWidget *network_value_label;
    GtkWidget *network_detail_label;
    GtkWidget *date_open;
    GtkWidget *appearance_previews;
    GtkWidget *scroller = gtk_scrolled_window_new();
    SsHomePageContext *stored_context = g_new0(SsHomePageContext, 1);
    struct utsname uts;
    g_autofree gchar *os_name = g_get_os_info(G_OS_INFO_KEY_PRETTY_NAME);
    const char *desktop = g_getenv("XDG_CURRENT_DESKTOP");
    const char *host_name = g_get_host_name();
    const int uname_result = uname(&uts);
    const char *kernel = uname_result == 0 ? uts.release : "Unknown";
    const char *architecture = uname_result == 0 ? uts.machine : "Unknown";
    SsHomeTemporalSnapshot temporal;
    SsHomeStatusSnapshot status;
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
    if (stored_context->temporal_provider == NULL ||
        !ss_home_temporal_provider_format(
            stored_context->temporal_provider, &temporal)) {
        temporal.clock_text = g_strdup("Unknown");
        temporal.date_text = g_strdup("");
        temporal.system_time_text = g_strdup("Unknown");
    }

    ss_home_status_snapshot_init(&status);
    ss_home_status_snapshot_read(&status);

    gtk_widget_add_css_class(page, "home-page");

    /*
     * The hero is visual context, not an advertisement. One concise product
     * identity block sits over the artwork; the previous feature-chip and
     * duplicate-brand overlays competed with the actual settings below.
     */
    gtk_widget_add_css_class(hero, "home-hero");
    gtk_widget_set_margin_bottom(hero, 18);
    gtk_widget_set_size_request(hero, -1, 238);
    gtk_overlay_set_child(GTK_OVERLAY(hero), hero_scene);

    gtk_widget_add_css_class(hero_copy, "hero-copy-overlay");
    gtk_widget_set_valign(hero_copy, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(hero_copy, GTK_ALIGN_START);
    gtk_widget_set_margin_start(hero_copy, 20);
    gtk_widget_set_margin_end(hero_copy, 20);
    gtk_widget_set_margin_top(hero_copy, 20);
    gtk_widget_set_margin_bottom(hero_copy, 20);

    gtk_widget_add_css_class(hero_badge, "home-hero-badge");
    gtk_image_set_pixel_size(GTK_IMAGE(hero_badge_icon), 16);
    gtk_box_append(GTK_BOX(hero_badge), hero_badge_icon);
    gtk_box_append(
        GTK_BOX(hero_badge),
        ss_linux_ui_make_label("INFILTRATOR OS", "home-hero-eyebrow"));
    gtk_widget_set_halign(hero_badge, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(hero_copy), hero_badge);
    gtk_box_append(
        GTK_BOX(hero_copy),
        ss_linux_ui_make_label("System Settings", "home-hero-title"));
    gtk_box_append(
        GTK_BOX(hero_copy),
        ss_linux_ui_make_label(
            "Your system at a glance. Open a setting and make it yours.",
            "home-hero-subtitle"));
    gtk_box_append(GTK_BOX(hero_foreground), hero_copy);
    gtk_overlay_add_overlay(GTK_OVERLAY(hero), hero_foreground);
    gtk_overlay_set_measure_overlay(GTK_OVERLAY(hero), hero_foreground, TRUE);
    gtk_box_append(GTK_BOX(page), hero);

    /*
     * Primary row: System Overview and Quick Actions are explicit peers. A
     * homogeneous GtkBox gives them equal authority and guarantees that they
     * remain side-by-side at normal desktop widths.
     */
    gtk_widget_add_css_class(overview, "home-card");
    gtk_widget_set_hexpand(overview, TRUE);
    gtk_widget_add_css_class(overview_icon_wrap, "home-card-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(overview_icon), 22);
    gtk_box_append(GTK_BOX(overview_icon_wrap), overview_icon);
    gtk_box_append(GTK_BOX(overview_heading), overview_icon_wrap);
    gtk_box_append(
        GTK_BOX(overview_heading),
        ss_linux_ui_make_label("System Overview", "home-card-title"));
    {
        GtkWidget *overview_spacer =
            gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *updates_button =
            gtk_button_new_with_label("Check for updates");

        gtk_widget_set_hexpand(overview_spacer, TRUE);
        gtk_box_append(GTK_BOX(overview_heading), overview_spacer);
        gtk_widget_add_css_class(updates_button, "overview-link-button");
        (void)connect_program_action(
            updates_button,
            "infiltrator-software",
            NULL,
            "Software is not installed.");
        gtk_box_append(GTK_BOX(overview_heading), updates_button);
    }
    gtk_box_append(GTK_BOX(overview), overview_heading);

    gtk_grid_set_column_spacing(GTK_GRID(overview_data), 16);
    gtk_grid_set_row_spacing(GTK_GRID(overview_data), 7);
    append_overview_row(
        GTK_GRID(overview_data), 0, "Operating system", os_display);
    append_overview_row(GTK_GRID(overview_data), 1, "Kernel", kernel);
    append_overview_row(
        GTK_GRID(overview_data), 2, "Architecture", architecture);
    append_overview_row(GTK_GRID(overview_data), 3, "Desktop", desktop);
    append_overview_row(GTK_GRID(overview_data), 4, "Hostname", host_name);
    GtkWidget *uptime_label = append_overview_row(
        GTK_GRID(overview_data), 5, "Uptime", status.uptime);
    system_time_label = append_overview_row(
        GTK_GRID(overview_data), 6, "System time", temporal.system_time_text);
    gtk_widget_set_hexpand(overview_data, TRUE);
    gtk_widget_set_valign(overview_data, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(overview_scene, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(overview_body), overview_scene);
    gtk_box_append(GTK_BOX(overview_body), overview_data);
    gtk_box_append(GTK_BOX(overview), overview_body);

    gtk_widget_add_css_class(quick, "home-card");
    gtk_widget_set_hexpand(quick, TRUE);
    gtk_widget_add_css_class(quick_icon_wrap, "home-card-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(quick_icon), 22);
    gtk_box_append(GTK_BOX(quick_icon_wrap), quick_icon);
    gtk_box_append(GTK_BOX(quick_heading), quick_icon_wrap);
    gtk_box_append(
        GTK_BOX(quick_heading),
        ss_linux_ui_make_label("Quick Actions", "home-card-title"));
    gtk_box_append(GTK_BOX(quick), quick_heading);

    gtk_widget_add_css_class(quick_grid, "quick-action-grid");
    gtk_grid_set_column_spacing(GTK_GRID(quick_grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(quick_grid), 8);
    gtk_grid_set_column_homogeneous(GTK_GRID(quick_grid), TRUE);
    gtk_grid_set_row_homogeneous(GTK_GRID(quick_grid), TRUE);

    GtkWidget *date_action = make_quick_action(
        "preferences-system-time-symbolic",
        "Set Date & Time",
        "Time zone, clock & calendar");
    g_signal_connect(
        date_action,
        "clicked",
        G_CALLBACK(open_date_time_from_home),
        stored_context);
    gtk_grid_attach(GTK_GRID(quick_grid), date_action, 0, 0, 1, 1);
    gtk_grid_attach(
        GTK_GRID(quick_grid),
        make_program_action(
            "preferences-desktop-locale-symbolic",
            "Change Region",
            "Language, formats & location",
            "mintlocale",
            NULL),
        1, 0, 1, 1);
    gtk_grid_attach(
        GTK_GRID(quick_grid),
        make_program_action(
            "video-display-symbolic",
            "Configure Display",
            "Scaling, layout & monitors",
            "cinnamon-settings",
            "display"),
        0, 1, 1, 1);
    gtk_grid_attach(
        GTK_GRID(quick_grid),
        make_program_action(
            "system-software-install-symbolic",
            "Software & Updates",
            "Apps, packages & updates",
            "infiltrator-software",
            NULL),
        1, 1, 1, 1);
    gtk_box_append(GTK_BOX(quick), quick_grid);

    gtk_box_append(GTK_BOX(primary_row), overview);
    gtk_box_append(GTK_BOX(primary_row), quick);
    gtk_box_append(GTK_BOX(page), primary_row);

    GtkWidget *section_header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_top(section_header, 24);
    gtk_widget_set_margin_bottom(section_header, 10);
    gtk_widget_set_margin_start(section_header, 2);
    gtk_box_append(
        GTK_BOX(section_header),
        ss_linux_ui_make_label("AT A GLANCE", "page-eyebrow"));
    gtk_box_append(
        GTK_BOX(section_header),
        ss_linux_ui_make_label(
            "Live system state and the settings you use most.",
            "page-summary"));
    gtk_box_append(GTK_BOX(page), section_header);

    /*
     * Status surfaces use two explicit peer rows. This is intentionally not a
     * flow layout: card pairings are part of the information architecture.
     */
    date_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 9);
    gtk_widget_add_css_class(date_card, "status-card");
    gtk_widget_add_css_class(date_card, "date-status-card");
    gtk_widget_set_hexpand(date_card, TRUE);
    {
        GtkWidget *date_heading =
            gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
        GtkWidget *date_icon_wrap =
            gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *date_icon = gtk_image_new_from_icon_name(
            "preferences-system-time-symbolic");

        gtk_widget_add_css_class(date_heading, "status-card-heading");
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
    }

    GtkWidget *date_body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *date_clock = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *date_clock_label = ss_linux_ui_make_label(
        temporal.clock_text, "home-clock-value");
    GtkWidget *date_calendar_label = ss_linux_ui_make_label(
        temporal.date_text, "home-clock-date");
    gtk_box_append(GTK_BOX(date_clock), date_clock_label);
    gtk_label_set_wrap(GTK_LABEL(date_calendar_label), TRUE);
    gtk_box_append(GTK_BOX(date_clock), date_calendar_label);
    gtk_widget_set_hexpand(date_clock, TRUE);
    gtk_widget_set_valign(date_clock, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(date_body), date_clock);

    GtkWidget *date_meta = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *pin_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *pin = gtk_image_new_from_icon_name("mark-location-symbolic");
    GtkWidget *date_location = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *date_timezone_label = ss_linux_ui_make_label(
        status.timezone, "location-primary");
    gtk_widget_add_css_class(pin_wrap, "location-pin-well");
    gtk_image_set_pixel_size(GTK_IMAGE(pin), 24);
    gtk_box_append(GTK_BOX(pin_wrap), pin);
    gtk_box_append(GTK_BOX(date_meta), pin_wrap);
    gtk_box_append(GTK_BOX(date_location), date_timezone_label);
    gtk_box_append(
        GTK_BOX(date_location),
        ss_linux_ui_make_label(
            "Authoritative local time zone", "status-card-detail"));
    gtk_box_append(GTK_BOX(date_meta), date_location);
    gtk_widget_set_hexpand(date_meta, TRUE);
    gtk_widget_set_valign(date_meta, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(date_body), date_meta);

    GtkWidget *date_scene = make_visual_panel(
        "date-asset.png", 120, 78, "date-scene");
    gtk_widget_set_valign(date_scene, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(date_scene, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(date_body), date_scene);
    gtk_box_append(GTK_BOX(date_card), date_body);

    region_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(region_card, "status-card");
    gtk_widget_add_css_class(region_card, "region-status-card");
    gtk_widget_set_hexpand(region_card, TRUE);
    {
        GtkWidget *region_heading =
            gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
        GtkWidget *region_icon = gtk_image_new_from_icon_name(
            "preferences-desktop-locale-symbolic");

        gtk_widget_add_css_class(region_heading, "status-card-heading");
        gtk_image_set_pixel_size(GTK_IMAGE(region_icon), 23);
        gtk_box_append(GTK_BOX(region_heading), region_icon);
        gtk_box_append(
            GTK_BOX(region_heading),
            ss_linux_ui_make_label("Region & Language", "home-card-title"));
        gtk_box_append(GTK_BOX(region_card), region_heading);
    }

    GtkWidget *region_body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    GtkWidget *region_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *region_value_label = ss_linux_ui_make_label(
        status.region_value, "region-country");
    GtkWidget *region_detail_label = ss_linux_ui_make_label(
        status.region_detail, "status-card-detail");
    gtk_label_set_ellipsize(
        GTK_LABEL(region_value_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_wrap(GTK_LABEL(region_detail_label), TRUE);
    gtk_box_append(GTK_BOX(region_copy), region_value_label);
    gtk_box_append(GTK_BOX(region_copy), region_detail_label);
    gtk_widget_set_hexpand(region_copy, TRUE);
    gtk_widget_set_valign(region_copy, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(region_body), region_copy);
    GtkWidget *region_scene = make_ui_asset_picture(
        "region-asset.png", 190, 96, "region-scene");
    if (region_scene != NULL) {
        gtk_widget_set_halign(region_scene, GTK_ALIGN_END);
        gtk_widget_set_valign(region_scene, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(region_body), region_scene);
    }
    gtk_box_append(GTK_BOX(region_card), region_body);

    appearance_card = make_status_card(
        "appearance-status-card",
        "preferences-desktop-theme-symbolic",
        "Display & Appearance",
        status.appearance_value,
        status.appearance_detail,
        &appearance_value_label,
        &appearance_detail_label);
    appearance_previews = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(appearance_previews), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(
        GTK_FLOW_BOX(appearance_previews), 1U);
    gtk_flow_box_set_max_children_per_line(
        GTK_FLOW_BOX(appearance_previews), 3U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(appearance_previews), 6U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(appearance_previews), 6U);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(appearance_previews), TRUE);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(appearance_previews),
        make_theme_preview("Day", "theme-light"),
        -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(appearance_previews),
        make_theme_preview("Night", "theme-dark"),
        -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(appearance_previews),
        make_theme_preview("System", "theme-follow"),
        -1);
    gtk_box_append(GTK_BOX(appearance_card), appearance_previews);

    network_card = make_status_card(
        "network-status-card",
        "network-wired-symbolic",
        "Network",
        status.network_value,
        status.network_detail,
        &network_value_label,
        &network_detail_label);
    GtkWidget *network_visual = make_ui_asset_picture(
        "network-asset.png", 220, 100, "network-visual");
    if (network_visual == NULL) {
        network_visual = make_visual_panel(
            "network-asset.png", 220, 100, "network-visual");
    }
    gtk_widget_set_halign(network_visual, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(network_card), network_visual);

    gtk_box_append(GTK_BOX(status_row_top), date_card);
    gtk_box_append(GTK_BOX(status_row_top), region_card);
    gtk_box_append(GTK_BOX(status_row_bottom), appearance_card);
    gtk_box_append(GTK_BOX(status_row_bottom), network_card);
    gtk_box_append(GTK_BOX(status_stack), status_row_top);
    gtk_box_append(GTK_BOX(status_stack), status_row_bottom);
    gtk_box_append(GTK_BOX(page), status_stack);

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

    HomeAdaptiveLayout *adaptive_layout = g_new0(HomeAdaptiveLayout, 1);
    adaptive_layout->hero = hero;
    adaptive_layout->primary_row = GTK_BOX(primary_row);
    adaptive_layout->overview_body = GTK_BOX(overview_body);
    adaptive_layout->status_row_top = GTK_BOX(status_row_top);
    adaptive_layout->status_row_bottom = GTK_BOX(status_row_bottom);
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
    ticker->temporal_provider = stored_context->temporal_provider;
    g_signal_connect(
        scroller, "map", G_CALLBACK(home_temporal_mapped), ticker);
    g_signal_connect(
        scroller, "unmap", G_CALLBACK(home_temporal_unmapped), ticker);
    g_object_set_data_full(
        G_OBJECT(scroller),
        "system-settings-home-temporal-source",
        ticker,
        home_temporal_ticker_free);

    ss_home_status_attach(
        scroller,
        GTK_LABEL(uptime_label),
        GTK_LABEL(date_timezone_label),
        GTK_LABEL(region_value_label),
        GTK_LABEL(region_detail_label),
        GTK_LABEL(appearance_value_label),
        GTK_LABEL(appearance_detail_label),
        GTK_LABEL(network_value_label),
        GTK_LABEL(network_detail_label));

    ss_home_status_snapshot_clear(&status);
    ss_home_temporal_snapshot_clear(&temporal);
    return scroller;
}
