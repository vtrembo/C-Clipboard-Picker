#include <gtk/gtk.h>
#include <string.h>

#include "clipboard.h"
#include "config.h"
#include "dedup.h"
#include "models.h"
#include "window.h"

static PickerWindow *g_picker = NULL;
static Config        g_config;

/* ── Minimal key=value config parser ───────────────────────────── */

/* Read a value for a key from a config file. Returns allocated string or NULL. */
static char *conf_read_key(const char *path, const char *key) {
    char *contents = NULL;
    if (!g_file_get_contents(path, &contents, NULL, NULL))
        return NULL;

    char *result = NULL;
    gchar **lines = g_strsplit(contents, "\n", -1);
    gsize key_len = strlen(key);

    for (int i = 0; lines[i]; i++) {
        char *line = g_strstrip(lines[i]);
        if (line[0] == '#' || line[0] == '\0')
            continue;
        if (strncmp(line, key, key_len) == 0) {
            char *eq = line + key_len;
            while (*eq == ' ' || *eq == '\t') eq++;
            if (*eq == '=') {
                eq++;
                while (*eq == ' ' || *eq == '\t') eq++;
                result = g_strdup(eq);
                break;
            }
        }
    }

    g_strfreev(lines);
    g_free(contents);
    return result;
}

/* ── Ensure default config files exist ─────────────────────────── */

static void copy_resource_to_file(const char *resource_path,
                                  const char *dest_path) {
    if (g_file_test(dest_path, G_FILE_TEST_EXISTS))
        return;

    GBytes *bytes = g_resources_lookup_data(resource_path,
        G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
    if (!bytes) return;

    gsize len;
    const char *data = g_bytes_get_data(bytes, &len);
    g_file_set_contents(dest_path, data, len, NULL);
    g_bytes_unref(bytes);
}

static void ensure_config_dir(void) {
    char *config_dir = g_build_filename(
        g_get_home_dir(), ".config", "cliphist-picker", NULL);
    char *themes_dir = g_build_filename(config_dir, "themes", NULL);
    g_mkdir_with_parents(themes_dir, 0755);

    /* Copy bundled defaults if not present */
    char *theme_conf = g_build_filename(config_dir, "theme.conf", NULL);
    copy_resource_to_file(
        "/com/github/cliphist-picker/theme.conf", theme_conf);

    char *mocha_conf = g_build_filename(themes_dir, "catppuccin-mocha.conf", NULL);
    copy_resource_to_file(
        "/com/github/cliphist-picker/themes/catppuccin-mocha.conf", mocha_conf);

    char *mocha_css = g_build_filename(themes_dir, "catppuccin-mocha.css", NULL);
    copy_resource_to_file(
        "/com/github/cliphist-picker/themes/catppuccin-mocha.css", mocha_css);

    g_free(mocha_css);
    g_free(mocha_conf);
    g_free(theme_conf);
    g_free(themes_dir);
    g_free(config_dir);
}

/* ── Resolve CSS path from config chain ────────────────────────── */

static char *resolve_css_path(void) {
    char *config_dir = g_build_filename(
        g_get_home_dir(), ".config", "cliphist-picker", NULL);
    char *theme_conf = g_build_filename(config_dir, "theme.conf", NULL);

    /* theme.conf → source = themes/something.conf */
    char *source = conf_read_key(theme_conf, "source");
    g_free(theme_conf);
    if (!source) {
        g_free(config_dir);
        return NULL;
    }

    /* Resolve relative to config dir */
    char *theme_path = g_build_filename(config_dir, source, NULL);
    g_free(source);

    /* theme file → css = something.css */
    char *css_name = conf_read_key(theme_path, "css");
    if (!css_name) {
        g_free(theme_path);
        g_free(config_dir);
        return NULL;
    }

    /* Resolve CSS relative to the theme conf's directory */
    char *theme_dir = g_path_get_dirname(theme_path);
    char *css_path = g_build_filename(theme_dir, css_name, NULL);

    g_free(theme_dir);
    g_free(css_name);
    g_free(theme_path);
    g_free(config_dir);
    return css_path;
}

/* ── Startup ───────────────────────────────────────────────────── */

static void on_startup(GtkApplication *app, gpointer user_data) {
    (void)app; (void)user_data;

    g_config = config_defaults();
    ensure_config_dir();

    /* Resolve CSS through config chain */
    GtkCssProvider *provider = gtk_css_provider_new();
    char *css_path = resolve_css_path();

    if (css_path && g_file_test(css_path, G_FILE_TEST_EXISTS)) {
        gtk_css_provider_load_from_path(provider, css_path);
    } else {
        /* Fallback to embedded resource */
        gtk_css_provider_load_from_resource(provider,
            "/com/github/cliphist-picker/themes/catppuccin-mocha.css");
    }
    g_free(css_path);

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    /* Create window shell once */
    if (!g_picker)
        g_picker = picker_window_new(app, &g_config);

    /* Fetch fresh clipboard entries */
    int raw_count = 0;
    RawEntry *raw = clipboard_list(&raw_count);
    if (!raw || raw_count == 0) {
        g_free(raw);
        return; /* daemon stays alive */
    }

    /* Deduplicate */
    bool multi_type = clipboard_current_has_text_and_image();
    int entry_count = 0;
    ClipboardEntry *entries = deduplicate(
        raw, raw_count,
        g_config.max_entries,
        multi_type,
        &entry_count);

    /* Free raw entries */
    for (int i = 0; i < raw_count; i++)
        raw_entry_free(&raw[i]);
    g_free(raw);

    if (entry_count == 0) {
        g_free(entries);
        return; /* daemon stays alive */
    }

    /* Refresh and show */
    picker_window_refresh(g_picker, entries, entry_count);
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new(
        "com.github.cliphist-picker",
        G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "startup",  G_CALLBACK(on_startup),  NULL);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
