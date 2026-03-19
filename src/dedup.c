#include "dedup.h"

#include <stdlib.h>
#include <string.h>

ClipboardEntry *deduplicate(const RawEntry *raw, int raw_count,
                            int id_threshold, int max_entries,
                            int *out_count) {
    ClipboardEntry *result = g_new0(ClipboardEntry, max_entries);
    bool *consumed = g_new0(bool, raw_count);
    int count = 0;
    int i = 0;

    while (i < raw_count && count < max_entries) {
        if (consumed[i]) {
            i++;
            continue;
        }

        const RawEntry *entry = &raw[i];

        /* Find next unconsumed neighbor */
        int j = i + 1;
        while (j < raw_count && consumed[j])
            j++;

        bool merged = false;
        if (j < raw_count) {
            const RawEntry *neighbor = &raw[j];
            int id_gap = abs(entry->id - neighbor->id);

            if (id_gap <= id_threshold && entry->is_binary != neighbor->is_binary) {
                const RawEntry *img = entry->is_binary ? entry : neighbor;
                const RawEntry *txt = entry->is_binary ? neighbor : entry;

                ClipboardEntry *ce = &result[count];
                ce->entry_type      = ENTRY_TYPE_MERGED;
                ce->text_id         = txt->id;
                ce->text_preview    = g_strdup(txt->preview);
                ce->image_id        = img->id;
                ce->image_size      = g_strdup(img->binary_size);
                ce->image_format    = g_strdup(img->binary_format);
                ce->image_dims      = g_strdup(img->binary_dims);
                ce->primary_raw_line = g_strdup(txt->raw_line);
                ce->image_raw_line  = g_strdup(img->raw_line);

                consumed[i] = true;
                consumed[j] = true;
                merged = true;
                count++;
            }
        }

        if (!merged) {
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
            consumed[i] = true;
            count++;
        }

        i++;
    }

    g_free(consumed);
    *out_count = count;
    return result;
}
