import gi

gi.require_version("Gtk", "4.0")
gi.require_version("GdkPixbuf", "2.0")

from gi.repository import Gdk, GdkPixbuf, Gtk, Pango

from .config import Config
from .models import ClipboardEntry, EntryType


def create_row(entry: ClipboardEntry, config: Config) -> Gtk.ListBoxRow:
    """Factory: return the correct row widget for an entry type."""
    if entry.entry_type == EntryType.TEXT:
        return _create_text_row(entry, config)
    return _create_image_row(entry, config)


def _create_text_row(entry: ClipboardEntry, config: Config) -> Gtk.ListBoxRow:
    row = Gtk.ListBoxRow()
    row.entry = entry

    label = Gtk.Label(label=entry.text_preview or "")
    label.set_xalign(0.0)
    label.set_ellipsize(Pango.EllipsizeMode.END)
    label.set_single_line_mode(True)
    label.add_css_class("text-content")
    label.set_hexpand(True)

    box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
    box.add_css_class("text-row")
    box.set_margin_start(8)
    box.set_margin_end(8)
    box.set_margin_top(4)
    box.set_margin_bottom(4)

    icon = Gtk.Image.new_from_icon_name("edit-paste-symbolic")
    icon.add_css_class("row-icon")
    box.append(icon)
    box.append(label)

    row.set_child(box)
    row.set_size_request(-1, config.row_height_text)
    return row


def _create_image_row(entry: ClipboardEntry, config: Config) -> Gtk.ListBoxRow:
    row = Gtk.ListBoxRow()
    row.entry = entry

    hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
    hbox.add_css_class("image-row")
    hbox.set_margin_start(8)
    hbox.set_margin_end(8)
    hbox.set_margin_top(4)
    hbox.set_margin_bottom(4)

    thumbnail = Gtk.Picture()
    thumbnail.set_size_request(config.thumbnail_size, config.thumbnail_size)
    thumbnail.set_can_shrink(True)
    thumbnail.set_content_fit(Gtk.ContentFit.COVER)
    thumbnail.add_css_class("thumbnail")
    row.thumbnail_widget = thumbnail
    hbox.append(thumbnail)

    vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
    vbox.set_valign(Gtk.Align.CENTER)
    vbox.set_hexpand(True)

    if entry.entry_type == EntryType.MERGED and entry.text_preview:
        text_label = Gtk.Label(label=entry.text_preview)
        text_label.set_xalign(0.0)
        text_label.set_ellipsize(Pango.EllipsizeMode.END)
        text_label.set_single_line_mode(True)
        text_label.add_css_class("merged-text")
        vbox.append(text_label)

    fmt = (entry.image_format or "?").upper()
    dims = entry.image_dims or "?"
    size = entry.image_size or "?"
    meta_label = Gtk.Label(label=f"{fmt}  {dims}  ({size})")
    meta_label.set_xalign(0.0)
    meta_label.add_css_class("image-meta")
    vbox.append(meta_label)

    hbox.append(vbox)
    row.set_child(hbox)
    row.set_size_request(-1, config.row_height_image)
    return row


def load_thumbnail_from_bytes(
    image_bytes: bytes, target_size: int,
) -> Gdk.Texture | None:
    """Create a scaled Gdk.Texture from raw image bytes."""
    try:
        loader = GdkPixbuf.PixbufLoader()
        loader.write(image_bytes)
        loader.close()
        pixbuf = loader.get_pixbuf()
        if pixbuf is None:
            return None
        w, h = pixbuf.get_width(), pixbuf.get_height()
        if w > h:
            new_w = target_size
            new_h = max(1, int(h * target_size / w))
        else:
            new_h = target_size
            new_w = max(1, int(w * target_size / h))
        scaled = pixbuf.scale_simple(new_w, new_h, GdkPixbuf.InterpType.BILINEAR)
        return Gdk.Texture.new_for_pixbuf(scaled)
    except Exception:
        return None


def load_preview_from_bytes(
    image_bytes: bytes, max_width: int, max_height: int,
) -> Gdk.Texture | None:
    """Create a Gdk.Texture for the larger preview, constrained to max dims."""
    try:
        loader = GdkPixbuf.PixbufLoader()
        loader.write(image_bytes)
        loader.close()
        pixbuf = loader.get_pixbuf()
        if pixbuf is None:
            return None
        w, h = pixbuf.get_width(), pixbuf.get_height()
        scale = min(max_width / w, max_height / h, 1.0)
        if scale < 1.0:
            new_w = max(1, int(w * scale))
            new_h = max(1, int(h * scale))
            pixbuf = pixbuf.scale_simple(
                new_w, new_h, GdkPixbuf.InterpType.BILINEAR,
            )
        return Gdk.Texture.new_for_pixbuf(pixbuf)
    except Exception:
        return None
