#ifndef WINDOW_H
#define WINDOW_H

#include <gtk/gtk.h>

#include "config.h"
#include "models.h"

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
} PickerWindow;

typedef struct {
    guint8 *data;
    gsize   len;
} ImageData;

PickerWindow *picker_window_new(GtkApplication *app,
                                ClipboardEntry *entries, int n_entries,
                                const Config *config);

#endif
