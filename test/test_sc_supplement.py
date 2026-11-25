"""Acceptance tests for the Noto Sans SC supplement (docs/FORK_CHANGES.md §2).

The fork's primary font is Noto Sans JP; the supplement holds ONLY the
simplified-Chinese codepoints JP lacks (GB2312 + TGH 2013 + the fork's
zh YAMLs/samples), and the SD-font pipeline merges it into the
NotoSansJP family as the converter fallback. These tests pin that
contract so a regenerated font cannot silently drift.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
JP_DIR = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansJP"
SC_DIR = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansSC"


def cmap(path):
    from fontTools.ttLib import TTFont
    return set(TTFont(str(path)).getBestCmap())


class ScSupplementTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.jp = cmap(JP_DIR / "NotoSansJP-Regular.ttf")
        cls.sc_reg = cmap(SC_DIR / "NotoSansSC-Regular.ttf")
        cls.sc_bold = cmap(SC_DIR / "NotoSansSC-Bold.ttf")
        listed = set()
        for line in (SC_DIR / "supplement_codepoints.txt").read_text(
                encoding="utf-8").splitlines():
            if line.startswith("U+"):
                listed.add(int(line.split()[0][2:], 16))
        cls.listed = listed

    def test_supplement_matches_recorded_codepoint_list(self):
        self.assertEqual(self.sc_reg, self.listed)
        self.assertEqual(self.sc_bold, self.listed)

    def test_supplement_is_disjoint_from_primary_font(self):
        # Supplementary by definition: nothing JP already has.
        self.assertFalse(self.sc_reg & self.jp)

    def test_known_gap_characters_are_covered(self):
        for ch in "这简认专业丛东丝丢严丧临为丽举义乌乐习乡书买":
            with self.subTest(char=ch):
                self.assertIn(ord(ch), self.sc_reg)

    def test_supplement_stays_small(self):
        # Full Noto Sans SC is ~17 MB variable; the supplement subsets
        # must stay near ~1 MB per weight.
        for name in ("NotoSansSC-Regular.ttf", "NotoSansSC-Bold.ttf"):
            size = (SC_DIR / name).stat().st_size
            self.assertLess(size, 2 * 1024 * 1024, f"{name}: {size} bytes")

    def test_of_license_present(self):
        self.assertIn("SIL Open Font License",
                      (SC_DIR / "OFL.txt").read_text(encoding="utf-8",
                                                     errors="replace"))

    def test_sd_fonts_yaml_registers_fallback(self):
        yaml_text = (ROOT / "lib/EpdFont/scripts/sd-fonts.yaml").read_text(
            encoding="utf-8")
        jp_block = yaml_text.split("- name: NotoSansJP\n", 1)[1].split(
            "\n  - name:", 1)[0]
        self.assertIn("fallback:", jp_block)
        # Since the Hangul supplement landed, the registered fallback is
        # the merged SC+KR face (which contains this SC supplement).
        self.assertIn("NotoSansSCKR/NotoSansSCKR-Regular.ttf", jp_block)
        self.assertIn("NotoSansSCKR/NotoSansSCKR-Bold.ttf", jp_block)
        self.assertIn("- name: NotoSansSCSupplement", yaml_text)
        # BMP supplement blocks beyond cjk/latin-ext must be in the
        # NotoSansJP intervals, or the converter never emits them.
        intervals = re.search(r"intervals: (.+)", jp_block).group(1)
        for token in ("greek", "cyrillic", "symbols", "(0x3400-0x4DBF)"):
            self.assertIn(token, intervals)


if __name__ == "__main__":
    unittest.main()
