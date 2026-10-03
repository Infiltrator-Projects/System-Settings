// SPDX-License-Identifier: GPL-3.0-or-later
#include "linux-ui-helpers.h"

#include <infiltratr/design.h>

static const InfiltratrDesignMetrics *ui_metrics(void)
{
    return infiltratr_design_metrics();
}

static int compact_spacing(void)
{
    const InfiltratrDesignMetrics *metrics = ui_metrics();
    return metrics != NULL ? (int)metrics->compact_spacing : 6;
}

static int control_spacing(void)
{
    const InfiltratrDesignMetrics *metrics = ui_metrics();
    return metrics != NULL ? (int)metrics->control_spacing : 10;
}

static GtkWidget *make_setting_identity(const char *title,
                                        const char *description)
{
    GtkWidget *box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        compact_spacing());
    GtkWidget *heading =
        ss_linux_ui_make_label(title, "setting-label");
    GtkWidget *copy =
        ss_linux_ui_make_label(description, "setting-description");

    gtk_label_set_wrap(GTK_LABEL(copy), TRUE);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_hexpand(copy, TRUE);
    gtk_box_append(GTK_BOX(box), heading);
    gtk_box_append(GTK_BOX(box), copy);
    return box;
}

GtkWidget *ss_linux_ui_make_label(const char *text,
                                  const char *css_class)
{
    GtkWidget *label = gtk_label_new(text);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    if (css_class != NULL) {
        gtk_widget_add_css_class(label, css_class);
    }
    return label;
}

GtkWidget *ss_linux_ui_make_setting_row(const char *title,
                                        const char *description,
                                        GtkWidget *control)
{
    GtkWidget *row;

    if (control == NULL) {
        return NULL;
    }

    /*
     * A setting row is one control group, not a section boundary. Earlier UI
     * iterations used Common's section spacing here, which made every row feel
     * unnecessarily loose compared with the rest of InfiltratorOS. Keep the
     * section metric for section-to-section layout and use the canonical
     * control spacing inside the row itself.
     */
    row = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL,
        control_spacing());
    gtk_widget_add_css_class(row, "setting-row");
    gtk_widget_set_hexpand(row, TRUE);
    gtk_box_append(
        GTK_BOX(row),
        make_setting_identity(title, description));
    gtk_widget_set_valign(control, GTK_ALIGN_CENTER);
    if (GTK_IS_SWITCH(control)) {
        /* A switch is a compact binary control, never a row-width slider. */
        gtk_widget_set_hexpand(control, FALSE);
        gtk_widget_set_size_request(control, 38, 20);
    }
    gtk_widget_set_halign(control, GTK_ALIGN_END);
    gtk_accessible_update_property(
        GTK_ACCESSIBLE(control),
        GTK_ACCESSIBLE_PROPERTY_LABEL,
        title,
        GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
        description,
        -1);
    gtk_box_append(GTK_BOX(row), control);
    return row;
}

GtkWidget *ss_linux_ui_make_setting_tile(const char *icon_name,
                                         const char *title,
                                         const char *description,
                                         GtkWidget *control)
{
    GtkWidget *tile;
    GtkWidget *icon_wrap;
    GtkWidget *icon;

    if (control == NULL) {
        return NULL;
    }

    tile = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL,
        control_spacing());
    icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    icon = gtk_image_new_from_icon_name(icon_name);

    gtk_widget_add_css_class(tile, "setting-tile");
    gtk_widget_add_css_class(icon_wrap, "setting-tile-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 19);
    gtk_box_append(GTK_BOX(icon_wrap), icon);
    gtk_box_append(GTK_BOX(tile), icon_wrap);
    gtk_box_append(
        GTK_BOX(tile),
        make_setting_identity(title, description));
    gtk_widget_set_hexpand(tile, TRUE);
    gtk_widget_set_valign(control, GTK_ALIGN_CENTER);
    if (GTK_IS_SWITCH(control)) {
        /* A switch is a compact binary control, never a row-width slider. */
        gtk_widget_set_hexpand(control, FALSE);
        gtk_widget_set_size_request(control, 38, 20);
    }
    gtk_widget_set_halign(control, GTK_ALIGN_END);
    gtk_accessible_update_property(
        GTK_ACCESSIBLE(control),
        GTK_ACCESSIBLE_PROPERTY_LABEL,
        title,
        GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
        description,
        -1);
    gtk_box_append(GTK_BOX(tile), control);
    return tile;
}
