# Cliphist Picker

A fast GTK4 clipboard history picker for Wayland compositors. Runs as a resident daemon for instant activation (~70-80 ms) via `gtk4-layer-shell`.

## Features

- Resident daemon — starts at login, stays in memory, activates instantly on keybind
- Full-screen transparent overlay with exclusive keyboard grab (Layer Shell)
- Text and image clipboard entries with thumbnail previews
- Adjacent text+image deduplication (merged rows)
- Modular theme system with hot-swappable CSS
- Auto-paste after selection via `wtype`

## Dependencies

**Runtime:** `cliphist`, `wl-clipboard`, `wtype`, `gtk4`, `gtk4-layer-shell`

**Build:** `meson`, `ninja`, `glib-compile-resources` (part of glib2)

On Arch Linux:

```sh
pacman -S gtk4 gtk4-layer-shell meson wl-clipboard wtype
paru -S cliphist   # or yay
```

## Build

```sh
meson setup build
meson compile -C build
```

## Usage

### Hyprland

Add the daemon to your `hyprland.conf`:

```conf
exec-once = /path/to/cliphist-picker
bind = $mainMod, V, exec, /path/to/cliphist-picker
```

The first `exec-once` starts the daemon at login. The keybind launches a second process that immediately exits — GtkApplication's D-Bus single-instance mechanism forwards an `activate` signal to the running daemon, which fetches fresh clipboard data, rebuilds the list, and shows the window.

### Controls

| Key | Action |
|---|---|
| `Escape` | Dismiss |
| Click outside | Dismiss |
| Click row / `Enter` | Copy entry and auto-paste |
| Hover image row | Show large preview |

## Themes

The picker ships with Catppuccin Mocha. Config files are created at `~/.config/cliphist-picker/` on first launch. See [THEMES.md](THEMES.md) for the full theme system reference.

## Reusing the daemon pattern

The resident daemon architecture is generic and can be extracted for any GTK4 Layer Shell overlay tool (emoji picker, app launcher, color picker, etc.). Here is how it works:

### Core idea

```
login  →  daemon starts  →  GTK init + Layer Shell setup (once)
keybind  →  second process launches  →  exits immediately
         →  D-Bus activate signal  →  daemon refreshes content + shows window
dismiss  →  window hides (not destroyed)  →  daemon stays alive
```

### Key components

1. **GtkApplication single-instance** — set an app ID (`com.example.my-tool`) and connect to both `"startup"` (one-time init) and `"activate"` (per-invocation refresh). GtkApplication handles the D-Bus registration and signal forwarding automatically.

2. **Window lifecycle** — create the window shell once in the first `activate` call. On subsequent activations, clear and repopulate content. On dismiss, hide with `gtk_widget_set_visible(win, FALSE)` instead of closing/quitting.

3. **Generation counter** — if you have background threads (thumbnail loading, async I/O), increment a generation counter on each refresh. Threads check `job->generation == current_generation` before posting results back to the main thread, preventing stale data from corrupting a newer session.

4. **Minimal `main()`:**

```c
static MyWindow *g_win = NULL;

static void on_startup(GtkApplication *app, gpointer data) {
    /* Load CSS, set up Layer Shell defaults — runs once */
}

static void on_activate(GtkApplication *app, gpointer data) {
    if (!g_win)
        g_win = my_window_new(app);  /* empty shell */

    /* Fetch fresh data */
    Data *items = fetch_items();
    my_window_refresh(g_win, items);
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new(
        "com.example.my-tool", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "startup",  G_CALLBACK(on_startup),  NULL);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
```

This gives you a daemon that idles at ~8 MB RSS and activates in under 80 ms on subsequent invocations, compared to 300+ ms for a cold start.
