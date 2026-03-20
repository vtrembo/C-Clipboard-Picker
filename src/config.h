#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

typedef struct {
    /* general */
    int  max_entries;
    /* appearance */
    int  width;
    int  max_height;
    int  row_height_text;
    int  row_height_image;
    int  thumbnail_size;
    int  preview_width;
    int  preview_max_height;
    bool dim_backdrop;
    /* behavior */
    bool auto_paste;
    int  paste_delay_ms;
    bool close_on_select;
} Config;

static inline Config config_defaults(void) {
    return (Config){
        .max_entries         = 50,
        .width               = 600,
        .max_height          = 500,
        .row_height_text     = 36,
        .row_height_image    = 64,
        .thumbnail_size      = 48,
        .preview_width       = 500,
        .preview_max_height  = 600,
        .dim_backdrop        = true,
        .auto_paste          = true,
        .paste_delay_ms      = 80,
        .close_on_select     = true,
    };
}

#endif
