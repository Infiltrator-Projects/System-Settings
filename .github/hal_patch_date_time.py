from pathlib import Path


def replace(path, old, new, count=1):
    p = Path(path)
    s = p.read_text()
    actual = s.count(old)
    if actual != count:
        raise SystemExit(f"{path}: expected {count} occurrences, found {actual} for {old[:80]!r}")
    p.write_text(s.replace(old, new, count))


theme = "src/shell/linux/linux-theme.c"
replacements = [
    ('".settings-content { padding: 18px 20px 22px 20px; }\\n"', '".settings-content { padding: 12px 16px 16px 16px; }\\n"'),
    ('".page-title { color: %s; font-size: 36px; font-weight: %u; }\\n"', '".page-title { color: %s; font-size: 32px; font-weight: %u; }\\n"'),
    ('".hero-card { background: %s; border: 1px solid %s; border-radius: 20px; padding: 26px 28px; box-shadow: none; }\\n"', '".hero-card { background: %s; border: 1px solid %s; border-radius: 16px; padding: 16px 18px; box-shadow: none; }\\n"'),
    ('".hero-top { min-height: 94px; }\\n"', '".hero-top { min-height: 76px; }\\n"'),
    ('".preview-time { color: %s; font-size: 46px; font-weight: %u; }\\n"', '".preview-time { color: %s; font-size: 40px; font-weight: %u; }\\n"'),
    ('".hero-note { color: %s; font-size: 11px; margin-top: 6px; }\\n"', '".hero-note { color: %s; font-size: 10px; margin-top: 3px; }\\n"'),
    ('".overview-panel { min-width: 260px; background: %s; border: 1px solid %s; border-radius: 16px; padding: 14px; }\\n"', '".overview-panel { min-width: 250px; background: %s; border: 1px solid %s; border-radius: 14px; padding: 10px; }\\n"'),
    ('".overview-item { background: %s; border: 1px solid %s; border-radius: 12px; padding: 10px 11px; min-height: 56px; }\\n"', '".overview-item { background: %s; border: 1px solid %s; border-radius: 10px; padding: 7px 9px; min-height: 46px; }\\n"'),
    ('".settings-card { background: %s; border: 1px solid %s; border-top-width: 2px; border-radius: 16px; padding: 20px; box-shadow: none; }\\n"', '".settings-card { background: %s; border: 1px solid %s; border-top-width: 2px; border-radius: 14px; padding: 14px; box-shadow: none; }\\n"'),
    ('".section-heading { margin-bottom: 8px; padding-bottom: 6px; border-bottom: 1px solid %s; }\\n"', '".section-heading { margin-bottom: 6px; padding-bottom: 5px; border-bottom: 1px solid %s; }\\n"'),
    ('".section-icon-wrap { background: %s; border: 1px solid %s; border-radius: 12px; padding: 9px; }\\n"', '".section-icon-wrap { background: %s; border: 1px solid %s; border-radius: 10px; padding: 7px; }\\n"'),
    ('".setting-tile { background: %s; border: 1px solid %s; border-radius: 13px; padding: 12px 13px; margin: 3px 0; }\\n"', '".setting-tile { background: %s; border: 1px solid %s; border-radius: 11px; padding: 8px 10px; margin: 2px 0; }\\n"'),
    ('".setting-tile-icon { min-width: 38px; min-height: 38px; background: %s; border: 1px solid %s; border-radius: 11px; padding: 6px; }\\n"', '".setting-tile-icon { min-width: 32px; min-height: 32px; background: %s; border: 1px solid %s; border-radius: 9px; padding: 5px; }\\n"'),
    ('".info-strip { background: %s; border: 1px solid %s; border-radius: 10px; padding: 9px 11px; }\\n"', '".info-strip { background: %s; border: 1px solid %s; border-radius: 9px; padding: 7px 9px; }\\n"'),
    ('".setting-dropdown, .setting-spin, .setting-entry, .setting-button { min-height: 38px; border-radius: %upx; }\\n"', '".setting-dropdown, .setting-spin, .setting-entry, .setting-button { min-height: 34px; border-radius: %upx; }\\n"'),
    ('".setting-switch { min-width: 44px; min-height: 24px; background: %s; border: 1px solid %s; border-radius: 12px; box-shadow: none; }\\n"', '".setting-switch { min-width: 38px; min-height: 20px; background: %s; border: 1px solid %s; border-radius: 10px; box-shadow: none; }\\n"'),
    ('".setting-switch slider { min-width: 18px; min-height: 18px; margin: 2px; background: %s; border: none; border-radius: 9px; box-shadow: none; }\\n"', '".setting-switch slider { min-width: 16px; min-height: 16px; margin: 1px; background: %s; border: none; border-radius: 8px; box-shadow: none; }\\n"'),
]
for old, new in replacements:
    replace(theme, old, new)
replace(theme,
    "        selected, accent, accent_foreground,\n        (unsigned int)type->ui_bold_weight,",
    "        surface, success, success,\n        (unsigned int)type->ui_bold_weight,")

helper = "src/shell/linux/linux-ui-helpers.c"
replace(helper,
    "    gtk_widget_set_valign(control, GTK_ALIGN_CENTER);\n    gtk_widget_set_halign(control, GTK_ALIGN_END);",
    "    gtk_widget_set_valign(control, GTK_ALIGN_CENTER);\n    if (GTK_IS_SWITCH(control)) {\n        /* A switch is a compact binary control, never a row-width slider. */\n        gtk_widget_set_hexpand(control, FALSE);\n        gtk_widget_set_size_request(control, 38, 20);\n    }\n    gtk_widget_set_halign(control, GTK_ALIGN_END);",
    count=2)

ui = "src/modules/date-time/linux-date-time-panel-ui.c"
old = '''    coordinate_box = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(coordinate_box), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(
        GTK_FLOW_BOX(coordinate_box), 1U);
    gtk_flow_box_set_max_children_per_line(
        GTK_FLOW_BOX(coordinate_box), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(coordinate_box), 10U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(coordinate_box), 8U);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(coordinate_box),
        make_coordinate_field("LATITUDE", state->latitude), -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(coordinate_box),
        make_coordinate_field("LONGITUDE", state->longitude), -1);'''
new = '''    /* Coordinates are one paired value. A FlowBox could wrap Longitude onto a
     * second line at ordinary desktop widths, visually separating the pair.
     * Keep them in a deterministic two-column grid instead. */
    coordinate_box = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(coordinate_box), 10U);
    gtk_widget_set_hexpand(coordinate_box, FALSE);
    GtkWidget *latitude_field =
        make_coordinate_field("LATITUDE", state->latitude);
    GtkWidget *longitude_field =
        make_coordinate_field("LONGITUDE", state->longitude);
    gtk_grid_attach(GTK_GRID(coordinate_box), latitude_field, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(coordinate_box), longitude_field, 1, 0, 1, 1);'''
replace(ui, old, new)
replace(ui,
    "    gtk_widget_set_size_request(GTK_WIDGET(state->latitude), 116, -1);\n    gtk_widget_set_size_request(GTK_WIDGET(state->longitude), 116, -1);",
    "    gtk_widget_set_size_request(GTK_WIDGET(state->latitude), 112, -1);\n    gtk_widget_set_size_request(GTK_WIDGET(state->longitude), 112, -1);\n    gtk_widget_set_hexpand(GTK_WIDGET(state->latitude), FALSE);\n    gtk_widget_set_hexpand(GTK_WIDGET(state->longitude), FALSE);")
replace(ui,
    "    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(lower), 18U);\n    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(lower), 18U);",
    "    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(lower), 12U);\n    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(lower), 12U);")
for expr in ["state->show_seconds", "state->show_date", "state->network_time"]:
    needle = f'    gtk_widget_add_css_class(\n        GTK_WIDGET({expr}), "setting-switch");'
    repl = needle + f'\n    gtk_widget_set_hexpand(GTK_WIDGET({expr}), FALSE);\n    gtk_widget_set_size_request(GTK_WIDGET({expr}), 38, 20);'
    replace(ui, needle, repl)

changelog = Path("CHANGELOG.md")
text = changelog.read_text()
marker = "## 0.4.50"
idx = text.find(marker)
if idx < 0:
    raise SystemExit("0.4.50 changelog section missing")
line_end = text.find("\n", idx)
entry = "\n- Compact the Date & Time page: prevent GtkSwitch controls from expanding into row-width sliders, keep Latitude/Longitude paired in a fixed two-column grid, use Common success green for LIVE state, and reduce excess card/control spacing.\n"
if entry.strip() not in text:
    text = text[:line_end + 1] + entry + text[line_end + 1:]
    changelog.write_text(text)
