# Themes

The picker ships with **Catppuccin Mocha** as the default theme. On first launch, config files are created at `~/.config/cliphist-picker/`.

## How it works

The theme system uses a two-level config chain (similar to Hyprland's `source`):

```
~/.config/cliphist-picker/
├── theme.conf                      # points to a theme config
└── themes/
    ├── catppuccin-mocha.conf        # points to its CSS file
    └── catppuccin-mocha.css         # the actual styles
```

**`theme.conf`** — selects which theme to use:
```conf
source = themes/catppuccin-mocha.conf
```

**`themes/catppuccin-mocha.conf`** — declares the CSS file:
```conf
css = catppuccin-mocha.css
```

## Switch to a different theme

1. Create your theme files:
   ```sh
   cp ~/.config/cliphist-picker/themes/catppuccin-mocha.css \
      ~/.config/cliphist-picker/themes/my-theme.css
   ```

2. Create a conf for it:
   ```sh
   echo "css = my-theme.css" > ~/.config/cliphist-picker/themes/my-theme.conf
   ```

3. Point `theme.conf` to it:
   ```sh
   echo "source = themes/my-theme.conf" > ~/.config/cliphist-picker/theme.conf
   ```

4. Restart the daemon:
   ```sh
   killall cliphist-picker && cliphist-picker &
   ```

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
