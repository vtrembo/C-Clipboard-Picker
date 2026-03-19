#ifndef ROW_H
#define ROW_H

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "config.h"
#include "models.h"

GtkListBoxRow *create_row(const ClipboardEntry *entry, const Config *config);
GdkTexture    *load_thumbnail_from_bytes(const guint8 *data, gsize len, int target_size);
GdkTexture    *load_preview_from_bytes(const guint8 *data, gsize len,
                                       int max_width, int max_height);

#endif
