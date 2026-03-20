#ifndef MODELS_H
#define MODELS_H

#include <glib.h>
#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    ENTRY_TYPE_TEXT,
    ENTRY_TYPE_IMAGE,
} EntryType;

typedef struct {
    int      id;
    char    *preview;
    char    *raw_line;
    bool     is_binary;
    char    *binary_size;
    char    *binary_format;
    char    *binary_dims;
} RawEntry;

typedef struct {
    EntryType  entry_type;
    int        text_id;
    char      *text_preview;
    int        image_id;
    char      *image_size;
    char      *image_format;
    char      *image_dims;
    char      *primary_raw_line;
    char      *image_raw_line;
} ClipboardEntry;

static inline bool clipboard_entry_has_image(const ClipboardEntry *e) {
    return e->entry_type == ENTRY_TYPE_IMAGE;
}

static inline void raw_entry_free(RawEntry *e) {
    g_free(e->preview);
    g_free(e->raw_line);
    g_free(e->binary_size);
    g_free(e->binary_format);
    g_free(e->binary_dims);
}

static inline void clipboard_entry_free(ClipboardEntry *e) {
    g_free(e->text_preview);
    g_free(e->image_size);
    g_free(e->image_format);
    g_free(e->image_dims);
    g_free(e->primary_raw_line);
    g_free(e->image_raw_line);
}

#endif
