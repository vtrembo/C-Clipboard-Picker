#include "clipboard.h"

#include <gio/gio.h>
#include <string.h>

static GRegex *binary_re = NULL;

static void ensure_binary_regex(void) {
    if (!binary_re) {
        binary_re = g_regex_new(
            "^\\[\\[ binary data ([\\d.]+\\s+\\w+)\\s+(\\w+)\\s+(\\d+x\\d+) \\]\\]$",
            0, 0, NULL);
    }
}

RawEntry *clipboard_list(int *out_count) {
    ensure_binary_regex();
    *out_count = 0;

    GError *err = NULL;
    GSubprocess *proc = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE,
        &err, "cliphist", "list", NULL);
    if (!proc) {
        g_clear_error(&err);
        return NULL;
    }

    GBytes *out_bytes = NULL;
    g_subprocess_communicate(proc, NULL, NULL, &out_bytes, NULL, &err);
    g_object_unref(proc);
    if (!out_bytes) {
        g_clear_error(&err);
        return NULL;
    }

    gsize buf_len;
    const char *buf = g_bytes_get_data(out_bytes, &buf_len);

    /* Count lines for initial allocation */
    int capacity = 1;
    for (gsize i = 0; i < buf_len; i++) {
        if (buf[i] == '\n') capacity++;
    }

    RawEntry *entries = g_new0(RawEntry, capacity);
    int count = 0;

    /* Parse line by line */
    const char *p = buf;
    const char *end = buf + buf_len;

    while (p < end) {
        const char *nl = memchr(p, '\n', end - p);
        if (!nl) nl = end;

        gsize line_len = nl - p;
        if (line_len == 0) {
            p = nl + 1;
            continue;
        }

        /* Find tab separator */
        const char *tab = memchr(p, '\t', line_len);
        if (!tab) {
            p = nl + 1;
            continue;
        }

        /* Parse ID */
        char id_buf[32];
        gsize id_len = tab - p;
        if (id_len >= sizeof(id_buf)) {
            p = nl + 1;
            continue;
        }
        memcpy(id_buf, p, id_len);
        id_buf[id_len] = '\0';
        g_strstrip(id_buf);

        char *endptr;
        long id = strtol(id_buf, &endptr, 10);
        if (*endptr != '\0') {
            p = nl + 1;
            continue;
        }

        /* Extract preview and raw_line */
        const char *preview_start = tab + 1;
        gsize preview_len = nl - preview_start;

        RawEntry *e = &entries[count];
        e->id = (int)id;
        e->preview = g_strndup(preview_start, preview_len);
        e->raw_line = g_strndup(p, line_len);

        /* Check for binary data pattern */
        char *trimmed = g_strstrip(g_strndup(preview_start, preview_len));
        GMatchInfo *match_info = NULL;
        if (g_regex_match(binary_re, trimmed, 0, &match_info)) {
            e->is_binary = true;
            e->binary_size   = g_match_info_fetch(match_info, 1);
            e->binary_format = g_match_info_fetch(match_info, 2);
            e->binary_dims   = g_match_info_fetch(match_info, 3);
        } else {
            e->is_binary = false;
        }
        g_match_info_free(match_info);
        g_free(trimmed);

        count++;
        p = nl + 1;
    }

    g_bytes_unref(out_bytes);
    *out_count = count;
    return entries;
}

guint8 *clipboard_decode(const char *raw_line, gsize *out_len) {
    *out_len = 0;

    GError *err = NULL;
    GSubprocess *proc = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE,
        &err, "cliphist", "decode", NULL);
    if (!proc) {
        g_clear_error(&err);
        return NULL;
    }

    GBytes *input = g_bytes_new(raw_line, strlen(raw_line));
    GBytes *output = NULL;
    g_subprocess_communicate(proc, input, NULL, &output, NULL, &err);
    g_bytes_unref(input);
    g_object_unref(proc);

    if (err || !output) {
        g_clear_error(&err);
        if (output) g_bytes_unref(output);
        return NULL;
    }

    gsize size;
    const guint8 *data = g_bytes_get_data(output, &size);
    guint8 *result = g_memdup2(data, size);
    *out_len = size;

    g_bytes_unref(output);
    return result;
}

void clipboard_copy_text(const char *raw_line) {
    gsize len = 0;
    guint8 *decoded = clipboard_decode(raw_line, &len);
    if (!decoded) return;

    GError *err = NULL;
    GSubprocess *proc = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDIN_PIPE,
        &err, "wl-copy", NULL);
    if (!proc) {
        g_clear_error(&err);
        g_free(decoded);
        return;
    }

    GBytes *input = g_bytes_new_take(decoded, len);
    g_subprocess_communicate(proc, input, NULL, NULL, NULL, NULL);
    g_bytes_unref(input);
    g_object_unref(proc);
}

void clipboard_copy_image(const char *raw_line, const char *mime) {
    gsize len = 0;
    guint8 *decoded = clipboard_decode(raw_line, &len);
    if (!decoded) return;

    GError *err = NULL;
    GSubprocess *proc = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDIN_PIPE,
        &err, "wl-copy", "--type", mime, NULL);
    if (!proc) {
        g_clear_error(&err);
        g_free(decoded);
        return;
    }

    GBytes *input = g_bytes_new_take(decoded, len);
    g_subprocess_communicate(proc, input, NULL, NULL, NULL, NULL);
    g_bytes_unref(input);
    g_object_unref(proc);
}

void clipboard_paste_wtype(void) {
    GError *err = NULL;
    GSubprocess *proc = g_subprocess_new(
        G_SUBPROCESS_FLAGS_NONE, &err,
        "wtype", "-M", "ctrl", "v", "-m", "ctrl", NULL);
    if (proc) g_object_unref(proc);
    g_clear_error(&err);
}
