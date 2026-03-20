#ifndef DEDUP_H
#define DEDUP_H

#include "models.h"

ClipboardEntry *deduplicate(const RawEntry *raw, int raw_count,
                            int max_entries,
                            bool current_clipboard_multi_type,
                            int *out_count);

#endif
