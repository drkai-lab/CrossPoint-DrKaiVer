#!/usr/bin/env python3
"""Build the Noto Sans SC *supplement* fonts for this fork.

Why this exists
---------------
The fork's primary font is Noto Sans JP (see
scripts/build_noto_sans_jp_subsets.py). The JP subset's cmap (13,529
codepoints, Regular == Bold) does not contain a number of simplified-
Chinese characters — e.g. 这 U+8FD9, 简 U+7B80, 认 U+8BA4 — and on
device they rendered blank (found by the host emulator, docs/EMULATOR.md).
With the user's approval the gap is closed with a *supplementary* font:
Noto Sans SC is used **only** for codepoints Noto Sans JP lacks.

What the supplement contains (set-based, not sample-based)
----------------------------------------------------------
needed = GB2312 repertoire (enumerated through the stdlib gb2312 codec:
         7,445 codepoints incl. 6,763 hanzi)
       ∪ 通用規範漢字表 TGH 2013 (Unicode Unihan kTGH property:
         8,105 codepoints)
       ∪ every character used in this fork's zh-Hans / zh-Hant
         translation YAMLs and in the emulator sample books

supplement = needed − cmap(Noto Sans JP subset) ∩ cmap(Noto Sans SC)

Result at build time (2026-11-05): 3,332 of the needed codepoints are
missing from JP and every one of them is present in SC, so the default
supplement holds 3,333 codepoints (the Hans set above plus one
zh-Hant-YAML character) — see the generated
lib/EpdFont/builtinFonts/source/NotoSansSC/supplement_codepoints.txt,
which is the auditable record.

zh-Hant note: the Big5 repertoire was checked separately. 3,903 Big5
codepoints (3,570 of them hanzi, mostly rare/variant forms — the
common traditional repertoire is already in JP) are also missing from
JP; all but four *symbol* codepoints (U+02CD, U+203E, U+223C, U+2252 —
not hanzi, and absent from SC too) exist in SC. They are NOT in the
default supplement, to keep it at the Hans scope the user approved:
--include-big5 regenerates with them added (6,627 codepoints,
2,362,652 B per weight instead of 1,080,208 B).

Source (official Google Fonts distribution):
  lib/EpdFont/builtinFonts/source/NotoSansSC/NotoSansSC[wght].ttf
  (variable font) + OFL.txt (SIL Open Font License 1.1),
  downloaded from https://github.com/google/fonts/tree/main/ofl/notosanssc

Outputs static Regular (wght 400) and Bold (wght 700) TTFs holding ONLY
the supplement codepoints. Like the JP subsets they are never compiled
into firmware or loaded into RAM: the SD-card font pipeline merges them
into the NotoSansJP .cpfont family as the converter fallback
(lib/EpdFont/scripts/build-sd-fonts.py honours the family's
`fallback:` entry in sd-fonts.yaml; fontconvert_sdcard.py only draws a
glyph from the fallback face when the primary face has no glyph for
it), and SdCardFont pages glyphs in from SD one page at a time.

Usage: python3 scripts/build_noto_sans_sc_supplement.py
Requires: fonttools (pip install fonttools). The kTGH set is read from
a cached Unihan download when present; pass --unihan <path-to-
Unihan_OtherMappings.txt> or let the script download the Unicode
15.0.0 Unihan.zip (the last release shipping kTGH in that file).
"""
from pathlib import Path
import io
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
JP_DIR = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansJP"
SC_DIR = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansSC"
SC_SRC = SC_DIR / "NotoSansSC[wght].ttf"
UNIHAN_URL = "https://www.unicode.org/Public/15.0.0/ucd/Unihan.zip"
UNIHAN_CACHE = Path("/tmp/Unihan_OtherMappings.txt")

# Chinese sample lines from emulator/make_samples.py (kept in-repo so the
# set does not depend on the emulator folder; GB2312 already covers them,
# this is a belt-and-braces guard).
SAMPLE_TEXT = (
    "这是简体中文的示例文本。这是简体中文的段落。"
    "简体中文页面第二页的内容，用于确认翻页。"
    "這是繁體中文的範例文字。這是繁體中文的段落。繁體中文頁面"
)


def codec_repertoire(name: str, his, los) -> set:
    out = set()
    for hi in his:
        for lo in los:
            try:
                ch = bytes([hi, lo]).decode(name)
            except Exception:
                continue
            if len(ch) == 1:
                out.add(ord(ch))
    return out


def gb2312() -> set:
    return codec_repertoire("gb2312", range(0xA1, 0xF8), range(0xA1, 0xFF))


def big5() -> set:
    los = list(range(0x40, 0x7F)) + list(range(0xA1, 0xFF))
    return codec_repertoire("big5", range(0x81, 0xFF), los)


def tgh2013() -> set:
    path = UNIHAN_CACHE
    if not path.exists():
        print(f"Downloading {UNIHAN_URL} for the kTGH (TGH 2013) set...")
        data = urllib.request.urlopen(UNIHAN_URL).read()
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            path.write_bytes(z.read("Unihan_OtherMappings.txt"))
    out = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split("\t")
        if len(parts) >= 3 and parts[1] == "kTGH":
            out.add(int(parts[0].replace("U+", ""), 16))
    if len(out) < 8000:
        raise RuntimeError(f"kTGH set looks wrong: {len(out)} codepoints")
    return out


def yaml_chars() -> set:
    out = set()
    tdir = ROOT / "lib/I18n/translations"
    for name in ("chinese_simplified.yaml", "chinese_traditional.yaml"):
        text = (tdir / name).read_text(encoding="utf-8")
        out |= {ord(c) for c in text if ord(c) > 0x2000}
    return out


def compute_supplement(include_big5: bool = False):
    from fontTools.ttLib import TTFont

    jp = set(TTFont(str(JP_DIR / "NotoSansJP-Regular.ttf")).getBestCmap())
    jp_bold = set(TTFont(str(JP_DIR / "NotoSansJP-Bold.ttf")).getBestCmap())
    assert jp == jp_bold, "JP Regular/Bold cmaps differ; revisit supplement"
    sc = set(TTFont(str(SC_SRC)).getBestCmap())

    needed = gb2312() | tgh2013() | yaml_chars() | {ord(c) for c in SAMPLE_TEXT}
    if include_big5:
        needed |= big5()
    missing = needed - jp
    supplement = sorted(c for c in missing if c in sc)
    uncovered = sorted(c for c in missing if c not in sc)
    return supplement, {
        "jp_cmap": len(jp),
        "needed": len(needed),
        "missing_from_jp": len(missing),
        "uncovered_by_sc": uncovered,
    }


def build(weight: int, out_name: str, unicodes) -> Path:
    from fontTools import subset
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont

    font = TTFont(str(SC_SRC))
    instantiateVariableFont(font, {"wght": weight}, inplace=True, updateFontNames=True)
    tmp = SC_DIR / f"_instance_{weight}.ttf"
    font.save(tmp)

    out = SC_DIR / out_name
    subset.main([
        str(tmp),
        f"--output-file={out}",
        f"--unicodes={','.join(hex(u) for u in unicodes)}",
        "--layout-features=*",
        "--no-hinting",
        "--name-IDs=*",
    ])
    tmp.unlink()
    return out


def main() -> int:
    include_big5 = "--include-big5" in sys.argv
    if not SC_SRC.exists():
        print(f"Missing source font: {SC_SRC}", file=sys.stderr)
        return 1
    supplement, stats = compute_supplement(include_big5)
    print(f"JP cmap: {stats['jp_cmap']} codepoints")
    print(f"Needed repertoire (GB2312 + TGH2013"
          f"{' + Big5' if include_big5 else ''} + fork YAML/samples): "
          f"{stats['needed']} codepoints")
    print(f"Missing from Noto Sans JP: {stats['missing_from_jp']}")
    print(f"Supplement (missing and present in Noto Sans SC): {len(supplement)}")
    uncovered = stats["uncovered_by_sc"]
    print(f"Missing from JP and also absent from SC: {len(uncovered)} "
          f"({', '.join(f'U+{c:04X}' for c in uncovered)})")

    list_path = SC_DIR / "supplement_codepoints.txt"
    with list_path.open("w", encoding="utf-8") as f:
        f.write("# Codepoints in the Noto Sans SC supplement: present in the\n")
        f.write("# GB2312 / TGH-2013 repertoires or in this fork's zh-Hans/zh-Hant\n")
        f.write("# YAMLs and samples, ABSENT from the Noto Sans JP subset, and\n")
        f.write("# present in Noto Sans SC.\n")
        f.write("# Generated by scripts/build_noto_sans_sc_supplement.py — do not edit.\n")
        for cp in supplement:
            f.write(f"U+{cp:04X} {chr(cp)}\n")
    print(f"Wrote {list_path}")

    for weight, name in ((400, "NotoSansSC-Regular.ttf"), (700, "NotoSansSC-Bold.ttf")):
        out = build(weight, name, supplement)
        print(f"{name}: {out.stat().st_size} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
