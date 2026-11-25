#!/usr/bin/env python3
"""Build the Noto Sans KR *supplement* and the merged SC+KR fallback face.

Mirrors scripts/build_noto_sans_sc_supplement.py. The fork's primary font
is Noto Sans JP; the SC supplement covers only simplified-Chinese glyphs
JP lacks. Korean text (book bodies and file names) needs the same
treatment for Hangul:

    supplement = cmap(Noto Sans KR) - cmap(Noto Sans JP subset)
                 - cmap(Noto Sans SC supplement)

Hangul syllables/jamo are the bulk; KR's Han glyphs mostly duplicate
SC/JP and are excluded, keeping the supplement disjoint from both
existing faces. Result at build time (2026-11-25): 14,538 codepoints,
including all 11,172 Hangul syllables.

fontconvert_sdcard.py takes a single fallback face per style, so the
supplement is merged with the SC supplement (disjoint cmaps) into one
fallback face per weight: NotoSansSCKR-Regular/Bold.ttf (17,869
codepoints), which sd-fonts.yaml registers as the NotoSansJP fallback.

Inputs / outputs
----------------
Input:  the official Noto Sans KR variable font (Google Fonts,
        ofl/notosanskr/NotoSansKR[wght].ttf). Point KR_SRC at it
        (env var) or pass its path as argv[1]; license: OFL.txt beside
        the outputs (SIL OFL 1.1, Copyright Google).
Output: lib/EpdFont/builtinFonts/source/NotoSansSCKR/
        (NotoSansKR-Supplement-*.ttf, NotoSansSCKR-*.ttf,
        kr_supplement_codepoints.txt — the auditable record).
"""
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
JP_DIR = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansJP"
SC_DIR = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansSC"
OUT_DIR = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansSCKR"
KR_SRC = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(
    os.environ.get("KR_SRC", "/tmp/NotoSansKR[wght].ttf"))


def cmap(path):
    from fontTools.ttLib import TTFont
    return set(TTFont(str(path)).getBestCmap())


def instance(weight, out):
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont
    font = TTFont(str(KR_SRC))
    instantiateVariableFont(font, {"wght": weight}, inplace=True, updateFontNames=True)
    font.save(str(out))
    return out


def subset_font(src, codepoints, out):
    from fontTools import subset
    subset.main([
        str(src),
        f"--output-file={out}",
        f"--unicodes={','.join(hex(u) for u in sorted(codepoints))}",
        "--layout-features=*",
        "--no-hinting",
    ])


def main():
    jp_reg = cmap(JP_DIR / "NotoSansJP-Regular.ttf")
    jp_bold = cmap(JP_DIR / "NotoSansJP-Bold.ttf")
    sc_reg = cmap(SC_DIR / "NotoSansSC-Regular.ttf")
    sc_bold = cmap(SC_DIR / "NotoSansSC-Bold.ttf")
    assert jp_reg == jp_bold, "JP Regular/Bold cmap mismatch"
    assert sc_reg == sc_bold, "SC Regular/Bold cmap mismatch"

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    results = {}
    for weight, name in ((400, "Regular"), (700, "Bold")):
        inst = OUT_DIR / f"_kr_instance_{weight}.ttf"
        instance(weight, inst)
        kr = cmap(inst)
        supp = kr - jp_reg - sc_reg
        print(f"{name}: KR cmap={len(kr)} JP-missing={len(kr - jp_reg)} "
              f"after SC exclusion={len(supp)}")
        hangul = [u for u in supp if 0xAC00 <= u <= 0xD7A3]
        jamo = [u for u in supp if 0x1100 <= u <= 0x11FF or 0x3130 <= u <= 0x318F
                or 0xA960 <= u <= 0xA97F or 0xD7B0 <= u <= 0xD7FF]
        print(f"  Hangul syllables={len(hangul)} jamo={len(jamo)} other={len(supp)-len(hangul)-len(jamo)}")
        out = OUT_DIR / f"NotoSansKR-Supplement-{name}.ttf"
        subset_font(inst, supp, out)
        # Subsetter may keep a couple of glyphs referenced by composites
        # (e.g. U+2208, U+224C); require a superset, not exact equality.
        actual = cmap(out)
        assert supp <= actual, f"subset lost codepoints {name}: {sorted(supp - actual)[:5]}"
        print(f"  subset kept {len(actual) - len(supp)} extra referenced codepoints")
        results[name] = (actual, out)
        inst.unlink()

    supp = results["Regular"][0]
    assert supp == results["Bold"][0], "KR supplement Regular/Bold mismatch"
    (OUT_DIR / "kr_supplement_codepoints.txt").write_text(
        "".join(f"U+{u:04X}\n" for u in sorted(supp)), encoding="utf-8")
    print(f"supplement total: {len(supp)} codepoints")

    # Merge SC supplement + KR supplement into one fallback face per weight.
    from fontTools.merge import Merger
    for name in ("Regular", "Bold"):
        sc = SC_DIR / f"NotoSansSC-{name}.ttf"
        kr = OUT_DIR / f"NotoSansKR-Supplement-{name}.ttf"
        out = OUT_DIR / f"NotoSansSCKR-{name}.ttf"
        merger = Merger()
        merged = merger.merge([str(sc), str(kr)])
        merged.save(str(out))
        merged_cmap = cmap(out)
        expected = sc_reg | supp
        print(f"merged {name}: cmap={len(merged_cmap)} expected={len(expected)} "
              f"match={merged_cmap == expected}")
        assert merged_cmap == expected, f"merged cmap mismatch {name}"
    print("OK")


if __name__ == "__main__":
    sys.exit(main())
