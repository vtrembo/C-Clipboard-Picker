import shutil
import tomllib
from dataclasses import dataclass
from pathlib import Path

CONFIG_DIR = Path.home() / ".config" / "cliphist-picker"
CONFIG_FILE = CONFIG_DIR / "config.toml"


@dataclass
class Config:
    # general
    max_entries: int = 50
    dedup_id_threshold: int = 3
    # appearance
    theme: str = "catppuccin-mocha"
    width: int = 600
    max_height: int = 500
    row_height_text: int = 36
    row_height_image: int = 64
    thumbnail_size: int = 48
    preview_width: int = 500
    preview_max_height: int = 600
    dim_backdrop: bool = True
    # behavior
    auto_paste: bool = True
    paste_delay_ms: int = 80
    close_on_select: bool = True

    @property
    def theme_path(self) -> Path:
        return CONFIG_DIR / "themes" / f"{self.theme}.css"


def load_config() -> Config:
    """Load config from TOML file, falling back to defaults."""
    cfg = Config()
    if CONFIG_FILE.exists():
        with open(CONFIG_FILE, "rb") as f:
            data = tomllib.load(f)
        for section in ("general", "appearance", "behavior"):
            for key, value in data.get(section, {}).items():
                if hasattr(cfg, key):
                    setattr(cfg, key, value)
    return cfg


def ensure_config_dir(bundled_data_dir: Path) -> None:
    """Create ~/.config/cliphist-picker/ and copy defaults if missing."""
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)
    themes_dir = CONFIG_DIR / "themes"
    themes_dir.mkdir(exist_ok=True)

    if not CONFIG_FILE.exists():
        src = bundled_data_dir / "config.toml"
        if src.exists():
            shutil.copy2(src, CONFIG_FILE)

    bundled_themes = bundled_data_dir / "themes"
    if bundled_themes.is_dir():
        for css_file in bundled_themes.glob("*.css"):
            dest = themes_dir / css_file.name
            if not dest.exists():
                shutil.copy2(css_file, dest)
