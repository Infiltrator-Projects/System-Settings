from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}: {old[:90]!r}")
    p.write_text(text.replace(old, new, 1))


theme = Path("src/shell/linux/linux-theme.c")
text = theme.read_text()
old = """static GtkCssProvider *common_theme_provider;
static bool common_theme_watch_installed;
static unsigned int common_theme_generation;
"""
new = """static GtkCssProvider *common_theme_provider;
static bool common_theme_watch_installed;
static bool common_theme_state_valid;
static bool common_theme_dark;
static unsigned int common_theme_generation;
"""
if text.count(old) != 1:
    raise SystemExit("linux-theme.c: theme state declarations changed")
text = text.replace(old, new, 1)

old = """void ss_linux_theme_install(void)
{
    const InfiltratrThemePalette *palette =
        infiltratr_theme_resolve(INFILTRATR_THEME_SYSTEM,
                                 system_prefers_dark());
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
"""
new = """void ss_linux_theme_install(void)
{
    const bool dark = system_prefers_dark();
    const InfiltratrThemePalette *palette;
    const InfiltratrDesignMetrics *metrics;

    /*
     * GtkCssProvider is display-global. Replacing an equivalent provider every
     * time a System Settings window is reopened invalidates the complete GTK
     * style tree while the previous window may still be finishing teardown.
     * That old iterative behaviour was both wasted work and, under native
     * LTO/PGO timing, could race widget finalisation. Only reinstall when the
     * Common palette selection can actually change.
     */
    if (common_theme_provider != NULL &&
        common_theme_state_valid &&
        common_theme_dark == dark) {
        return;
    }

    palette = infiltratr_theme_resolve(INFILTRATR_THEME_SYSTEM, dark);
    metrics = infiltratr_design_metrics();
"""
if text.count(old) != 1:
    raise SystemExit("linux-theme.c: install prologue changed")
text = text.replace(old, new, 1)

old = """    gtk_style_context_add_provider_for_display(
        display,
        GTK_STYLE_PROVIDER(common_theme_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    ++common_theme_generation;
"""
new = """    gtk_style_context_add_provider_for_display(
        display,
        GTK_STYLE_PROVIDER(common_theme_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    common_theme_dark = dark;
    common_theme_state_valid = true;
    ++common_theme_generation;
"""
if text.count(old) != 1:
    raise SystemExit("linux-theme.c: provider publish block changed")
text = text.replace(old, new, 1)
theme.write_text(text)

test = Path("tests/linux-shell-test.c")
text = test.read_text()
old = """    g_autoptr(GtkApplication) app = gtk_application_new(
        \"org.infiltrator.SystemSettings.Test\", G_APPLICATION_NON_UNIQUE);
    g_assert_true(g_application_register(G_APPLICATION(app), NULL, NULL));
    for (int i = 0; i < 8; ++i) {
        on_activate(app, NULL);
        while (g_main_context_iteration(NULL, FALSE)) {
        }
"""
new = """    g_autoptr(GtkApplication) app = gtk_application_new(
        \"org.infiltrator.SystemSettings.Test\", G_APPLICATION_NON_UNIQUE);
    unsigned int settled_theme_generation = 0U;
    g_assert_true(g_application_register(G_APPLICATION(app), NULL, NULL));
    for (int i = 0; i < 8; ++i) {
        on_activate(app, NULL);
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        if (i > 0) {
            g_assert_cmpuint(
                ss_linux_theme_generation(), ==, settled_theme_generation);
        }
"""
if text.count(old) != 1:
    raise SystemExit("linux-shell-test.c: activation loop anchor changed")
text = text.replace(old, new, 1)

old = """            g_object_set(
                gtk_settings,
                \"gtk-application-prefer-dark-theme\",
                prefer_dark,
                NULL);
            while (g_main_context_iteration(NULL, FALSE)) {
            }
        }
"""
new = """            g_object_set(
                gtk_settings,
                \"gtk-application-prefer-dark-theme\",
                prefer_dark,
                NULL);
            while (g_main_context_iteration(NULL, FALSE)) {
            }
            settled_theme_generation = ss_linux_theme_generation();
        }
"""
if text.count(old) != 1:
    raise SystemExit("linux-shell-test.c: theme restore anchor changed")
text = text.replace(old, new, 1)
test.write_text(text)

changelog = Path("CHANGELOG.md")
text = changelog.read_text()
anchor = "- Advance the pinned Common 1.19.38 source to current Common head `7070c5812b50821fd7580101cb2289a3184f6b2c`, including the latest portable-build and hosted-CI corrections.\n"
addition = anchor + "- Make Common-backed GTK theme installation idempotent so reopening the shell no longer removes and re-adds an equivalent display-global CSS provider during the previous window's teardown; add a repeated-activation regression for stable theme generation.\n"
if text.count(anchor) != 1:
    raise SystemExit("CHANGELOG.md: 0.4.50 Common note changed")
text = text.replace(anchor, addition, 1)
changelog.write_text(text)

Path(".github/workflows/hal-0.4.50-native-lifetime-fix.yml").unlink()
Path(".github/hal-0.4.50-native-lifetime-fix.py").unlink()
