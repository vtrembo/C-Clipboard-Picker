#!/usr/bin/env python3
"""Launch the cliphist picker."""
import ctypes
import os
import sys

# gtk4-layer-shell must be loaded before libwayland-client.
# See https://github.com/wmww/gtk4-layer-shell/blob/main/linking.md
if "gtk4-layer-shell" not in os.environ.get("LD_PRELOAD", ""):
    ctypes.cdll.LoadLibrary("libgtk4-layer-shell.so")

from cliphist_picker.app import CliphistPickerApp


def main():
    app = CliphistPickerApp()
    app.run(sys.argv)


if __name__ == "__main__":
    main()
