// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-builtin-module-host.c
 * @brief Composition boundary between the generic Linux shell and built-in modules.
 *
 * This is deliberately the only production translation unit outside individual
 * modules that knows their private GTK bridges. The shell consumes the generic
 * host API and therefore never owns module-private controller state.
 */

#include "builtin-module-host.h"

#include "linux-date-time-panel.h"

#include <gio/gio.h>

typedef gpointer (*SsBuiltinModuleCreateFunc)(GtkWindow *window, GError **error);
typedef GtkWidget *(*SsBuiltinModuleWidgetFunc)(gpointer controller);
typedef void (*SsBuiltinModuleDestroyFunc)(gpointer controller);

typedef struct SsBuiltinModuleDescriptor {
    const char *page_id;
    SsBuiltinModuleCreateFunc create;
    SsBuiltinModuleWidgetFunc widget;
    SsBuiltinModuleDestroyFunc destroy;
} SsBuiltinModuleDescriptor;

typedef struct SsBuiltinModuleInstance {
    const SsBuiltinModuleDescriptor *descriptor;
    gpointer controller;
} SsBuiltinModuleInstance;

struct SsBuiltinModuleHost {
    GtkWindow *window;
    GHashTable *instances;
};

static gpointer create_date_time_module(GtkWindow *window, GError **error)
{
    return ss_linux_date_time_panel_new(window, error);
}

static GtkWidget *date_time_module_widget(gpointer controller)
{
    return ss_linux_date_time_panel_widget(controller);
}

static void destroy_date_time_module(gpointer controller)
{
    ss_linux_date_time_panel_free(controller);
}

static const SsBuiltinModuleDescriptor builtin_modules[] = {
    {
        .page_id = "date-time",
        .create = create_date_time_module,
        .widget = date_time_module_widget,
        .destroy = destroy_date_time_module,
    },
};

static const SsBuiltinModuleDescriptor *find_module_descriptor(
    const char *page_id)
{
    if (page_id == NULL || page_id[0] == '\0') {
        return NULL;
    }

    for (guint index = 0U; index < G_N_ELEMENTS(builtin_modules); ++index) {
        if (g_strcmp0(page_id, builtin_modules[index].page_id) == 0) {
            return &builtin_modules[index];
        }
    }
    return NULL;
}

static void builtin_module_instance_free(gpointer data)
{
    SsBuiltinModuleInstance *instance = data;

    if (instance == NULL) {
        return;
    }
    if (instance->controller != NULL &&
        instance->descriptor != NULL &&
        instance->descriptor->destroy != NULL) {
        instance->descriptor->destroy(instance->controller);
    }
    g_free(instance);
}

SsBuiltinModuleHost *ss_builtin_module_host_new(GtkWindow *window)
{
    SsBuiltinModuleHost *host;

    if (window == NULL) {
        return NULL;
    }

    host = g_new0(SsBuiltinModuleHost, 1);
    host->window = window;
    host->instances = g_hash_table_new_full(
        g_str_hash,
        g_str_equal,
        NULL,
        builtin_module_instance_free);
    return host;
}

void ss_builtin_module_host_free(SsBuiltinModuleHost *host)
{
    if (host == NULL) {
        return;
    }

    /*
     * Async module work owns explicit lifetime anchors. The host therefore
     * tears modules down directly and does not publish controller pointers
     * through GtkWindow object-data side channels.
     */
    g_clear_pointer(&host->instances, g_hash_table_unref);
    g_free(host);
}

bool ss_builtin_module_host_ensure_page(
    SsBuiltinModuleHost *host,
    const char *page_id,
    GtkScrolledWindow *container,
    GError **error)
{
    const SsBuiltinModuleDescriptor *descriptor;
    SsBuiltinModuleInstance *instance;
    GtkWidget *panel_widget;

    if (host == NULL || container == NULL) {
        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_INVALID_ARGUMENT,
            "Invalid built-in module host or page container.");
        return false;
    }

    descriptor = find_module_descriptor(page_id);
    if (descriptor == NULL) {
        g_set_error(
            error,
            G_IO_ERROR,
            G_IO_ERROR_NOT_SUPPORTED,
            "Unknown built-in System Settings page: %s",
            page_id != NULL ? page_id : "(null)");
        return false;
    }

    instance = g_hash_table_lookup(host->instances, descriptor->page_id);
    if (instance == NULL) {
        gpointer controller = descriptor->create(host->window, error);

        if (controller == NULL) {
            return false;
        }

        instance = g_new0(SsBuiltinModuleInstance, 1);
        instance->descriptor = descriptor;
        instance->controller = controller;
        g_hash_table_insert(
            host->instances,
            (gpointer)descriptor->page_id,
            instance);

    }

    panel_widget = descriptor->widget(instance->controller);
    if (panel_widget == NULL) {
        g_set_error(
            error,
            G_IO_ERROR,
            G_IO_ERROR_FAILED,
            "The built-in %s module did not provide a panel.",
            descriptor->page_id);
        return false;
    }

    if (gtk_scrolled_window_get_child(container) != panel_widget) {
        gtk_scrolled_window_set_child(container, panel_widget);
    }
    return true;
}

bool ss_builtin_module_host_is_loaded(
    const SsBuiltinModuleHost *host,
    const char *page_id)
{
    return host != NULL && host->instances != NULL &&
           page_id != NULL &&
           g_hash_table_contains(host->instances, page_id);
}
