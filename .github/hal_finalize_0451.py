from pathlib import Path

p = Path('CHANGELOG.md')
s = p.read_text()
old = '''## 0.4.50 — 2026-10-03

- Compact the Date & Time page: prevent GtkSwitch controls from expanding into row-width sliders, keep Latitude/Longitude paired in a fixed two-column grid, use Common success green for LIVE state, and reduce excess card/control spacing.

'''
new = '''## 0.4.51 — 2026-10-03

- Compact the Date & Time page: prevent GtkSwitch controls from expanding into row-width sliders, keep Latitude/Longitude paired in a fixed two-column grid, use Common success green for LIVE state, and reduce excess card/control spacing.

## 0.4.50 — 2026-10-03

'''
if s.count(old) != 1:
    raise SystemExit('expected patched 0.4.50 changelog header exactly once')
p.write_text(s.replace(old, new, 1))

p = Path('tests/linux-shell-test.c')
s = p.read_text()
needle = '''        g_assert_nonnull(panel);
        g_assert_nonnull(panel->policy_observer);
'''
insert = '''        g_assert_nonnull(panel);
        g_assert_nonnull(panel->policy_observer);
        /* Compact binary controls must never inherit row-width expansion. */
        g_assert_false(gtk_widget_compute_expand(
            GTK_WIDGET(panel->show_seconds), GTK_ORIENTATION_HORIZONTAL));
        g_assert_false(gtk_widget_compute_expand(
            GTK_WIDGET(panel->show_date), GTK_ORIENTATION_HORIZONTAL));
        {
            int switch_width = 0;
            int switch_height = 0;
            gtk_widget_get_size_request(
                GTK_WIDGET(panel->show_seconds),
                &switch_width,
                &switch_height);
            g_assert_cmpint(switch_width, ==, 38);
            g_assert_cmpint(switch_height, ==, 20);
        }
        /* Latitude and Longitude are one paired control and must share a grid. */
        GtkWidget *latitude_field = gtk_widget_get_parent(GTK_WIDGET(panel->latitude));
        GtkWidget *longitude_field = gtk_widget_get_parent(GTK_WIDGET(panel->longitude));
        g_assert_nonnull(latitude_field);
        g_assert_nonnull(longitude_field);
        g_assert_true(GTK_IS_GRID(gtk_widget_get_parent(latitude_field)));
        g_assert_true(
            gtk_widget_get_parent(latitude_field) ==
            gtk_widget_get_parent(longitude_field));
'''
if s.count(needle) != 1:
    raise SystemExit('shell test insertion point not unique')
p.write_text(s.replace(needle, insert, 1))
