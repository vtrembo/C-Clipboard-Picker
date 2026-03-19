from dataclasses import dataclass
from enum import Enum


class EntryType(Enum):
    TEXT = "text"
    IMAGE = "image"
    MERGED = "merged"


@dataclass
class RawEntry:
    """One line from `cliphist list`, parsed."""
    id: int
    preview: str
    raw_line: str
    is_binary: bool
    binary_size: str | None = None
    binary_format: str | None = None
    binary_dims: str | None = None


@dataclass
class ClipboardEntry:
    """Deduplicated entry ready for display."""
    entry_type: EntryType

    text_id: int | None = None
    text_preview: str | None = None

    image_id: int | None = None
    image_size: str | None = None
    image_format: str | None = None
    image_dims: str | None = None

    primary_raw_line: str = ""
    image_raw_line: str | None = None

    @property
    def has_image(self) -> bool:
        return self.entry_type in (EntryType.IMAGE, EntryType.MERGED)

    @property
    def display_text(self) -> str:
        if self.text_preview:
            return self.text_preview
        if self.image_format and self.image_dims and self.image_size:
            return f"[{self.image_format.upper()} {self.image_dims} - {self.image_size}]"
        return "[Image]"
