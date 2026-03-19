import threading

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Gtk4LayerShell", "1.0")

from gi.repository import Gdk, GLib, Gtk, Gtk4LayerShell

from .clipboard import (
    copy_image_to_clipboard,
    copy_text_to_clipboard,
    decode_entry,
    paste_via_wtype,
)
from .config import Config
from .models import ClipboardEntry, EntryType
from .row import create_row, load_preview_from_bytes, load_thumbnail_from_bytes


class PickerWindow(Gtk.Window):
    def __init__(self, app: Gtk.Application, entries: list[ClipboardEntry], config: Config):
        super().__init__(application=app)
        self.app = app
        self.config = config
        self.entries = entries
        self._image_cache: dict[str, bytes] = {}
        self._hovered_row: Gtk.ListBoxRow | None = None

        # Layer Shell setup
        Gtk4LayerShell.init_for_window(self)
        Gtk4LayerShell.set_layer(self, Gtk4LayerShell.Layer.OVERLAY)
        Gtk4LayerShell.set_keyboard_mode(
            self, Gtk4LayerShell.KeyboardMode.EXCLUSIVE,
        )
        Gtk4LayerShell.set_namespace(self, "cliphist-picker")
        Gtk4LayerShell.set_exclusive_zone(self, -1)

        # Anchor all edges -> full screen
        for edge in (
            Gtk4LayerShell.Edge.TOP,
            Gtk4LayerShell.Edge.BOTTOM,
            Gtk4LayerShell.Edge.LEFT,
            Gtk4LayerShell.Edge.RIGHT,
        ):
            Gtk4LayerShell.set_anchor(self, edge, True)

        self.remove_css_class("background")
        self.add_css_class("backdrop")

        self._build_ui()
        self._setup_input()
        self._populate()
        self._load_thumbnails_async()

        # Position preview after the window gets its allocation
        self.connect("realize", self._on_realize)

    # ── Layout ───────────────────────────────────────────────────────

    def _build_ui(self):
        # Use Overlay so the preview floats without shifting the picker
        overlay = Gtk.Overlay()

        # Full-screen backdrop — ensures the Wayland input region covers the
        # entire surface so clicks outside the picker and keyboard events
        # are forwarded by the compositor.
        backdrop = Gtk.Box()
        backdrop.set_hexpand(True)
        backdrop.set_vexpand(True)
        overlay.set_child(backdrop)

        # Picker frame (centered, overlay child)
        self.picker_frame = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.picker_frame.add_css_class("picker-frame")
        self.picker_frame.set_size_request(self.config.width, -1)
        self.picker_frame.set_halign(Gtk.Align.CENTER)
        self.picker_frame.set_valign(Gtk.Align.CENTER)

        self.scrolled = Gtk.ScrolledWindow()
        self.scrolled.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        self.scrolled.set_max_content_height(self.config.max_height)
        self.scrolled.set_propagate_natural_height(True)
        self.scrolled.add_css_class("picker-scroll")

        self.listbox = Gtk.ListBox()
        self.listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        self.listbox.add_css_class("picker-list")
        self.scrolled.set_child(self.listbox)
        self.picker_frame.append(self.scrolled)
        overlay.add_overlay(self.picker_frame)

        # Preview — floats to the right of center, doesn't affect picker position
        self.preview_picture = Gtk.Picture()
        self.preview_picture.set_can_shrink(True)
        self.preview_picture.set_content_fit(Gtk.ContentFit.CONTAIN)
        self.preview_picture.add_css_class("preview-image")
        self.preview_picture.set_size_request(self.config.preview_width, -1)
        self.preview_picture.set_halign(Gtk.Align.END)
        self.preview_picture.set_valign(Gtk.Align.CENTER)
        self.preview_picture.set_margin_end(
            max(20, (1920 - self.config.width) // 2 - self.config.preview_width - 40),
        )
        self.preview_picture.set_visible(False)
        overlay.add_overlay(self.preview_picture)

        self.set_child(overlay)

    def _on_realize(self, widget):
        """Position the preview based on actual screen width."""
        surface = self.get_surface()
        if surface:
            monitor = self.get_display().get_monitor_at_surface(surface)
            if monitor:
                screen_w = monitor.get_geometry().width
                # Place preview in the gap between center picker and screen edge
                gap = (screen_w - self.config.width) // 2
                margin = max(20, gap - self.config.preview_width - 24)
                self.preview_picture.set_margin_end(margin)

    # ── Input ────────────────────────────────────────────────────────

    def _setup_input(self):
        key_ctrl = Gtk.EventControllerKey()
        key_ctrl.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
        key_ctrl.connect("key-pressed", self._on_key_pressed)
        self.add_controller(key_ctrl)

        self.listbox.connect("row-activated", self._on_row_activated)
        self.listbox.connect("selected-rows-changed", self._on_selection_changed)

        # Backdrop click
        click_ctrl = Gtk.GestureClick()
        click_ctrl.connect("pressed", self._on_backdrop_click)
        self.add_controller(click_ctrl)

        # Hover for preview
        motion_ctrl = Gtk.EventControllerMotion()
        motion_ctrl.connect("motion", self._on_listbox_motion)
        motion_ctrl.connect("leave", self._on_listbox_leave)
        self.listbox.add_controller(motion_ctrl)

    def _populate(self):
        for entry in self.entries:
            row = create_row(entry, self.config)
            self.listbox.append(row)
        first = self.listbox.get_row_at_index(0)
        if first:
            self.listbox.select_row(first)
            first.grab_focus()

    # ── Keyboard ─────────────────────────────────────────────────────

    def _on_key_pressed(self, controller, keyval, keycode, state):
        name = Gdk.keyval_name(keyval)
        print(f"[DEBUG] key-pressed: {name}", flush=True)

        if name == "Escape":
            self._dismiss()
            return True

        if name in ("Return", "KP_Enter"):
            selected = self.listbox.get_selected_row()
            if selected:
                self._activate_entry(selected.entry)
            return True

        if name in ("Up", "w", "W"):
            self._move_selection(-1)
            return True

        if name in ("Down", "s", "S"):
            self._move_selection(1)
            return True

        return False

    def _move_selection(self, delta: int):
        selected = self.listbox.get_selected_row()
        if selected is None:
            target_idx = 0
        else:
            target_idx = selected.get_index() + delta

        target_idx = max(0, min(target_idx, len(self.entries) - 1))
        target_row = self.listbox.get_row_at_index(target_idx)
        if target_row:
            self.listbox.select_row(target_row)
            target_row.grab_focus()

    # ── Selection / Preview ──────────────────────────────────────────

    def _on_selection_changed(self, listbox):
        selected = listbox.get_selected_row()
        if selected is None:
            self.preview_picture.set_visible(False)
            return

        entry: ClipboardEntry = selected.entry
        if entry.has_image and entry.image_raw_line:
            self._show_preview_for(entry)
        else:
            self.preview_picture.set_visible(False)

    def _on_listbox_motion(self, controller, x, y):
        row = self.listbox.get_row_at_y(y)
        if row is self._hovered_row:
            return
        self._hovered_row = row
        if row is None:
            return
        entry: ClipboardEntry = row.entry
        if entry.has_image and entry.image_raw_line:
            self._show_preview_for(entry)
        else:
            # Only hide preview if keyboard selection also has no image
            selected = self.listbox.get_selected_row()
            if selected is None or not selected.entry.has_image:
                self.preview_picture.set_visible(False)

    def _on_listbox_leave(self, controller):
        self._hovered_row = None
        # Fall back to keyboard selection preview
        selected = self.listbox.get_selected_row()
        if selected and selected.entry.has_image and selected.entry.image_raw_line:
            self._show_preview_for(selected.entry)
        else:
            self.preview_picture.set_visible(False)

    def _show_preview_for(self, entry: ClipboardEntry):
        raw_line = entry.image_raw_line
        if raw_line in self._image_cache:
            self._render_preview(self._image_cache[raw_line])
        else:
            def decode_and_show():
                data = decode_entry(raw_line)
                self._image_cache[raw_line] = data
                GLib.idle_add(self._render_preview, data)
            threading.Thread(target=decode_and_show, daemon=True).start()

    def _render_preview(self, image_bytes: bytes):
        texture = load_preview_from_bytes(
            image_bytes, self.config.preview_width, self.config.preview_max_height,
        )
        if texture:
            self.preview_picture.set_paintable(texture)
            self.preview_picture.set_visible(True)
        else:
            self.preview_picture.set_visible(False)
        return False  # for GLib.idle_add

    # ── Activation ───────────────────────────────────────────────────

    def _on_row_activated(self, listbox, row):
        self._activate_entry(row.entry)

    def _activate_entry(self, entry: ClipboardEntry):
        if entry.entry_type == EntryType.IMAGE:
            mime = f"image/{entry.image_format or 'png'}"
            copy_image_to_clipboard(entry.primary_raw_line, mime)
        else:
            copy_text_to_clipboard(entry.primary_raw_line)

        if self.config.close_on_select:
            self.set_visible(False)

        if self.config.auto_paste:
            GLib.timeout_add(self.config.paste_delay_ms, self._do_paste)
        else:
            self._dismiss()

    def _do_paste(self):
        paste_via_wtype()
        self._dismiss()
        return False

    def _dismiss(self):
        print("[DEBUG] _dismiss called", flush=True)
        self.close()
        self.app.quit()

    # ── Backdrop Click ───────────────────────────────────────────────

    def _on_backdrop_click(self, gesture, n_press, x, y):
        widget_at = self.pick(x, y, Gtk.PickFlags.DEFAULT)
        inside = self._is_inside_picker(widget_at) if widget_at else False
        print(f"[DEBUG] backdrop-click: widget={widget_at}, inside={inside}", flush=True)
        # Dismiss if click hit transparent area (None) or anything outside the picker
        if widget_at is None or not self._is_inside_picker(widget_at):
            self._dismiss()

    def _is_inside_picker(self, widget):
        """Check if a widget is a descendant of the picker frame or preview."""
        while widget is not None:
            if widget is self.picker_frame or widget is self.preview_picture:
                return True
            widget = widget.get_parent()
        return False

    # ── Async Thumbnails ─────────────────────────────────────────────

    def _load_thumbnails_async(self):
        def load_all():
            for i, entry in enumerate(self.entries):
                if not entry.has_image or not entry.image_raw_line:
                    continue
                raw_line = entry.image_raw_line
                if raw_line not in self._image_cache:
                    data = decode_entry(raw_line)
                    self._image_cache[raw_line] = data
                else:
                    data = self._image_cache[raw_line]

                if data:
                    # Create texture on main thread (GdkPixbuf not thread-safe)
                    GLib.idle_add(self._create_and_set_thumbnail, i, data)

        threading.Thread(target=load_all, daemon=True).start()

    def _create_and_set_thumbnail(self, index: int, data: bytes):
        texture = load_thumbnail_from_bytes(data, self.config.thumbnail_size)
        if texture:
            self._set_thumbnail(index, texture)
        return False

    def _set_thumbnail(self, index: int, texture):
        row = self.listbox.get_row_at_index(index)
        if row and hasattr(row, "thumbnail_widget"):
            row.thumbnail_widget.set_paintable(texture)
        return False
