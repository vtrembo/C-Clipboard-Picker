#ifndef WINDOW_H
#define WINDOW_H

#include <gtk/gtk.h>

#include "config.h"
#include "models.h"

typedef struct {
    guint8 *data;
    gsize   len;
} ImageData;

typedef struct {
    GtkWindow         *window;
    GtkApplication    *app;
    Config             config;

    ClipboardEntry    *entries;
    int                n_entries;

    GtkBox            *picker_frame;
    GtkScrolledWindow *scrolled;
    GtkListBox        *listbox;
    GtkPicture        *preview_picture;

    GHashTable        *image_cache;   /* char* -> ImageData* */
    GMutex             cache_mutex;
    GtkListBoxRow     *hovered_row;

    guint              generation;       /* incremented on each refresh */
    guint              paste_timeout_id; /* pending auto-paste timer, 0 if none */
} PickerWindow;

/* Create the window shell (layer shell, UI, input). No entries yet. */
PickerWindow *picker_window_new(GtkApplication *app, const Config *config);

/* Clear old data, assign new entries, rebuild listbox, and present the window. */
void picker_window_refresh(PickerWindow *win,
                           ClipboardEntry *entries, int n_entries);

#endif
