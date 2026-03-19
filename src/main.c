#include <gtk/gtk.h>

#include "clipboard.h"
#include "config.h"
#include "dedup.h"
#include "models.h"
#include "window.h"

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    Config config = config_defaults();

    /* Load CSS: user override first, then embedded resource */
    GtkCssProvider *provider = gtk_css_provider_new();
    char *user_css = g_build_filename(
        g_get_home_dir(), ".config", "cliphist-picker",
        "themes", "catppuccin-mocha.css", NULL);

    if (g_file_test(user_css, G_FILE_TEST_EXISTS)) {
        gtk_css_provider_load_from_path(provider, user_css);
    } else {
        gtk_css_provider_load_from_resource(provider,
            "/com/github/cliphist-picker/themes/catppuccin-mocha.css");
    }
    g_free(user_css);

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    /* Fetch clipboard entries */
    int raw_count = 0;
    RawEntry *raw = clipboard_list(&raw_count);
    if (!raw || raw_count == 0) {
        g_free(raw);
        g_application_quit(G_APPLICATION(app));
        return;
    }

    /* Deduplicate */
    int entry_count = 0;
    ClipboardEntry *entries = deduplicate(
        raw, raw_count,
        config.dedup_id_threshold,
        config.max_entries,
        &entry_count);

    /* Free raw entries */
    for (int i = 0; i < raw_count; i++)
        raw_entry_free(&raw[i]);
    g_free(raw);

    if (entry_count == 0) {
        g_free(entries);
        g_application_quit(G_APPLICATION(app));
        return;
    }

    /* Create and show picker */
    PickerWindow *win = picker_window_new(app, entries, entry_count, &config);
    gtk_window_present(win->window);
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new(
        "com.github.cliphist-picker",
        G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
