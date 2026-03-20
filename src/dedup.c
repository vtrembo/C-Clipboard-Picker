#include "dedup.h"
#include "clipboard.h"

#include <stdlib.h>
#include <string.h>

ClipboardEntry *deduplicate(const RawEntry *raw, int raw_count,
                            int max_entries,
                            bool current_clipboard_multi_type,
                            int *out_count) {

    int limit = raw_count < max_entries ? raw_count : max_entries;
    ClipboardEntry *result = g_new0(ClipboardEntry, limit);
    int count = 0;

    /*
     * If the current clipboard holds both text and image MIME types,
     * the most recent copy was a multi-type event (e.g. right-click
     * copy from a browser).  Find the text entry paired with the
     * image at the top and permanently delete it from cliphist.
     */
    int skip_id = -1;
    if (current_clipboard_multi_type && raw_count >= 2) {
        const RawEntry *a = &raw[0];
        const RawEntry *b = &raw[1];
        if (a->is_binary != b->is_binary) {
            const RawEntry *txt = a->is_binary ? b : a;
            skip_id = txt->id;
            clipboard_delete(txt->raw_line);
        }
    }

    for (int i = 0; i < raw_count && count < max_entries; i++) {
        const RawEntry *entry = &raw[i];

        if (entry->id == skip_id)
            continue;

        ClipboardEntry *ce = &result[count];
        if (entry->is_binary) {
            ce->entry_type      = ENTRY_TYPE_IMAGE;
            ce->text_id         = -1;
            ce->image_id        = entry->id;
            ce->image_size      = g_strdup(entry->binary_size);
            ce->image_format    = g_strdup(entry->binary_format);
            ce->image_dims      = g_strdup(entry->binary_dims);
            ce->primary_raw_line = g_strdup(entry->raw_line);
            ce->image_raw_line  = g_strdup(entry->raw_line);
        } else {
            ce->entry_type      = ENTRY_TYPE_TEXT;
            ce->text_id         = entry->id;
            ce->text_preview    = g_strdup(entry->preview);
            ce->image_id        = -1;
            ce->primary_raw_line = g_strdup(entry->raw_line);
        }
        count++;
    }

    *out_count = count;
    return result;
}
