# Themes

The picker ships with **Catppuccin Mocha** as the default theme, embedded in the binary.

## Override the default theme

1. Copy the default CSS to your config directory:
   ```sh
   mkdir -p ~/.config/cliphist-picker/themes
   cp data/themes/catppuccin-mocha.css ~/.config/cliphist-picker/themes/
   ```
2. Edit `~/.config/cliphist-picker/themes/catppuccin-mocha.css` to your liking.
3. Restart the daemon (`killall cliphist-picker && cliphist-picker &`).

The picker checks for `~/.config/cliphist-picker/themes/catppuccin-mocha.css` at startup — if the file exists, it is used instead of the embedded default.

## Create a new theme

1. Copy the default as a starting point:
   ```sh
   cp data/themes/catppuccin-mocha.css data/themes/my-theme.css
   ```
2. Edit colors, fonts, borders, etc. in `my-theme.css`.
3. Place it at `~/.config/cliphist-picker/themes/catppuccin-mocha.css` (the filename the picker looks for).
4. Restart the daemon.

## CSS classes reference

| Class | Element |
|---|---|
| `window.backdrop` | Full-screen transparent overlay |
| `.picker-frame` | Main centered container |
| `.picker-scroll` | Scrollable area |
| `.picker-list` | ListBox |
| `.picker-list row` | Individual row |
| `.text-row` | Text entry row container |
| `.text-content` | Text entry label |
| `.row-icon` | Paste icon in text rows |
| `.image-row` | Image/merged entry row container |
| `.merged-text` | Text label in merged entries |
| `.image-meta` | Format/dimensions/size label |
| `.thumbnail` | Image thumbnail |
| `.preview-image` | Large image preview panel |
| `.picker-scroll scrollbar slider` | Scrollbar thumb |
