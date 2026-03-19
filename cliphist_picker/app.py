import gi

gi.require_version("Gdk", "4.0")
gi.require_version("Gtk", "4.0")

from pathlib import Path

from gi.repository import Gdk, Gtk

from .clipboard import list_entries
from .config import Config, ensure_config_dir, load_config
from .dedup import deduplicate
from .window import PickerWindow

BUNDLED_DATA_DIR = Path(__file__).parent.parent / "data"


class CliphistPickerApp(Gtk.Application):
    def __init__(self):
        super().__init__(application_id="com.github.cliphist-picker")
        self.config: Config | None = None

    def do_startup(self):
        Gtk.Application.do_startup(self)
        ensure_config_dir(BUNDLED_DATA_DIR)
        self.config = load_config()
        self._load_css()

    def do_activate(self):
        raw = list_entries()
        entries = deduplicate(
            raw,
            id_threshold=self.config.dedup_id_threshold,
            max_entries=self.config.max_entries,
        )

        if not entries:
            self.quit()
            return

        win = PickerWindow(self, entries, self.config)
        win.present()

    def _load_css(self):
        provider = Gtk.CssProvider()
        theme_path = self.config.theme_path

        if theme_path.exists():
            provider.load_from_path(str(theme_path))
        else:
            fallback = BUNDLED_DATA_DIR / "themes" / f"{self.config.theme}.css"
            if fallback.exists():
                provider.load_from_path(str(fallback))

        Gtk.StyleContext.add_provider_for_display(
            Gdk.Display.get_default(),
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION,
        )
