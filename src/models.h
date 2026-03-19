#ifndef MODELS_H
#define MODELS_H

#include <glib.h>
#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    ENTRY_TYPE_TEXT,
    ENTRY_TYPE_IMAGE,
    ENTRY_TYPE_MERGED,
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
    return e->entry_type == ENTRY_TYPE_IMAGE ||
           e->entry_type == ENTRY_TYPE_MERGED;
}

static inline char *clipboard_entry_display_text(const ClipboardEntry *e) {
    if (e->text_preview)
        return g_strdup(e->text_preview);
    if (e->image_format && e->image_dims && e->image_size) {
        char *upper_fmt = g_ascii_strup(e->image_format, -1);
        char *result = g_strdup_printf("[%s %s - %s]",
            upper_fmt, e->image_dims, e->image_size);
        g_free(upper_fmt);
        return result;
    }
    return g_strdup("[Image]");
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
