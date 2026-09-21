// SPDX-License-Identifier: GPL-3.0-or-later
#include "linux-ui-helpers.h"

static GtkWidget *make_setting_identity(const char *title,
                                        const char *description)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *heading =
        ss_linux_ui_make_label(title, "setting-label");
    GtkWidget *copy =
        ss_linux_ui_make_label(description, "setting-description");

    gtk_label_set_wrap(GTK_LABEL(copy), TRUE);
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

    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_widget_add_css_class(row, "setting-row");
    gtk_widget_set_hexpand(row, TRUE);
    gtk_box_append(
        GTK_BOX(row),
        make_setting_identity(title, description));
    gtk_widget_set_valign(control, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(row), control);
    return row;
}
