#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include "models.h"
#include <glib.h>

RawEntry *clipboard_list(int *out_count);
guint8   *clipboard_decode(const char *raw_line, gsize *out_len);
void      clipboard_copy_text(const char *raw_line);
void      clipboard_copy_image(const char *raw_line, const char *mime);
void      clipboard_paste_wtype(void);

#endif
