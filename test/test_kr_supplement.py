"""Acceptance tests for the Noto Sans KR supplement + merged SC+KR fallback.

The fork's primary font is Noto Sans JP; the SC supplement holds the
simplified-Chinese codepoints JP lacks, and the KR supplement holds the
codepoints (mostly Hangul) that both JP and SC lack. Because the SD-font
converter takes a single fallback face per style, the two supplements
are merged into NotoSansSCKR, which sd-fonts.yaml registers as the
NotoSansJP fallback. These tests pin that contract.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
JP_DIR = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansJP"
SC_DIR = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansSC"
SCKR_DIR = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansSCKR"


def cmap(path):
    from fontTools.ttLib import TTFont
    return set(TTFont(str(path)).getBestCmap())


class KrSupplementTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.jp = cmap(JP_DIR / "NotoSansJP-Regular.ttf")
        cls.sc = cmap(SC_DIR / "NotoSansSC-Regular.ttf")
        cls.kr_reg = cmap(SCKR_DIR / "NotoSansKR-Supplement-Regular.ttf")
        cls.kr_bold = cmap(SCKR_DIR / "NotoSansKR-Supplement-Bold.ttf")
        cls.merged_reg = cmap(SCKR_DIR / "NotoSansSCKR-Regular.ttf")
        cls.merged_bold = cmap(SCKR_DIR / "NotoSansSCKR-Bold.ttf")
        listed = set()
        for line in (SCKR_DIR / "kr_supplement_codepoints.txt").read_text(
                encoding="utf-8").splitlines():
            if line.startswith("U+"):
                listed.add(int(line.split()[0][2:], 16))
        cls.listed = listed

    def test_supplement_matches_recorded_codepoint_list(self):
        self.assertEqual(self.kr_reg, self.listed)
        self.assertEqual(self.kr_bold, self.listed)

    def test_supplement_is_disjoint_from_jp_and_sc(self):
        # The subsetter keeps two glyphs referenced by composites
        # (U+2208, U+224C) that JP and SC already have; duplicates are
        # harmless because the converter only draws fallback glyphs
        # the primary face lacks.
        self.assertLessEqual(self.kr_reg & self.jp, {0x2208, 0x224C})
        self.assertLessEqual(self.kr_reg & self.sc, {0x2208, 0x224C})

    def test_all_hangul_syllables_covered(self):
        syllables = set(range(0xAC00, 0xD7A4))
        self.assertEqual(syllables - self.kr_reg, set())

    def test_merged_fallback_is_sc_union_kr(self):
        self.assertEqual(self.merged_reg, self.sc | self.kr_reg)
        self.assertEqual(self.merged_bold, self.sc | self.kr_reg)

    def test_of_license_present(self):
        self.assertIn("SIL Open Font License",
                      (SCKR_DIR / "OFL.txt").read_text(encoding="utf-8",
                                                       errors="replace"))

    def test_sd_fonts_yaml_registers_merged_fallback(self):
        yaml_text = (ROOT / "lib/EpdFont/scripts/sd-fonts.yaml").read_text(
            encoding="utf-8")
        jp_block = yaml_text.split("- name: NotoSansJP\n", 1)[1].split(
            "\n  - name:", 1)[0]
        self.assertIn("NotoSansSCKR/NotoSansSCKR-Regular.ttf", jp_block)
        self.assertIn("NotoSansSCKR/NotoSansSCKR-Bold.ttf", jp_block)
        # UI text draws at 8/10pt too; those sizes must be generated or
        # small UI strings lose their CJK fallback on device.
        self.assertIn("sizes: [8, 10, 12, 14, 16, 18]", jp_block)
        self.assertIn("hangul", jp_block)


if __name__ == "__main__":
    unittest.main()
