"""Acceptance tests for the fork's language/font scope (docs/SHIFT_LEFT.md R1/R2).

Shift-left: these were written before the language reduction was implemented.
"""
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from gen_i18n import parse_yaml_file  # noqa: E402

EXPECTED = {  # bcp47 -> _language_code
    "en": "EN",
    "ja": "JA",
    "zh-Hans": "ZHS",
    "zh-Hant": "ZHT",
    "id": "ID",
}


class SupportedLanguagesTest(unittest.TestCase):
    def test_exactly_five_translation_files_with_expected_bcp47(self):
        directory = ROOT / "lib/I18n/translations"
        files = sorted(directory.glob("*.yaml"))
        self.assertEqual(len(files), 5, [f.name for f in files])
        seen = {}
        for path in files:
            values = parse_yaml_file(str(path))
            seen[values["_bcp47"]] = values["_language_code"]
        self.assertEqual(seen, EXPECTED)

    def test_every_locale_defines_confirmation_questions(self):
        directory = ROOT / "lib/I18n/translations"
        for path in directory.glob("*.yaml"):
            values = parse_yaml_file(str(path))
            for key in ("STR_DELETE_ITEM_CONFIRM", "STR_SET_SLEEP_COVER_CONFIRM"):
                with self.subTest(language=path.stem, key=key):
                    self.assertTrue(values[key])
                    self.assertEqual(values[key], values[key].strip())

    def test_platformio_compiles_in_exactly_the_five_languages(self):
        ini = (ROOT / "platformio.ini").read_text()
        match = re.search(r"^custom_i18n_builtin_langs\s*=\s*(\S+)", ini, re.M)
        self.assertIsNotNone(match)
        codes = {c.strip().upper() for c in match.group(1).split(",")}
        self.assertEqual(codes, {code.upper() for code in EXPECTED.values()})


class NotoSansJpFontTest(unittest.TestCase):
    def test_variable_font_and_ofl_present(self):
        base = ROOT / "lib/EpdFont/builtinFonts/source/NotoSansJP"
        font = base / "NotoSansJP[wght].ttf"
        self.assertTrue(font.exists(), "Noto Sans JP variable font missing")
        self.assertGreater(font.stat().st_size, 1_000_000)
        data = font.read_bytes()
        # Name table / OS2 must identify the family; the full name string is UTF-16BE in TTF.
        self.assertIn("Noto Sans JP".encode("utf-16-be"), data)
        ofl = (base / "OFL.txt").read_text(errors="replace")
        self.assertIn("SIL Open Font License", ofl)

    def test_subset_pipeline_and_catalog_entry_exist(self):
        self.assertTrue((ROOT / "scripts/build_noto_sans_jp_subsets.py").exists())
        catalog = (ROOT / "lib/EpdFont/scripts/sd-fonts.yaml").read_text()
        self.assertIn("NotoSansJP", catalog)


if __name__ == "__main__":
    unittest.main()
