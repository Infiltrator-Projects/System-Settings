from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(
            f"{path}: expected one match, found {count}: {old[:80]!r}")
    p.write_text(text.replace(old, new, 1))


# Remove the now-dead per-row colour-class contract. Common owns the single
# neutral accent; keeping gold/cyan classes after making them identical would
# preserve exactly the kind of iteration debris this pass is intended to remove.
main = Path("src/shell/linux/main.c")
text = main.read_text()
old = """static GtkWidget *make_external_navigation_row(const char *icon_name,
                                               const char *title,
                                               const char *subtitle,
                                               const char *search_text,
                                               const char *program,
                                               const char *argument,
                                               const char *accent_class)
{
    GtkWidget *row = make_navigation_row(
        icon_name,
        title,
        subtitle,
        NULL,
        search_text);
    g_autofree gchar *path =
        find_trusted_system_program(program);

    if (accent_class != NULL) {
        gtk_widget_add_css_class(row, accent_class);
    }
"""
new = """static GtkWidget *make_external_navigation_row(const char *icon_name,
                                               const char *title,
                                               const char *subtitle,
                                               const char *search_text,
                                               const char *program,
                                               const char *argument)
{
    GtkWidget *row = make_navigation_row(
        icon_name,
        title,
        subtitle,
        NULL,
        search_text);
    g_autofree gchar *path =
        find_trusted_system_program(program);

"""
if text.count(old) != 1:
    raise SystemExit("main.c: external navigation signature changed")
text = text.replace(old, new, 1)

# Internal Home/Date rows no longer need a colour class either.
for statement in (
    '    gtk_widget_add_css_class(home_row, "nav-gold");\n\n',
    '    gtk_widget_add_css_class(date_row, "nav-gold");\n\n',
    '    gtk_widget_add_css_class(row, "nav-cyan");\n',
):
    if text.count(statement) != 1:
        raise SystemExit(f"main.c: expected one stale navigation class: {statement!r}")
    text = text.replace(statement, "", 1)

# All external calls end with a former accent class; delete that final argument
# rather than retaining a parameter that no longer has any visual meaning.
for klass in ("nav-gold", "nav-cyan"):
    token = f',\n        "{klass}");'
    count = text.count(token)
    if count == 0:
        raise SystemExit(f"main.c: no {klass} call arguments found")
    text = text.replace(token, ");")

if "nav-gold" in text or "nav-cyan" in text or "accent_class" in text:
    raise SystemExit("main.c: stale navigation accent contract remains")
main.write_text(text)

# Theme the navigation icon well directly from Common's neutral accent.
theme = Path("src/shell/linux/linux-theme.c")
text = theme.read_text()
old = (
    '        ".nav-gold .nav-icon-well image { color: %s; }\\n"\n'
    '        ".nav-cyan .nav-icon-well image { color: %s; }\\n"\n'
)
new = '        ".nav-icon-well image { color: %s; }\\n"\n'
if text.count(old) != 1:
    raise SystemExit("linux-theme.c: navigation selectors changed")
text = text.replace(old, new, 1)
old_args = "        surface, border,\n        accent, accent,\n        text, (unsigned int)type->ui_bold_weight,"
new_args = "        surface, border, accent,\n        text, (unsigned int)type->ui_bold_weight,"
if text.count(old_args) != 1:
    raise SystemExit("linux-theme.c: navigation selector arguments changed")
text = text.replace(old_args, new_args, 1)
theme.write_text(text)

# A preview tick requires both labels before it can use the date label as the
# cache owner. This also makes the guard truthful rather than relying on build
# order to guarantee date_preview.
panel = Path("src/modules/date-time/linux-date-time-panel.c")
text = panel.read_text()
old = """    if (state == NULL || state->clock_preview == NULL || now == NULL) {
        return G_SOURCE_CONTINUE;
    }
"""
new = """    if (state == NULL || state->clock_preview == NULL ||
        state->date_preview == NULL || now == NULL) {
        return G_SOURCE_CONTINUE;
    }
"""
if text.count(old) != 1:
    raise SystemExit("linux-date-time-panel.c: preview guard changed")
text = text.replace(old, new, 1)
panel.write_text(text)

# Cache only a successfully formatted non-Gregorian date. Calendar can become
# available after startup; caching its temporary fallback until midnight would
# turn a performance optimization into stale UI.
home = Path("src/modules/date-time/home-temporal-presentation.c")
text = home.read_text()
start = text.find("static gchar *presenter_format_date(\n")
end = text.find("\nstatic bool presenter_format_into(\n", start)
if start < 0 or end < 0:
    raise SystemExit("home-temporal-presentation.c: presenter date anchors changed")
replacement = r'''static gchar *presenter_format_date(
    SsHomeTemporalPresenter *presenter,
    GDateTime *now)
{
    const gint year = g_date_time_get_year(now);
    const gint month = g_date_time_get_month(now);
    const gint day = g_date_time_get_day_of_month(now);
    const bool gregorian =
        g_strcmp0(presenter->policy.calendar, "gregorian") == 0;
    g_autofree gchar *formatted = NULL;

    if (presenter->cached_date_text != NULL &&
        presenter->cached_calendar_id != NULL &&
        presenter->cached_year == year &&
        presenter->cached_month == month &&
        presenter->cached_day == day &&
        g_strcmp0(presenter->cached_calendar_id,
                  presenter->policy.calendar) == 0) {
        return g_strdup(presenter->cached_date_text);
    }

    if (gregorian) {
        formatted = g_date_time_format(now, "%A, %e %B %Y");
    } else {
        const InfiltratrTemporalCalendarInfo *calendar =
            infiltratr_temporal_calendar_find(presenter->policy.calendar);
        formatted = ss_calendar_preview_provider_format_date(
            presenter->calendar_provider,
            presenter->policy.calendar,
            year, month, day);
        if (formatted == NULL) {
            return calendar != NULL
                ? g_strdup_printf(
                      "%s — preview unavailable", calendar->name)
                : NULL;
        }
    }
    if (formatted == NULL) {
        return NULL;
    }

    g_free(presenter->cached_date_text);
    presenter->cached_date_text = g_strdup(formatted);
    g_free(presenter->cached_calendar_id);
    presenter->cached_calendar_id = g_strdup(presenter->policy.calendar);
    presenter->cached_year = year;
    presenter->cached_month = month;
    presenter->cached_day = day;
    return g_steal_pointer(&formatted);
}
'''
text = text[:start] + replacement + text[end:]
home.write_text(text)

# Amend the release note so the visible standardization description matches the
# code: the old colour-class split is gone, not merely mapped to one colour.
changelog = Path("CHANGELOG.md")
text = changelog.read_text()
old = "using the Common neutral accent for both legacy navigation accent classes instead of preserving an old gold/cyan split."
new = "removing the obsolete gold/cyan navigation class split and styling navigation icons directly with Common's neutral accent."
if text.count(old) != 1:
    raise SystemExit("CHANGELOG.md: 0.4.50 UI note changed")
text = text.replace(old, new, 1)
changelog.write_text(text)

Path(".github/workflows/hal-0.4.50-finalize.yml").unlink()
Path(".github/hal-0.4.50-finalize.py").unlink()
