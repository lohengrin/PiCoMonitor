"""The PiCoMonitor icon (level bars), drawn with Pillow: used for the tray icon and, through
make_icon.py, for icon.ico of the Windows executable. Same drawing as assets/icon.svg (64x64 grid)."""

from PIL import Image, ImageDraw

BACKGROUND = "#14212b"
# (x, y, width, height, color) on the 64x64 grid
BARS = (
    (12, 34, 8, 18, "#2ecc71"),
    (23, 24, 8, 28, "#2ecc71"),
    (34, 16, 8, 36, "#f5a623"),
    (45, 10, 8, 42, "#e74c3c"),
)


def draw_icon(size: int = 64) -> Image.Image:
    """RGBA icon of size x size pixels (drawn at 4x and reduced, for smooth edges)"""
    big = size * 4
    k = big / 64.0
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(image)
    d.rounded_rectangle((2 * k, 2 * k, 62 * k - 1, 62 * k - 1), radius=12 * k, fill=BACKGROUND)
    for x, y, w, h, color in BARS:
        d.rounded_rectangle((x * k, y * k, (x + w) * k - 1, (y + h) * k - 1), radius=2 * k, fill=color)
    return image.resize((size, size), Image.LANCZOS)
