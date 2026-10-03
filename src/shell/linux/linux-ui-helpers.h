// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_LINUX_UI_HELPERS_H
#define SYSTEM_SETTINGS_LINUX_UI_HELPERS_H

#include <gtk/gtk.h>

GtkWidget *ss_linux_ui_make_label(const char *text,
                                  const char *css_class);
GtkWidget *ss_linux_ui_make_setting_row(const char *title,
                                        const char *description,
                                        GtkWidget *control);
GtkWidget *ss_linux_ui_make_setting_tile(const char *icon_name,
                                         const char *title,
                                         const char *description,
                                         GtkWidget *control);

/*
 * Large dashboard artwork must not be decoded synchronously while the shell is
 * still constructing its first frame.  Keep the normal GtkPicture contract for
 * callers, but defer the actual filename load to low-priority main-loop work.
 * main.c historically called gtk_picture_new_for_filename() directly in many
 * places; this compatibility alias removes that old first-paint stall without
 * duplicating asset-loading policy throughout the shell.
 */
GtkWidget *ss_linux_ui_picture_new_for_filename(const char *filename);
#ifndef SYSTEM_SETTINGS_LINUX_UI_HELPERS_IMPLEMENTATION
#define gtk_picture_new_for_filename ss_linux_ui_picture_new_for_filename
#endif

#endif
