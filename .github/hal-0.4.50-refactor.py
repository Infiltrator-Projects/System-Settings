from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(
            f"{path}: expected one match, found {count}: {old[:80]!r}")
    p.write_text(text.replace(old, new, 1))


main = Path("src/shell/linux/main.c")
text = main.read_text()

old = """} HomeStatusTicker;

static gboolean refresh_home_temporal(gpointer user_data)
"""
new = """} HomeStatusTicker;

static void label_set_text_if_changed(GtkLabel *label, const char *text)
{
    const char *next = text != NULL ? text : "";

    if (label != NULL &&
        g_strcmp0(gtk_label_get_text(label), next) != 0) {
        gtk_label_set_text(label, next);
    }
}

static gboolean refresh_home_temporal(gpointer user_data)
"""
if text.count(old) != 1:
    raise SystemExit("main.c: HomeStatusTicker anchor changed")
text = text.replace(old, new, 1)

old = """        gtk_label_set_text(ticker->clock, temporal.clock_text);
        gtk_label_set_text(ticker->date, temporal.date_text);
        gtk_label_set_text(ticker->system_time, temporal.system_time_text);"""
new = """        label_set_text_if_changed(ticker->clock, temporal.clock_text);
        label_set_text_if_changed(ticker->date, temporal.date_text);
        label_set_text_if_changed(
            ticker->system_time, temporal.system_time_text);"""
if text.count(old) != 1:
    raise SystemExit("main.c: temporal label update block changed")
text = text.replace(old, new, 1)

replacements = [
    (
        """    if (ticker->uptime != NULL) gtk_label_set_text(ticker->uptime, uptime);""",
        """    label_set_text_if_changed(ticker->uptime, uptime);""",
    ),
    (
        """    if (ticker->date_timezone != NULL && local_zone != NULL) {
        gtk_label_set_text(
            ticker->date_timezone,
            g_time_zone_get_identifier(local_zone));
    }""",
        """    if (local_zone != NULL) {
        label_set_text_if_changed(
            ticker->date_timezone,
            g_time_zone_get_identifier(local_zone));
    }""",
    ),
    (
        """    if (ticker->region_value != NULL) {
        gtk_label_set_text(ticker->region_value, format_locale);
    }""",
        """    label_set_text_if_changed(ticker->region_value, format_locale);""",
    ),
    (
        """    if (ticker->region_detail != NULL) {
        gtk_label_set_text(ticker->region_detail, region_detail);
    }""",
        """    label_set_text_if_changed(ticker->region_detail, region_detail);""",
    ),
    (
        """    if (ticker->appearance_value != NULL) {
        gtk_label_set_text(
            ticker->appearance_value,
            theme_name != NULL ? theme_name : "System theme");
    }""",
        """    label_set_text_if_changed(
        ticker->appearance_value,
        theme_name != NULL ? theme_name : "System theme");""",
    ),
    (
        """    if (ticker->appearance_detail != NULL) {
        gtk_label_set_text(ticker->appearance_detail, appearance_detail);
    }""",
        """    label_set_text_if_changed(
        ticker->appearance_detail, appearance_detail);""",
    ),
    (
        """    if (ticker->network_value != NULL) {
        gtk_label_set_text(
            ticker->network_value, online ? "Connected" : "Offline");
    }""",
        """    label_set_text_if_changed(
        ticker->network_value, online ? "Connected" : "Offline");""",
    ),
    (
        """    if (ticker->network_detail != NULL) {
        gtk_label_set_text(ticker->network_detail, network_detail);
    }""",
        """    label_set_text_if_changed(ticker->network_detail, network_detail);""",
    ),
]
for old, new in replacements:
    if text.count(old) != 1:
        raise SystemExit(
            f"main.c: status refresh anchor changed: {old[:60]!r}")
    text = text.replace(old, new, 1)

old = """typedef struct {
    GtkWidget *hero;
    GtkFlowBox *features;
    GtkFlowBox *primary_grid;
    GtkFlowBox *status_grid;
    GtkFlowBox *appearance_previews;
} HomeAdaptiveLayout;"""
new = """typedef struct {
    GtkWidget *hero;
    GtkFlowBox *features;
    GtkFlowBox *primary_grid;
    GtkFlowBox *status_grid;
    GtkFlowBox *appearance_previews;
    guint applied_columns;
} HomeAdaptiveLayout;"""
if text.count(old) != 1:
    raise SystemExit("main.c: adaptive layout struct changed")
text = text.replace(old, new, 1)

old = """static void home_layout_apply_width(HomeAdaptiveLayout *layout, int width)
{
    const guint columns = home_layout_columns_for_width(width);

    if (layout == NULL) {
        return;
    }"""
new = """static void home_layout_apply_width(HomeAdaptiveLayout *layout, int width)
{
    const guint columns = home_layout_columns_for_width(width);

    if (layout == NULL || layout->applied_columns == columns) {
        return;
    }
    /*
     * notify::width can fire for every allocation step while a window is
     * resized. Reapplying identical FlowBox constraints invalidates layout
     * again, creating the resize feedback that made Home feel sticky. Only
     * cross the adaptive contract when the actual column mode changes.
     */
    layout->applied_columns = columns;"""
if text.count(old) != 1:
    raise SystemExit("main.c: adaptive apply anchor changed")
text = text.replace(old, new, 1)
main.write_text(text)

panel = Path("src/modules/date-time/linux-date-time-panel.c")
text = panel.read_text()
start_marker = "static gboolean refresh_preview(gpointer user_data)\n{"
end_marker = "\nstatic gboolean refresh_preview_once(gpointer user_data)"
start = text.find(start_marker)
end = text.find(end_marker, start)
if start < 0 or end < 0:
    raise SystemExit("linux-date-time-panel.c: preview function anchors changed")
replacement = r'''static gboolean refresh_preview(gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    const InfiltratrTemporalPolicyV3 *policy;
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    char clock_text[160];

    if (state == NULL || state->clock_preview == NULL || now == NULL) {
        return G_SOURCE_CONTINUE;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL) {
        return G_SOURCE_CONTINUE;
    }

    if (format_preview(state, clock_text, sizeof(clock_text)) &&
        g_strcmp0(
            gtk_label_get_text(GTK_LABEL(state->clock_preview)),
            clock_text) != 0) {
        gtk_label_set_text(GTK_LABEL(state->clock_preview), clock_text);
    }

    /*
     * Extended clocks can legitimately require a 250 ms timer, but the
     * calendar and overview policy do not. Older iterations rebuilt the same
     * date, called into Calendar and rewrote static labels on every clock tick.
     * Cache successful date formatting by civil day/calendar and leave overview
     * policy refreshes on their existing policy/settings change paths.
     */
    const gint year = g_date_time_get_year(now);
    const gint month = g_date_time_get_month(now);
    const gint day = g_date_time_get_day_of_month(now);
    g_autofree gchar *date_key = g_strdup_printf(
        "%04d-%02d-%02d|%s",
        year, month, day, policy->calendar);
    const char *cached_key = g_object_get_data(
        G_OBJECT(state->date_preview),
        "system-settings-preview-date-key");

    if (g_strcmp0(cached_key, date_key) != 0) {
        const InfiltratrTemporalCalendarInfo *calendar =
            infiltratr_temporal_calendar_find(policy->calendar);
        g_autofree gchar *gregorian =
            g_date_time_format(now, "%A, %e %B %Y");
        g_autofree gchar *selected_date = NULL;
        g_autofree gchar *summary = NULL;
        bool cacheable = false;

        if (calendar != NULL) {
            if (strcmp(policy->calendar, "gregorian") == 0) {
                summary = g_strdup_printf(
                    "%s • %s",
                    calendar->name,
                    gregorian != NULL ? gregorian : "");
                cacheable = true;
            } else {
                selected_date =
                    ss_calendar_preview_provider_format_date(
                        ensure_calendar_preview_provider(state),
                        policy->calendar,
                        year,
                        month,
                        day);
                if (selected_date != NULL) {
                    summary = g_strdup_printf(
                        "%s • %s",
                        calendar->name,
                        selected_date);
                    cacheable = true;
                } else {
                    summary = g_strdup_printf(
                        "%s • preview unavailable",
                        calendar->name);
                }
            }
        }

        if (summary != NULL &&
            g_strcmp0(
                gtk_label_get_text(GTK_LABEL(state->date_preview)),
                summary) != 0) {
            gtk_label_set_text(GTK_LABEL(state->date_preview), summary);
        }
        if (cacheable) {
            g_object_set_data_full(
                G_OBJECT(state->date_preview),
                "system-settings-preview-date-key",
                g_strdup(date_key),
                g_free);
        }
    }

    return G_SOURCE_CONTINUE;
}
'''
text = text[:start] + replacement + text[end:]
panel.write_text(text)

theme = Path("src/shell/linux/linux-theme.c")
text = theme.read_text()
shadow = "box-shadow: 0 6px 18px rgba(0,0,0,0.22);"
if text.count(shadow) != 1:
    raise SystemExit("linux-theme.c: hero shadow anchor changed")
text = text.replace(shadow, "box-shadow: none;", 1)
nav_args = (
    "        surface, border,\n"
    "        warm, accent,\n"
    "        text, (unsigned int)type->ui_bold_weight,"
)
if text.count(nav_args) != 1:
    raise SystemExit("linux-theme.c: navigation palette anchor changed")
text = text.replace(
    nav_args,
    "        surface, border,\n"
    "        accent, accent,\n"
    "        text, (unsigned int)type->ui_bold_weight,",
    1,
)
theme.write_text(text)

Path("VERSION").write_text("0.4.50\n")

changelog = Path("CHANGELOG.md")
text = changelog.read_text()
if not text.startswith("## 0.4.49"):
    raise SystemExit("CHANGELOG.md: expected 0.4.49 at top")
entry = """## 0.4.50 — 2026-10-03

- Forensically remove high-frequency refresh-all behaviour left by the iterative Home and Date & Time UI work: unchanged GTK labels are no longer rewritten, successful calendar formatting is cached by calendar/day, and static overview policy labels are no longer asserted on every 250 ms clock tick.
- Stop Home resize feedback from repeatedly reapplying identical FlowBox constraints for every `notify::width`; adaptive geometry now changes only when the one/two-column breakpoint is actually crossed.
- Keep fast extended-clock cadence intact so decimal/French and other non-SI clock displays remain accurate while moving the expensive static work off that cadence.
- Bring the shell closer to the restrained InfiltratorOS surface language by removing the leftover Date & Time hero drop shadow and using the Common neutral accent for both legacy navigation accent classes instead of preserving an old gold/cyan split.
- Advance the pinned Common 1.19.38 source to current Common head `7070c5812b50821fd7580101cb2289a3184f6b2c`, including the latest portable-build and hosted-CI corrections.

"""
changelog.write_text(entry + text)

Path(".github/workflows/hal-0.4.50-refactor.yml").unlink()
Path(".github/hal-0.4.50-refactor.py").unlink()
