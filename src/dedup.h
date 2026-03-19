#ifndef DEDUP_H
#define DEDUP_H

#include "models.h"

ClipboardEntry *deduplicate(const RawEntry *raw, int raw_count,
                            int id_threshold, int max_entries,
                            int *out_count);

#endif
