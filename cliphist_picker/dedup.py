from .models import ClipboardEntry, EntryType, RawEntry


def deduplicate(
    raw_entries: list[RawEntry],
    id_threshold: int = 3,
    max_entries: int = 50,
) -> list[ClipboardEntry]:
    """Group adjacent text+image entries from the same copy operation.

    cliphist list returns entries most-recent-first (descending ID).
    When a single copy stores both text and image, their IDs are
    within ``id_threshold`` and they appear adjacent.

    Walk linearly: at index *i*, peek at the next unconsumed index *j*.
    If one is binary and the other text with IDs close enough, merge
    into a MERGED entry. Otherwise emit standalone.
    """
    result: list[ClipboardEntry] = []
    consumed: set[int] = set()
    i = 0

    while i < len(raw_entries) and len(result) < max_entries:
        if i in consumed:
            i += 1
            continue

        entry = raw_entries[i]

        # Find the next unconsumed entry
        j = i + 1
        while j < len(raw_entries) and j in consumed:
            j += 1

        merged = False
        if j < len(raw_entries):
            neighbor = raw_entries[j]
            id_gap = abs(entry.id - neighbor.id)

            if id_gap <= id_threshold and entry.is_binary != neighbor.is_binary:
                if entry.is_binary:
                    img_entry, txt_entry = entry, neighbor
                else:
                    txt_entry, img_entry = entry, neighbor

                result.append(ClipboardEntry(
                    entry_type=EntryType.MERGED,
                    text_id=txt_entry.id,
                    text_preview=txt_entry.preview,
                    image_id=img_entry.id,
                    image_size=img_entry.binary_size,
                    image_format=img_entry.binary_format,
                    image_dims=img_entry.binary_dims,
                    primary_raw_line=txt_entry.raw_line,
                    image_raw_line=img_entry.raw_line,
                ))
                consumed.add(i)
                consumed.add(j)
                merged = True

        if not merged:
            if entry.is_binary:
                result.append(ClipboardEntry(
                    entry_type=EntryType.IMAGE,
                    image_id=entry.id,
                    image_size=entry.binary_size,
                    image_format=entry.binary_format,
                    image_dims=entry.binary_dims,
                    primary_raw_line=entry.raw_line,
                    image_raw_line=entry.raw_line,
                ))
            else:
                result.append(ClipboardEntry(
                    entry_type=EntryType.TEXT,
                    text_id=entry.id,
                    text_preview=entry.preview,
                    primary_raw_line=entry.raw_line,
                ))
            consumed.add(i)

        i += 1

    return result
