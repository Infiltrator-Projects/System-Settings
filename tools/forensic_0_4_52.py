#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def text(path):
    return (ROOT / path).read_text(encoding="utf-8")

def write(path, data):
    (ROOT / path).write_text(data, encoding="utf-8")

def replace_once(data, old, new, label):
    count = data.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return data.replace(old, new, 1)

# Version/changelog: 0.4.51 is already published, so this pass is a new release.
write("VERSION", "0.4.52\n")
changelog = text("CHANGELOG.md")
entry = """## 0.4.52 — 2026-10-03

- Forensically remove remaining UI residue from the 0.4.45–0.4.49 iteration cycle: delete dead cyan/gold quick-action classes, remove the obsolete Mercedes Grey appearance preview/asset, and use one trusted executable-resolution path for delegated actions.
- Reduce live Date & Time allocation churn by sharing one `GDateTime` per preview tick and replacing the per-tick heap-allocated date cache key with explicit cached civil-day/calendar state.
- Back off failed optional Calendar-runtime discovery exponentially to a 60-second ceiling instead of rescanning trusted library roots every five seconds on the GTK thread.
- Bring Home and Date & Time geometry back onto the current Common design values (18 px panel radius, 12 px card radius, 10 px control radius, 20 px screen padding) and reserve Common warning gold for actual warning semantics instead of decorative accents.
- Align the UI vision document with the canonical Infiltrator design contract: System/Day/Night follow Common semantic palettes and metrics, while product identity remains in genuine product assets rather than ad-hoc colour splits.
- Re-verified that the pinned Common 1.19.38 commit `7070c5812b50821fd7580101cb2289a3184f6b2c` is the current `Infiltrator-Libraries` main head; no submodule movement is required.

"""
if not changelog.startswith("## 0.4.51"):
    raise SystemExit("Unexpected CHANGELOG head")
write("CHANGELOG.md", entry + changelog)

# Remove a no-longer-shipped legacy preview asset from package inputs.
cmake = text("CMakeLists.txt")
cmake = replace_once(cmake, "        assets/ui/theme-mercedes.png\n", "", "CMake Mercedes preview install")
write("CMakeLists.txt", cmake)

# Shell cleanup: one trusted program resolver, no dead colour classes, no obsolete theme preview.
main = text("src/shell/linux/main.c")
main = replace_once(
    main,
    """    g_autofree gchar *path =\n        program != NULL ? g_find_program_in_path(program) : NULL;\n\n    g_object_set_data_full(\n        G_OBJECT(button),\n        \"action-program\",\n        g_strdup(program),\n        g_free);\n""",
    """    g_autofree gchar *path =\n        find_trusted_system_program(program);\n\n    g_object_set_data_full(\n        G_OBJECT(button),\n        \"action-program\",\n        g_strdup(path),\n        g_free);\n""",
    "trusted quick action launcher")
for stale in (
    '    gtk_widget_add_css_class(date_action, "quick-action-cyan");\n',
    '    gtk_widget_add_css_class(region_action, "quick-action-cyan");\n',
    '    gtk_widget_add_css_class(display_action, "quick-action-cyan");\n',
    '    gtk_widget_add_css_class(software_action, "quick-action-gold");\n',
):
    main = replace_once(main, stale, "", f"dead class {stale.strip()}")
main = replace_once(
    main,
    """        make_theme_preview(\"Light\", \"theme-light\", FALSE), -1);\n    gtk_flow_box_insert(\n        GTK_FLOW_BOX(appearance_previews),\n        make_theme_preview(\"Dark\", \"theme-dark\", FALSE), -1);\n    gtk_flow_box_insert(\n        GTK_FLOW_BOX(appearance_previews),\n        make_theme_preview(\"Follow OS\", \"theme-follow\", FALSE), -1);\n    gtk_flow_box_insert(\n        GTK_FLOW_BOX(appearance_previews),\n        make_theme_preview(\"Mercedes Grey\", \"theme-mercedes\", FALSE), -1);\n""",
    """        make_theme_preview(\"Day\", \"theme-light\", FALSE), -1);\n    gtk_flow_box_insert(\n        GTK_FLOW_BOX(appearance_previews),\n        make_theme_preview(\"Night\", \"theme-dark\", FALSE), -1);\n    gtk_flow_box_insert(\n        GTK_FLOW_BOX(appearance_previews),\n        make_theme_preview(\"System\", \"theme-follow\", FALSE), -1);\n""",
    "appearance preview contract")
main = main.replace("Follow OS/Mercedes state", "System/Day/Night state")
write("src/shell/linux/main.c", main)

# The canonical Common contract owns structural geometry. Keep the dense 0.4.51
# arrangement, but remove accumulated one-off radii/padding from earlier layout fixes.
theme = text("src/shell/linux/linux-theme.c")
theme = replace_once(theme, "    gchar *warm;\n", "", "warning alias declaration")
theme = replace_once(theme, "    warm = rgb_css(palette->warning_rgb);\n", "", "warning alias init")ntheme = theme.replace("        warm, accent, success,\n", "        accent, accent, success,\n")
theme = theme.replace("        warm, (unsigned int)type->ui_bold_weight,\n", "        accent, (unsigned int)type->ui_bold_weight,\n")
theme = theme.replace("        warm, accent, warm, success,\n", "        accent, accent, accent, success,\n")
theme = replace_once(theme, "    g_free(warm);\n", "", "warning alias free")
geometry = {
    ".page-icon { min-width: 54px; min-height: 54px; background: %s; border: 1px solid %s; border-radius: 15px; padding: 10px; }": ".page-icon { min-width: 54px; min-height: 54px; background: %s; border: 1px solid %s; border-radius: 12px; padding: 10px; }",
    ".hero-card { background: %s; border: 1px solid %s; border-radius: 16px; padding: 16px 18px; box-shadow: none; }": ".hero-card { background: %s; border: 1px solid %s; border-radius: 18px; padding: 16px 18px; box-shadow: none; }",
    ".overview-panel { min-width: 250px; background: %s; border: 1px solid %s; border-radius: 14px; padding: 10px; }": ".overview-panel { min-width: 250px; background: %s; border: 1px solid %s; border-radius: 12px; padding: 10px; }",
    ".settings-card { background: %s; border: 1px solid %s; border-top-width: 2px; border-radius: 14px; padding: 14px; box-shadow: none; }": ".settings-card { background: %s; border: 1px solid %s; border-top-width: 2px; border-radius: 12px; padding: 14px; box-shadow: none; }",
    ".setting-tile { background: %s; border: 1px solid %s; border-radius: 11px; padding: 8px 10px; margin: 2px 0; }": ".setting-tile { background: %s; border: 1px solid %s; border-radius: 10px; padding: 8px 10px; margin: 2px 0; }",
    ".home-page { padding: 22px 26px 28px 26px; }": ".home-page { padding: 20px; }",
    ".home-hero { min-height: 270px; padding: 0; border: 0; border-radius: 20px; box-shadow: none; }": ".home-hero { min-height: 270px; padding: 0; border: 0; border-radius: 18px; box-shadow: none; }",
    ".hero-copy-overlay { min-width: 0; padding: 14px 18px; margin: 12px; border-radius: 15px; background: rgba(0,0,0,0.56); }": ".hero-copy-overlay { min-width: 0; padding: 14px 18px; margin: 12px; border-radius: 12px; background: rgba(0,0,0,0.56); }",
    ".hero-brand-overlay { margin: 14px; padding: 12px 14px; background: rgba(0,0,0,0.46); border: 1px solid %s; border-radius: 13px; box-shadow: none; }": ".hero-brand-overlay { margin: 14px; padding: 12px 14px; background: rgba(0,0,0,0.46); border: 1px solid %s; border-radius: 12px; box-shadow: none; }",
    ".home-card, .status-card { background: %s; border: 1px solid %s; border-radius: 17px; padding: 16px 17px; box-shadow: none; }": ".home-card, .status-card { background: %s; border: 1px solid %s; border-radius: 12px; padding: 16px; box-shadow: none; }",
    ".quick-action { min-height: 68px; padding: 8px 10px; background: %s; border: 1px solid %s; border-radius: 13px; }": ".quick-action { min-height: 68px; padding: 8px 10px; background: %s; border: 1px solid %s; border-radius: 10px; }",
}
for old, new in geometry.items():
    theme = replace_once(theme, old, new, f"theme geometry {old[:24]}")
write("src/shell/linux/linux-theme.c", theme)

# Date & Time: one wall-clock sample per tick and an allocation-free date-cache hit path.
header = text("src/modules/date-time/linux-date-time-panel-private.h")
header = replace_once(
    header,
    """    guint manual_time_generation;\n    bool location_metadata_present;\n""",
    """    guint manual_time_generation;\n    gint preview_date_year;\n    gint preview_date_month;\n    gint preview_date_day;\n    gchar *preview_date_calendar;\n    bool preview_date_cache_valid;\n    bool location_metadata_present;\n""",
    "preview cache fields")
write("src/modules/date-time/linux-date-time-panel-private.h", header)

panel = text("src/modules/date-time/linux-date-time-panel.c")
panel = replace_once(
    panel,
    """static bool format_preview(SsLinuxDateTimePanel *state,\n                           char *buffer,\n                           size_t capacity)\n{\n    const InfiltratrTemporalPolicyV3 *policy =\n        ss_date_time_model_policy(&state->model);\n    const InfiltratrTemporalClockModeInfo *mode;\n    g_autoptr(GDateTime) now = g_date_time_new_now_local();\n""",
    """static bool format_preview_at(SsLinuxDateTimePanel *state,\n                              GDateTime *now,\n                              char *buffer,\n                              size_t capacity)\n{\n    const InfiltratrTemporalPolicyV3 *policy =\n        ss_date_time_model_policy(&state->model);\n    const InfiltratrTemporalClockModeInfo *mode;\n""",
    "single preview time sample")
panel = replace_once(
    panel,
    """    if (format_preview(state, clock_text, sizeof(clock_text)) &&\n""",
    """    if (format_preview_at(state, now, clock_text, sizeof(clock_text)) &&\n""",
    "preview call")
old_cache = """    const gint year = g_date_time_get_year(now);\n    const gint month = g_date_time_get_month(now);\n    const gint day = g_date_time_get_day_of_month(now);\n    g_autofree gchar *date_key = g_strdup_printf(\n        \"%04d-%02d-%02d|%s\",\n        year, month, day, policy->calendar);\n    const char *cached_key = g_object_get_data(\n        G_OBJECT(state->date_preview),\n        \"system-settings-preview-date-key\");\n\n    if (g_strcmp0(cached_key, date_key) != 0) {\n"""
new_cache = """    const gint year = g_date_time_get_year(now);\n    const gint month = g_date_time_get_month(now);\n    const gint day = g_date_time_get_day_of_month(now);\n    const bool date_cache_hit =\n        state->preview_date_cache_valid &&\n        state->preview_date_year == year &&\n        state->preview_date_month == month &&\n        state->preview_date_day == day &&\n        g_strcmp0(state->preview_date_calendar, policy->calendar) == 0;\n\n    if (!date_cache_hit) {\n"""
panel = replace_once(panel, old_cache, new_cache, "allocation-free date cache lookup")
panel = replace_once(
    panel,
    """        if (cacheable) {\n            g_object_set_data_full(\n                G_OBJECT(state->date_preview),\n                \"system-settings-preview-date-key\",\n                g_strdup(date_key),\n                g_free);\n        }\n""",
    """        if (cacheable) {\n            state->preview_date_year = year;\n            state->preview_date_month = month;\n            state->preview_date_day = day;\n            g_free(state->preview_date_calendar);\n            state->preview_date_calendar = g_strdup(policy->calendar);\n            state->preview_date_cache_valid =\n                state->preview_date_calendar != NULL;\n        }\n""",
    "date cache update")
panel = replace_once(
    panel,
    """    g_clear_pointer(&state->clock_mode_ids, g_ptr_array_unref);\n    g_clear_pointer(&state->timezone_ids, g_ptr_array_unref);\n""",
    """    g_clear_pointer(&state->clock_mode_ids, g_ptr_array_unref);\n    g_clear_pointer(&state->timezone_ids, g_ptr_array_unref);\n    g_clear_pointer(&state->preview_date_calendar, g_free);\n""",
    "date cache cleanup")
write("src/modules/date-time/linux-date-time-panel.c", panel)

# Optional Calendar is not authoritative functionality. A missing library must not
# create a synchronous trusted-root scan every five seconds on the UI thread.
provider = text("src/modules/date-time/calendar-preview-provider.c")
provider = replace_once(
    provider,
    "#define DISCOVERY_RETRY_USEC (5 * G_USEC_PER_SEC)\n",
    "#define DISCOVERY_RETRY_INITIAL_USEC (5 * G_USEC_PER_SEC)\n#define DISCOVERY_RETRY_MAX_USEC (60 * G_USEC_PER_SEC)\n",
    "calendar retry constants")
provider = replace_once(
    provider,
    """    char *discovery_root_override;\n    gint64 retry_after_monotonic_us;\n};\n""",
    """    char *discovery_root_override;\n    gint64 retry_after_monotonic_us;\n    gint64 retry_delay_us;\n};\n""",
    "calendar retry state")
provider = replace_once(
    provider,
    """    provider->retry_after_monotonic_us =\n        now + DISCOVERY_RETRY_USEC;\n    if (discover_runtime(provider)) {\n        provider->retry_after_monotonic_us = 0;\n        return true;\n    }\n    return false;\n""",
    """    if (provider->retry_delay_us <= 0) {\n        provider->retry_delay_us = DISCOVERY_RETRY_INITIAL_USEC;\n    }\n    provider->retry_after_monotonic_us =\n        now + provider->retry_delay_us;\n    if (discover_runtime(provider)) {\n        provider->retry_after_monotonic_us = 0;\n        provider->retry_delay_us = DISCOVERY_RETRY_INITIAL_USEC;\n        return true;\n    }\n    provider->retry_delay_us = MIN(\n        provider->retry_delay_us * 2,\n        (gint64)DISCOVERY_RETRY_MAX_USEC);\n    return false;\n""",
    "calendar exponential backoff")
provider = replace_once(
    provider,
    """    provider->library = (InfiltratrDynlib)INFILTRATR_DYNLIB_INIT;\n    provider->discover_default_runtime = true;\n    return provider;\n""",
    """    provider->library = (InfiltratrDynlib)INFILTRATR_DYNLIB_INIT;\n    provider->discover_default_runtime = true;\n    provider->retry_delay_us = DISCOVERY_RETRY_INITIAL_USEC;\n    return provider;\n""",
    "calendar retry initialization")
provider = replace_once(
    provider,
    """    if (provider != NULL) {\n        provider->retry_after_monotonic_us = 0;\n    }\n""",
    """    if (provider != NULL) {\n        provider->retry_after_monotonic_us = 0;\n        provider->retry_delay_us = DISCOVERY_RETRY_INITIAL_USEC;\n    }\n""",
    "calendar forced retry reset")
write("src/modules/date-time/calendar-preview-provider.c", provider)

# Documentation: Common is now the visual authority, not the old warm-gold prototype palette.
vision = text("docs/UI-VISION.md")
vision = replace_once(
    vision,
    """- blue/cyan and warm gold accents over the project's dark Mercedes-grey base;\n""",
    """- Common's semantic System/Day/Night palettes, with the neutral accent used for selection/focus and warning/success/fault colours reserved for those actual states;\n""",
    "UI vision palette")
vision = replace_once(
    vision,
    """The prototype establishes the desired level of finish, hierarchy, colour and\ngraphical density. Individual imagery, labels, sample modules and decorative\ncontent may change as real modules are implemented. Hero artwork and any\nrecognisable third-party marks shown in concept imagery are mood references only\nand are not a requirement for shipped assets.\n""",
    """The prototype establishes the desired level of finish, hierarchy and graphical\ndensity, but the canonical visual contract is the current Common\n`infiltrator-design-v1` contract. Shared typography, semantic palette roles,\nradii and spacing follow Common; product-specific imagery remains local.\nIndividual imagery, labels and sample modules may change as real modules are\nimplemented. Hero artwork and any recognisable third-party marks shown in\nconcept imagery are mood references only and are not a requirement for shipped\nassets.\n""",
    "UI vision authority")
write("docs/UI-VISION.md", vision)

# Remove the unused 2 MB legacy preview image after its only code/package references are gone.
legacy_asset = ROOT / "assets/ui/theme-mercedes.png"
if not legacy_asset.exists():
    raise SystemExit("Legacy theme-mercedes asset unexpectedly absent")
legacy_asset.unlink()

# Make sure the stale identifiers really are gone.
joined = "\n".join([
    text("src/shell/linux/main.c"),
    text("src/shell/linux/linux-theme.c"),
    text("CMakeLists.txt"),
    text("docs/UI-VISION.md"),
])
for forbidden in ("quick-action-cyan", "quick-action-gold", "theme-mercedes", "Mercedes Grey"):
    if forbidden in joined:
        raise SystemExit(f"stale identifier remains: {forbidden}")

print("0.4.52 forensic cleanup applied")
