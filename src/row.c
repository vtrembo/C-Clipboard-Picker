#include "row.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static GdkTexture *texture_from_pixbuf(GdkPixbuf *pixbuf) {
    return gdk_texture_new_for_pixbuf(pixbuf);
}
G_GNUC_END_IGNORE_DEPRECATIONS

static GtkListBoxRow *create_text_row(const ClipboardEntry *entry,
                                      const Config *config) {
    GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
    g_object_set_data(G_OBJECT(row), "clipboard-entry", (gpointer)entry);

    GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));
    gtk_widget_add_css_class(GTK_WIDGET(box), "text-row");
    gtk_widget_set_margin_start(GTK_WIDGET(box), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(box), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(box), 4);
    gtk_widget_set_margin_bottom(GTK_WIDGET(box), 4);

    GtkImage *icon = GTK_IMAGE(gtk_image_new_from_icon_name("edit-paste-symbolic"));
    gtk_widget_add_css_class(GTK_WIDGET(icon), "row-icon");
    gtk_box_append(box, GTK_WIDGET(icon));

    GtkLabel *label = GTK_LABEL(gtk_label_new(entry->text_preview ? entry->text_preview : ""));
    gtk_label_set_xalign(label, 0.0f);
    gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_END);
    gtk_label_set_single_line_mode(label, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(label), "text-content");
    gtk_widget_set_hexpand(GTK_WIDGET(label), TRUE);
    gtk_box_append(box, GTK_WIDGET(label));

    gtk_list_box_row_set_child(row, GTK_WIDGET(box));
    gtk_widget_set_size_request(GTK_WIDGET(row), -1, config->row_height_text);
    return row;
}

static GtkListBoxRow *create_image_row(const ClipboardEntry *entry,
                                       const Config *config) {
    GtkListBoxRow *row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
    g_object_set_data(G_OBJECT(row), "clipboard-entry", (gpointer)entry);

    GtkBox *hbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10));
    gtk_widget_add_css_class(GTK_WIDGET(hbox), "image-row");
    gtk_widget_set_margin_start(GTK_WIDGET(hbox), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(hbox), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(hbox), 4);
    gtk_widget_set_margin_bottom(GTK_WIDGET(hbox), 4);

    GtkPicture *thumbnail = GTK_PICTURE(gtk_picture_new());
    gtk_widget_set_size_request(GTK_WIDGET(thumbnail),
                                config->thumbnail_size, config->thumbnail_size);
    gtk_picture_set_can_shrink(thumbnail, TRUE);
    gtk_picture_set_content_fit(thumbnail, GTK_CONTENT_FIT_COVER);
    gtk_widget_add_css_class(GTK_WIDGET(thumbnail), "thumbnail");
    g_object_set_data(G_OBJECT(row), "thumbnail-widget", thumbnail);
    gtk_box_append(hbox, GTK_WIDGET(thumbnail));

    GtkBox *vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
    gtk_widget_set_valign(GTK_WIDGET(vbox), GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(GTK_WIDGET(vbox), TRUE);

    const char *fmt = entry->image_format ? entry->image_format : "?";
    const char *dims = entry->image_dims ? entry->image_dims : "?";
    const char *size = entry->image_size ? entry->image_size : "?";
    char *upper_fmt = g_ascii_strup(fmt, -1);
    char *meta_text = g_strdup_printf("%s  %s  (%s)", upper_fmt, dims, size);

    GtkLabel *meta_label = GTK_LABEL(gtk_label_new(meta_text));
    gtk_label_set_xalign(meta_label, 0.0f);
    gtk_widget_add_css_class(GTK_WIDGET(meta_label), "image-meta");
    gtk_box_append(vbox, GTK_WIDGET(meta_label));

    g_free(upper_fmt);
    g_free(meta_text);

    gtk_box_append(hbox, GTK_WIDGET(vbox));
    gtk_list_box_row_set_child(row, GTK_WIDGET(hbox));
    gtk_widget_set_size_request(GTK_WIDGET(row), -1, config->row_height_image);
    return row;
}

GtkListBoxRow *create_row(const ClipboardEntry *entry, const Config *config) {
    if (entry->entry_type == ENTRY_TYPE_TEXT)
        return create_text_row(entry, config);
    return create_image_row(entry, config);
}

GdkTexture *load_thumbnail_from_bytes(const guint8 *data, gsize len,
                                      int target_size) {
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    if (!gdk_pixbuf_loader_write(loader, data, len, NULL)) {
        g_object_unref(loader);
        return NULL;
    }
    gdk_pixbuf_loader_close(loader, NULL);

    GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    if (!pixbuf) {
        g_object_unref(loader);
        return NULL;
    }

    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    int new_w, new_h;

    if (w > h) {
        new_w = target_size;
        new_h = MAX(1, h * target_size / w);
    } else {
        new_h = target_size;
        new_w = MAX(1, w * target_size / h);
    }

    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, new_w, new_h,
                                                  GDK_INTERP_BILINEAR);
    GdkTexture *texture = texture_from_pixbuf(scaled);

    g_object_unref(scaled);
    g_object_unref(loader);
    return texture;
}

GdkTexture *load_preview_from_bytes(const guint8 *data, gsize len,
                                    int max_width, int max_height) {
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    if (!gdk_pixbuf_loader_write(loader, data, len, NULL)) {
        g_object_unref(loader);
        return NULL;
    }
    gdk_pixbuf_loader_close(loader, NULL);

    GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    if (!pixbuf) {
        g_object_unref(loader);
        return NULL;
    }

    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    double scale_w = (double)max_width / w;
    double scale_h = (double)max_height / h;
    double scale = scale_w < scale_h ? scale_w : scale_h;
    if (scale > 1.0) scale = 1.0;

    GdkTexture *texture;
    if (scale < 1.0) {
        int new_w = MAX(1, (int)(w * scale));
        int new_h = MAX(1, (int)(h * scale));
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, new_w, new_h,
                                                      GDK_INTERP_BILINEAR);
        texture = texture_from_pixbuf(scaled);
        g_object_unref(scaled);
    } else {
        texture = texture_from_pixbuf(pixbuf);
    }

    g_object_unref(loader);
    return texture;
}
