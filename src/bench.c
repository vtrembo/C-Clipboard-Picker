/* Benchmark: cliphist list + parse + dedup (no GUI). */
#include <stdio.h>
#include <time.h>

#include "clipboard.h"
#include "config.h"
#include "dedup.h"
#include "models.h"

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(void) {
    double t0 = now_ms();

    Config config = config_defaults();

    double t_init = now_ms();

    int raw_count = 0;
    RawEntry *raw = clipboard_list(&raw_count);

    double t_list = now_ms();

    int entry_count = 0;
    ClipboardEntry *entries = NULL;
    if (raw && raw_count > 0) {
        entries = deduplicate(raw, raw_count,
                              config.dedup_id_threshold,
                              config.max_entries,
                              &entry_count);
    }

    double t_dedup = now_ms();

    printf("C startup benchmark:\n");
    printf("  init:           %.1f ms\n", t_init - t0);
    printf("  cliphist list:  %.1f ms\n", t_list - t_init);
    printf("  dedup:          %.1f ms\n", t_dedup - t_list);
    printf("  total:          %.1f ms\n", t_dedup - t0);
    printf("  entries: %d raw -> %d deduped\n", raw_count, entry_count);

    /* Cleanup */
    if (raw) {
        for (int i = 0; i < raw_count; i++)
            raw_entry_free(&raw[i]);
        g_free(raw);
    }
    if (entries) {
        for (int i = 0; i < entry_count; i++)
            clipboard_entry_free(&entries[i]);
        g_free(entries);
    }

    return 0;
}
