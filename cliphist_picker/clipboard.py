import re
import subprocess

from .models import RawEntry

BINARY_RE = re.compile(
    r"^\[\[ binary data ([\d.]+\s+\w+)\s+(\w+)\s+(\d+x\d+) \]\]$"
)


def list_entries() -> list[RawEntry]:
    """Run `cliphist list` and parse each line into a RawEntry."""
    result = subprocess.run(
        ["cliphist", "list"],
        capture_output=True, text=True, timeout=5,
    )
    entries: list[RawEntry] = []
    for line in result.stdout.strip().splitlines():
        if "\t" not in line:
            continue
        id_str, preview = line.split("\t", 1)
        try:
            entry_id = int(id_str.strip())
        except ValueError:
            continue
        match = BINARY_RE.match(preview.strip())
        if match:
            entries.append(RawEntry(
                id=entry_id, preview=preview, raw_line=line,
                is_binary=True,
                binary_size=match.group(1),
                binary_format=match.group(2),
                binary_dims=match.group(3),
            ))
        else:
            entries.append(RawEntry(
                id=entry_id, preview=preview, raw_line=line,
                is_binary=False,
            ))
    return entries


def decode_entry(raw_line: str) -> bytes:
    """Pipe a raw cliphist line through `cliphist decode` and return raw bytes."""
    result = subprocess.run(
        ["cliphist", "decode"],
        input=raw_line.encode(), capture_output=True, timeout=5,
    )
    return result.stdout


def copy_text_to_clipboard(raw_line: str) -> None:
    """Decode a text entry and pipe to wl-copy."""
    decode = subprocess.Popen(
        ["cliphist", "decode"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    )
    wl = subprocess.Popen(["wl-copy"], stdin=decode.stdout)
    decode.stdout.close()
    decode.stdin.write(raw_line.encode())
    decode.stdin.close()
    wl.wait()
    decode.wait()


def copy_image_to_clipboard(raw_line: str, mime: str = "image/png") -> None:
    """Decode an image entry and pipe to wl-copy with explicit MIME type."""
    decode = subprocess.Popen(
        ["cliphist", "decode"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    )
    wl = subprocess.Popen(["wl-copy", "--type", mime], stdin=decode.stdout)
    decode.stdout.close()
    decode.stdin.write(raw_line.encode())
    decode.stdin.close()
    wl.wait()
    decode.wait()


def paste_via_wtype() -> None:
    """Simulate Ctrl+V keystroke via wtype."""
    subprocess.Popen(["wtype", "-M", "ctrl", "v", "-m", "ctrl"])
