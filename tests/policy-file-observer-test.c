// SPDX-License-Identifier: GPL-3.0-or-later
#include "policy-file-observer.h"

#include <glib.h>
#include <glib/gstdio.h>

static guint changes;

static void changed(gpointer user_data)
{
    guint *count = user_data;
    ++(*count);
}

static bool spin_until(guint minimum)
{
    for (guint attempt = 0U; attempt < 3000U; ++attempt) {
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        if (changes >= minimum) {
            return true;
        }
        g_usleep(1000U);
    }
    return false;
}

int main(void)
{
    g_autofree gchar *root =
        g_dir_make_tmp("ss-policy-observer-XXXXXX", NULL);
    g_autofree gchar *directory =
        g_build_filename(root, "infiltrator", NULL);
    g_autofree gchar *path =
        g_build_filename(directory, "presentation.conf", NULL);
    g_autofree gchar *temporary =
        g_build_filename(directory, ".presentation.tmp", NULL);
    SsPolicyFileObserver *observer;

    g_assert_nonnull(root);

    /* Construct before the target directory or file exists. */
    observer = ss_policy_file_observer_new(path, changed, &changes);
    g_assert_nonnull(observer);

    g_assert_cmpint(g_mkdir(directory, 0700), ==, 0);
    g_assert_true(g_file_set_contents(temporary, "one\n", -1, NULL));
    g_assert_cmpint(g_rename(temporary, path), ==, 0);
    g_assert_true(spin_until(1U));

    const guint after_create = changes;
    g_assert_true(g_file_set_contents(temporary, "two\n", -1, NULL));
    g_assert_cmpint(g_rename(temporary, path), ==, 0);
    g_assert_true(spin_until(after_create + 1U));

    const guint after_replace = changes;
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_true(spin_until(after_replace + 1U));

    ss_policy_file_observer_free(observer);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0);
    return 0;
}
