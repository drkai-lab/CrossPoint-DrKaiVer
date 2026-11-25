#!/usr/bin/env python3
"""Build static, subset Noto Sans JP fonts for this fork.

Source (official Google Fonts distribution):
  lib/EpdFont/builtinFonts/source/NotoSansJP/NotoSansJP[wght].ttf
  (variable font, wght 100..900) + OFL.txt (SIL Open Font License 1.1),
  downloaded from https://github.com/google/fonts/tree/main/ofl/notosansjp

Outputs static Regular (wght 400) and Bold (wght 700) TTFs subset to the
scripts this fork ships: Latin (en/id), Hiragana/Katakana, fullwidth forms,
and CJK Unified Ideographs (ja / zh-Hans / zh-Hant share this block).
The full variable font is ~9.2 MB and is never compiled into firmware or
loaded into RAM: the subsets are converted with the repo's existing
SD-card font pipeline (fontconvert_sdcard.py) and paged from SD by
SdCardFont, one glyph range at a time.

Usage: python3 scripts/build_noto_sans_jp_subsets.py
Requires: fonttools (pip install fonttools).
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansJP"
SRC = SRC_DIR / "NotoSansJP[wght].ttf"

UNICODE_RANGES = (
    list(range(0x0020, 0x00FF + 1))          # Basic Latin + Latin-1 Supplement (en, id)
    + list(range(0x2000, 0x206F + 1))        # General punctuation
    + list(range(0x3000, 0x30FF + 1))        # CJK punctuation, Hiragana, Katakana
    + list(range(0x3400, 0x4DBF + 1))        # CJK Ext A
    + list(range(0x4E00, 0x9FFF + 1))        # CJK Unified Ideographs (ja, zh-Hans, zh-Hant)
    + list(range(0xFF00, 0xFF65 + 1))        # Fullwidth forms
)


def build(weight: int, out_name: str) -> Path:
    from fontTools import subset
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont

    font = TTFont(SRC)
    instantiateVariableFont(font, {"wght": weight}, inplace=True, updateFontNames=True)
    tmp = SRC_DIR / f"_instance_{weight}.ttf"
    font.save(tmp)

    out = SRC_DIR / out_name
    subset.main([
        str(tmp),
        f"--output-file={out}",
        f"--unicodes={','.join(hex(u) for u in UNICODE_RANGES)}",
        "--layout-features=*",
        "--no-hinting",
    ])
    tmp.unlink()
    return out


def main() -> int:
    if not SRC.exists():
        print(f"Missing source font: {SRC}", file=sys.stderr)
        return 1
    for weight, name in ((400, "NotoSansJP-Regular.ttf"), (700, "NotoSansJP-Bold.ttf")):
        out = build(weight, name)
        print(f"{name}: {out.stat().st_size} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
