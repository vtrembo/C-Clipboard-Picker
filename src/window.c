#include "window.h"

#include <gtk4-layer-shell.h>
#include <string.h>

#include "clipboard.h"
#include "row.h"

/* ── Forward declarations ──────────────────────────────────────── */

static void     build_ui(PickerWindow *win);
static void     setup_input(PickerWindow *win);
static void     populate(PickerWindow *win);
static void     load_thumbnails_async(PickerWindow *win);
static void     dismiss(PickerWindow *win);
static gboolean render_preview_idle(gpointer user_data);

/* ── Image cache helpers ───────────────────────────────────────── */

static ImageData *image_data_new(guint8 *data, gsize len) {
    ImageData *img = g_new(ImageData, 1);
    img->data = data;
    img->len  = len;
    return img;
}

static void image_data_free(gpointer p) {
    ImageData *img = p;
    g_free(img->data);
    g_free(img);
}

/* ── Thumbnail thread data ─────────────────────────────────────── */

typedef struct {
    PickerWindow *win;
    int           row_index;
    guint8       *data;
    gsize         len;
} ThumbnailJob;

static gboolean create_and_set_thumbnail_idle(gpointer user_data) {
    ThumbnailJob *job = user_data;
    PickerWindow *win = job->win;

    GdkTexture *texture = load_thumbnail_from_bytes(
        job->data, job->len, win->config.thumbnail_size);

    if (texture) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(
            win->listbox, job->row_index);
        if (row) {
            GtkPicture *thumb = GTK_PICTURE(
                g_object_get_data(G_OBJECT(row), "thumbnail-widget"));
            if (thumb)
                gtk_picture_set_paintable(thumb, GDK_PAINTABLE(texture));
        }
        g_object_unref(texture);
    }

    g_free(job);
    return G_SOURCE_REMOVE;
}

static gpointer thumbnail_thread_func(gpointer data) {
    PickerWindow *win = data;

    for (int i = 0; i < win->n_entries; i++) {
        ClipboardEntry *entry = &win->entries[i];
        if (!clipboard_entry_has_image(entry) || !entry->image_raw_line)
            continue;

        g_mutex_lock(&win->cache_mutex);
        gboolean cached = g_hash_table_contains(win->image_cache,
                                                 entry->image_raw_line);
        g_mutex_unlock(&win->cache_mutex);

        ImageData *img;
        if (!cached) {
            gsize len = 0;
            guint8 *raw = clipboard_decode(entry->image_raw_line, &len);
            if (!raw || len == 0) {
                g_free(raw);
                continue;
            }
            img = image_data_new(raw, len);
            g_mutex_lock(&win->cache_mutex);
            g_hash_table_insert(win->image_cache,
                                g_strdup(entry->image_raw_line), img);
            g_mutex_unlock(&win->cache_mutex);
        } else {
            g_mutex_lock(&win->cache_mutex);
            img = g_hash_table_lookup(win->image_cache, entry->image_raw_line);
            g_mutex_unlock(&win->cache_mutex);
            if (!img) continue;
        }

        ThumbnailJob *job = g_new0(ThumbnailJob, 1);
        job->win       = win;
        job->row_index = i;
        job->data      = img->data;
        job->len       = img->len;
        g_idle_add(create_and_set_thumbnail_idle, job);
    }

    return NULL;
}

static void load_thumbnails_async(PickerWindow *win) {
    g_thread_new("thumbnail-loader", thumbnail_thread_func, win);
}

/* ── Preview ───────────────────────────────────────────────────── */

static void render_preview(PickerWindow *win, const guint8 *data, gsize len) {
    GdkTexture *texture = load_preview_from_bytes(
        data, len, win->config.preview_width, win->config.preview_max_height);
    if (texture) {
        gtk_picture_set_paintable(win->preview_picture, GDK_PAINTABLE(texture));
        gtk_widget_set_visible(GTK_WIDGET(win->preview_picture), TRUE);
        g_object_unref(texture);
    } else {
        gtk_widget_set_visible(GTK_WIDGET(win->preview_picture), FALSE);
    }
}

typedef struct {
    PickerWindow *win;
    char         *raw_line;
} PreviewDecodeJob;

static gpointer preview_decode_thread(gpointer data) {
    PreviewDecodeJob *job = data;
    PickerWindow *win = job->win;

    gsize len = 0;
    guint8 *decoded = clipboard_decode(job->raw_line, &len);

    if (decoded && len > 0) {
        ImageData *img = image_data_new(decoded, len);
        g_mutex_lock(&win->cache_mutex);
        g_hash_table_insert(win->image_cache,
                            job->raw_line, img);
        g_mutex_unlock(&win->cache_mutex);
        job->raw_line = NULL; /* ownership transferred to cache */

        ThumbnailJob *tj = g_new0(ThumbnailJob, 1);
        tj->win  = win;
        tj->data = img->data;
        tj->len  = img->len;
        tj->row_index = -1; /* signal: this is a preview, not thumbnail */
        g_idle_add(render_preview_idle, tj);
    }

    g_free(job->raw_line);
    g_free(job);
    return NULL;
}

static gboolean render_preview_idle(gpointer user_data) {
    ThumbnailJob *job = user_data;
    render_preview(job->win, job->data, job->len);
    g_free(job);
    return G_SOURCE_REMOVE;
}

static void show_preview_for(PickerWindow *win, ClipboardEntry *entry) {
    const char *raw_line = entry->image_raw_line;
    if (!raw_line) return;

    g_mutex_lock(&win->cache_mutex);
    ImageData *img = g_hash_table_lookup(win->image_cache, raw_line);
    g_mutex_unlock(&win->cache_mutex);

    if (img) {
        render_preview(win, img->data, img->len);
    } else {
        PreviewDecodeJob *job = g_new0(PreviewDecodeJob, 1);
        job->win      = win;
        job->raw_line = g_strdup(raw_line);
        g_thread_new("preview-decode", preview_decode_thread, job);
    }
}

/* ── Keyboard ──────────────────────────────────────────────────── */

static void move_selection(PickerWindow *win, int delta) {
    GtkListBoxRow *selected = gtk_list_box_get_selected_row(win->listbox);
    int target_idx;
    if (!selected) {
        target_idx = 0;
    } else {
        target_idx = gtk_list_box_row_get_index(selected) + delta;
    }

    if (target_idx < 0) target_idx = 0;
    if (target_idx >= win->n_entries) target_idx = win->n_entries - 1;

    GtkListBoxRow *target = gtk_list_box_get_row_at_index(
        win->listbox, target_idx);
    if (target) {
        gtk_list_box_select_row(win->listbox, target);
        gtk_widget_grab_focus(GTK_WIDGET(target));
    }
}

static void activate_entry(PickerWindow *win, ClipboardEntry *entry);

static gboolean on_key_pressed(GtkEventControllerKey *ctrl,
                                guint keyval, guint keycode,
                                GdkModifierType state,
                                gpointer user_data) {
    (void)ctrl; (void)keycode; (void)state;
    PickerWindow *win = user_data;
    const char *name = gdk_keyval_name(keyval);

    if (g_strcmp0(name, "Escape") == 0) {
        dismiss(win);
        return TRUE;
    }

    if (g_strcmp0(name, "Return") == 0 || g_strcmp0(name, "KP_Enter") == 0) {
        GtkListBoxRow *row = gtk_list_box_get_selected_row(win->listbox);
        if (row) {
            ClipboardEntry *entry = g_object_get_data(
                G_OBJECT(row), "clipboard-entry");
            if (entry) activate_entry(win, entry);
        }
        return TRUE;
    }

    if (g_strcmp0(name, "Up") == 0 ||
        g_strcmp0(name, "w") == 0 ||
        g_strcmp0(name, "W") == 0) {
        move_selection(win, -1);
        return TRUE;
    }

    if (g_strcmp0(name, "Down") == 0 ||
        g_strcmp0(name, "s") == 0 ||
        g_strcmp0(name, "S") == 0) {
        move_selection(win, 1);
        return TRUE;
    }

    return FALSE;
}

/* ── Selection / Hover ─────────────────────────────────────────── */

static void on_selection_changed(GtkListBox *listbox, gpointer user_data) {
    PickerWindow *win = user_data;
    GtkListBoxRow *selected = gtk_list_box_get_selected_row(listbox);

    if (!selected) {
        gtk_widget_set_visible(GTK_WIDGET(win->preview_picture), FALSE);
        return;
    }

    ClipboardEntry *entry = g_object_get_data(
        G_OBJECT(selected), "clipboard-entry");
    if (entry && clipboard_entry_has_image(entry) && entry->image_raw_line) {
        show_preview_for(win, entry);
    } else {
        gtk_widget_set_visible(GTK_WIDGET(win->preview_picture), FALSE);
    }
}

static void on_listbox_motion(GtkEventControllerMotion *ctrl,
                               double x, double y,
                               gpointer user_data) {
    (void)ctrl; (void)x;
    PickerWindow *win = user_data;
    GtkListBoxRow *row = gtk_list_box_get_row_at_y(win->listbox, y);

    if (row == win->hovered_row) return;
    win->hovered_row = row;

    if (!row) return;

    ClipboardEntry *entry = g_object_get_data(
        G_OBJECT(row), "clipboard-entry");
    if (entry && clipboard_entry_has_image(entry) && entry->image_raw_line) {
        show_preview_for(win, entry);
    } else {
        GtkListBoxRow *selected = gtk_list_box_get_selected_row(win->listbox);
        if (!selected) {
            gtk_widget_set_visible(GTK_WIDGET(win->preview_picture), FALSE);
        } else {
            ClipboardEntry *sel_entry = g_object_get_data(
                G_OBJECT(selected), "clipboard-entry");
            if (!sel_entry || !clipboard_entry_has_image(sel_entry))
                gtk_widget_set_visible(GTK_WIDGET(win->preview_picture), FALSE);
        }
    }
}

static void on_listbox_leave(GtkEventControllerMotion *ctrl,
                              gpointer user_data) {
    (void)ctrl;
    PickerWindow *win = user_data;
    win->hovered_row = NULL;

    GtkListBoxRow *selected = gtk_list_box_get_selected_row(win->listbox);
    if (selected) {
        ClipboardEntry *entry = g_object_get_data(
            G_OBJECT(selected), "clipboard-entry");
        if (entry && clipboard_entry_has_image(entry) && entry->image_raw_line) {
            show_preview_for(win, entry);
            return;
        }
    }
    gtk_widget_set_visible(GTK_WIDGET(win->preview_picture), FALSE);
}

/* ── Activation ────────────────────────────────────────────────── */

static gboolean do_paste(gpointer user_data) {
    PickerWindow *win = user_data;
    clipboard_paste_wtype();
    dismiss(win);
    return G_SOURCE_REMOVE;
}

static void activate_entry(PickerWindow *win, ClipboardEntry *entry) {
    if (entry->entry_type == ENTRY_TYPE_IMAGE) {
        char *mime = g_strdup_printf("image/%s",
            entry->image_format ? entry->image_format : "png");
        clipboard_copy_image(entry->primary_raw_line, mime);
        g_free(mime);
    } else {
        clipboard_copy_text(entry->primary_raw_line);
    }

    if (win->config.close_on_select) {
        gtk_widget_set_visible(GTK_WIDGET(win->window), FALSE);
    }

    if (win->config.auto_paste) {
        g_timeout_add(win->config.paste_delay_ms, do_paste, win);
    } else {
        dismiss(win);
    }
}

static void on_row_activated(GtkListBox *listbox, GtkListBoxRow *row,
                              gpointer user_data) {
    (void)listbox;
    PickerWindow *win = user_data;
    ClipboardEntry *entry = g_object_get_data(
        G_OBJECT(row), "clipboard-entry");
    if (entry) activate_entry(win, entry);
}

/* ── Backdrop Click ────────────────────────────────────────────── */

static gboolean is_inside_picker(PickerWindow *win, GtkWidget *widget) {
    while (widget) {
        if (widget == GTK_WIDGET(win->picker_frame) ||
            widget == GTK_WIDGET(win->preview_picture))
            return TRUE;
        widget = gtk_widget_get_parent(widget);
    }
    return FALSE;
}

static void on_backdrop_click(GtkGestureClick *gesture,
                               int n_press, double x, double y,
                               gpointer user_data) {
    (void)gesture; (void)n_press;
    PickerWindow *win = user_data;
    GtkWidget *target = gtk_widget_pick(
        GTK_WIDGET(win->window), x, y, GTK_PICK_DEFAULT);

    if (!target || !is_inside_picker(win, target))
        dismiss(win);
}

/* ── Realize ───────────────────────────────────────────────────── */

static void on_realize(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    PickerWindow *win = user_data;
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(win->window));
    if (!surface) return;

    GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(win->window));
    GdkMonitor *monitor = gdk_display_get_monitor_at_surface(display, surface);
    if (!monitor) return;

    GdkRectangle geom;
    gdk_monitor_get_geometry(monitor, &geom);
    int screen_w = geom.width;
    int gap = (screen_w - win->config.width) / 2;
    int margin = MAX(20, gap - win->config.preview_width - 24);
    gtk_widget_set_margin_end(GTK_WIDGET(win->preview_picture), margin);
}

/* ── Dismiss ───────────────────────────────────────────────────── */

static void dismiss(PickerWindow *win) {
    gtk_window_close(win->window);
    g_application_quit(G_APPLICATION(win->app));
}

/* ── Build UI ──────────────────────────────────────────────────── */

static void build_ui(PickerWindow *win) {
    GtkOverlay *overlay = GTK_OVERLAY(gtk_overlay_new());

    /* Full-screen backdrop for input region */
    GtkBox *backdrop = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_hexpand(GTK_WIDGET(backdrop), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(backdrop), TRUE);
    gtk_overlay_set_child(overlay, GTK_WIDGET(backdrop));

    /* Picker frame */
    win->picker_frame = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_add_css_class(GTK_WIDGET(win->picker_frame), "picker-frame");
    gtk_widget_set_size_request(GTK_WIDGET(win->picker_frame),
                                 win->config.width, -1);
    gtk_widget_set_halign(GTK_WIDGET(win->picker_frame), GTK_ALIGN_CENTER);
    gtk_widget_set_valign(GTK_WIDGET(win->picker_frame), GTK_ALIGN_CENTER);

    /* Scrolled window */
    win->scrolled = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_scrolled_window_set_policy(win->scrolled,
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_max_content_height(win->scrolled,
                                                 win->config.max_height);
    gtk_scrolled_window_set_propagate_natural_height(win->scrolled, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(win->scrolled), "picker-scroll");

    /* List box */
    win->listbox = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(win->listbox, GTK_SELECTION_SINGLE);
    gtk_widget_add_css_class(GTK_WIDGET(win->listbox), "picker-list");
    gtk_scrolled_window_set_child(win->scrolled, GTK_WIDGET(win->listbox));
    gtk_box_append(win->picker_frame, GTK_WIDGET(win->scrolled));
    gtk_overlay_add_overlay(overlay, GTK_WIDGET(win->picker_frame));

    /* Preview picture */
    win->preview_picture = GTK_PICTURE(gtk_picture_new());
    gtk_picture_set_can_shrink(win->preview_picture, TRUE);
    gtk_picture_set_content_fit(win->preview_picture, GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_add_css_class(GTK_WIDGET(win->preview_picture), "preview-image");
    gtk_widget_set_size_request(GTK_WIDGET(win->preview_picture),
                                 win->config.preview_width, -1);
    gtk_widget_set_halign(GTK_WIDGET(win->preview_picture), GTK_ALIGN_END);
    gtk_widget_set_valign(GTK_WIDGET(win->preview_picture), GTK_ALIGN_CENTER);
    gtk_widget_set_margin_end(GTK_WIDGET(win->preview_picture),
        MAX(20, (1920 - win->config.width) / 2 - win->config.preview_width - 40));
    gtk_widget_set_visible(GTK_WIDGET(win->preview_picture), FALSE);
    gtk_overlay_add_overlay(overlay, GTK_WIDGET(win->preview_picture));

    gtk_window_set_child(win->window, GTK_WIDGET(overlay));
}

/* ── Input setup ───────────────────────────────────────────────── */

static void setup_input(PickerWindow *win) {
    /* Keyboard (capture phase) */
    GtkEventControllerKey *key_ctrl = GTK_EVENT_CONTROLLER_KEY(
        gtk_event_controller_key_new());
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(key_ctrl), GTK_PHASE_CAPTURE);
    g_signal_connect(key_ctrl, "key-pressed",
        G_CALLBACK(on_key_pressed), win);
    gtk_widget_add_controller(GTK_WIDGET(win->window),
        GTK_EVENT_CONTROLLER(key_ctrl));

    /* Row activation and selection */
    g_signal_connect(win->listbox, "row-activated",
        G_CALLBACK(on_row_activated), win);
    g_signal_connect(win->listbox, "selected-rows-changed",
        G_CALLBACK(on_selection_changed), win);

    /* Backdrop click */
    GtkGestureClick *click = GTK_GESTURE_CLICK(gtk_gesture_click_new());
    g_signal_connect(click, "pressed",
        G_CALLBACK(on_backdrop_click), win);
    gtk_widget_add_controller(GTK_WIDGET(win->window),
        GTK_EVENT_CONTROLLER(click));

    /* Hover for preview */
    GtkEventControllerMotion *motion = GTK_EVENT_CONTROLLER_MOTION(
        gtk_event_controller_motion_new());
    g_signal_connect(motion, "motion",
        G_CALLBACK(on_listbox_motion), win);
    g_signal_connect(motion, "leave",
        G_CALLBACK(on_listbox_leave), win);
    gtk_widget_add_controller(GTK_WIDGET(win->listbox),
        GTK_EVENT_CONTROLLER(motion));
}

/* ── Populate ──────────────────────────────────────────────────── */

static void populate(PickerWindow *win) {
    for (int i = 0; i < win->n_entries; i++) {
        GtkListBoxRow *row = create_row(&win->entries[i], &win->config);
        gtk_list_box_append(win->listbox, GTK_WIDGET(row));
    }

    GtkListBoxRow *first = gtk_list_box_get_row_at_index(win->listbox, 0);
    if (first) {
        gtk_list_box_select_row(win->listbox, first);
        gtk_widget_grab_focus(GTK_WIDGET(first));
    }
}

/* ── Public ────────────────────────────────────────────────────── */

PickerWindow *picker_window_new(GtkApplication *app,
                                ClipboardEntry *entries, int n_entries,
                                const Config *config) {
    PickerWindow *win = g_new0(PickerWindow, 1);
    win->app       = app;
    win->config    = *config;
    win->entries   = entries;
    win->n_entries = n_entries;

    g_mutex_init(&win->cache_mutex);
    win->image_cache = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, image_data_free);

    /* Create window */
    win->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_application(win->window, app);

    /* Layer Shell setup */
    gtk_layer_init_for_window(win->window);
    gtk_layer_set_layer(win->window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_keyboard_mode(win->window,
        GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    gtk_layer_set_namespace(win->window, "cliphist-picker");
    gtk_layer_set_exclusive_zone(win->window, -1);

    gtk_layer_set_anchor(win->window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(win->window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(win->window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(win->window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    gtk_widget_remove_css_class(GTK_WIDGET(win->window), "background");
    gtk_widget_add_css_class(GTK_WIDGET(win->window), "backdrop");

    build_ui(win);
    setup_input(win);
    populate(win);
    load_thumbnails_async(win);

    g_signal_connect(win->window, "realize", G_CALLBACK(on_realize), win);

    return win;
}
