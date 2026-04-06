from pathlib import Path
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
OUT = ASSETS / "app_icon.ico"


def make(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    def rr(box, radius, fill, outline=None, width=1):
        d.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)

    rr((size * 0.08, size * 0.08, size * 0.92, size * 0.92), size * 0.18, (15, 23, 42, 255))
    rr((size * 0.15, size * 0.15, size * 0.85, size * 0.85), size * 0.14, None, (37, 99, 235, 255), max(1, size // 20))
    d.ellipse((size * 0.24, size * 0.24, size * 0.76, size * 0.76), fill=(219, 234, 254, 255))
    d.rounded_rectangle((size * 0.33, size * 0.31, size * 0.67, size * 0.69), radius=size * 0.07, fill=(248, 250, 252, 240))
    d.polygon([(size * 0.47, size * 0.40), (size * 0.59, size * 0.50), (size * 0.47, size * 0.60)], fill=(245, 158, 11, 255))
    d.ellipse((size * 0.39, size * 0.37, size * 0.44, size * 0.42), fill=(96, 165, 250, 255))
    d.ellipse((size * 0.54, size * 0.58, size * 0.58, size * 0.62), fill=(16, 185, 129, 255))
    d.arc((size * 0.21, size * 0.53, size * 0.79, size * 0.83), 15, 165, fill=(147, 197, 253, 220), width=max(1, size // 30))
    return img


def main():
    images = [make(s) for s in (256, 128, 64, 48, 32, 16)]
    images[0].save(OUT, format="ICO", sizes=[(im.width, im.height) for im in images])


if __name__ == "__main__":
    main()
