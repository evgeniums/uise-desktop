#!/usr/bin/env python3
"""
Render-based screen for the QtSvg black-silhouette hazard (see thirdparty/noto-emoji/README.md).

A grep for "<use" over-flags files that render correctly in QtSvg despite containing the tag
(known cases in the shipped 54: wink-tongue U+1F61C, roll-eyes U+1F644) -- only some <use>
constructions actually defeat QtSvg's resolver. The documented, definitive test is a render:
measure the fraction of opaque pixels that are near-black. A broken hand measures 45-72%, a
correct icon is ~0%, and U+1F60E (sunglasses) is a legitimate ~33% outlier.

Requires PySide6 (a throwaway venv is fine -- this never touches the C++ build).

Usage: python3 screen_svgs.py <svg-dir>   # screens every emoji_u*.svg in the directory
"""
import sys
import os
import glob

from PySide6.QtCore import QSize
from PySide6.QtGui import QImage, QPainter, qAlpha, qRed, qGreen, qBlue
from PySide6.QtSvg import QSvgRenderer
from PySide6.QtWidgets import QApplication

RENDER_SIZE = 128
NEAR_BLACK_THRESHOLD = 40      # max R/G/B value to count as "near black"
BROKEN_FRACTION_MIN = 0.40     # comfortably below the documented 45% floor for a broken render
KNOWN_DARK_OK = {
    0x1F60E,    # sunglasses -- legitimately ~33% near-black
    0x1F576,    # dark sunglasses -- legitimately ~40%+, visually verified 2026-09-16
    0x1F4F1,    # mobile phone -- black device body behind a colourful screen
    0x1F4F2,    # mobile phone with arrow -- same device art
    0x1F4F3,    # vibration mode -- same device art
}


def near_black_fraction(path):
    renderer = QSvgRenderer(path)
    if not renderer.isValid():
        return None, "QSvgRenderer reports invalid"
    img = QImage(QSize(RENDER_SIZE, RENDER_SIZE), QImage.Format.Format_ARGB32)
    img.fill(0)
    painter = QPainter(img)
    renderer.render(painter)
    painter.end()

    opaque = 0
    near_black = 0
    for y in range(RENDER_SIZE):
        for x in range(RENDER_SIZE):
            px = img.pixel(x, y)
            a = qAlpha(px)
            if a < 16:
                continue
            opaque += 1
            r, g, b = qRed(px), qGreen(px), qBlue(px)
            if r <= NEAR_BLACK_THRESHOLD and g <= NEAR_BLACK_THRESHOLD and b <= NEAR_BLACK_THRESHOLD:
                near_black += 1
    if opaque == 0:
        return 0.0, None
    return near_black / opaque, None


def main():
    svg_dir = sys.argv[1]
    app = QApplication.instance() or QApplication([])

    files = sorted(glob.glob(os.path.join(svg_dir, "emoji_u*.svg")))
    broken = []
    errors = []
    for path in files:
        name = os.path.basename(path)
        m = name[len("emoji_u"):-len(".svg")]
        try:
            cp = int(m, 16)
        except ValueError:
            cp = None
        frac, err = near_black_fraction(path)
        if err:
            errors.append((name, err))
            continue
        if frac >= BROKEN_FRACTION_MIN and cp not in KNOWN_DARK_OK:
            broken.append((name, frac))

    print("Screened %d SVGs (render size %dx%d)." % (len(files), RENDER_SIZE, RENDER_SIZE))
    if errors:
        print("%d file(s) QSvgRenderer could not even load:" % len(errors))
        for name, err in errors:
            print("  %s -- %s" % (name, err))
    if broken:
        print("%d file(s) FAIL the black-silhouette screen (near-black fraction >= %.0f%%):"
              % (len(broken), BROKEN_FRACTION_MIN * 100))
        for name, frac in sorted(broken, key=lambda x: -x[1]):
            print("  %-24s %.1f%% near-black" % (name, frac * 100))
        sys.exit(1)
    print("PASS: no broken renders detected.")


if __name__ == "__main__":
    main()
