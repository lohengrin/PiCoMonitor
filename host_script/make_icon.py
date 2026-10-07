#!/usr/bin/env python3
"""Generates icon.ico (16-256 px, for the PyInstaller build) and assets/icon.png from icon_art.py"""

import os

from icon_art import draw_icon

HERE = os.path.dirname(os.path.abspath(__file__))
SIZES = [16, 24, 32, 48, 64, 128, 256]


def make_icon(directory: str = HERE) -> str:
    ico = os.path.join(directory, "icon.ico")
    draw_icon(256).save(ico, format="ICO", sizes=[(s, s) for s in SIZES])
    draw_icon(256).save(os.path.join(directory, "assets", "icon.png"))
    return ico


if __name__ == "__main__":
    print("wrote", make_icon())
