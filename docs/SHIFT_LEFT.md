# Shift-Left Plan — 5-Language / Noto Sans JP / txt+md+epub+pdf / SD File Cache Fork

This fork was developed shift-left: the acceptance tests in this document were
written **before** the implementation, run red against the upstream tree, and
then driven green change by change. Host-side tests live under `test/` and run
in CI (`.github/workflows/shiftleft.yml`) before any firmware build.

## Requirements → acceptance tests

| # | Requirement | Acceptance test(s) | Where |
|---|-------------|--------------------|-------|
| R1 | UI languages are exactly ja / en / zh-Hans / zh-Hant / id | Only 5 YAML files exist; `_bcp47` set is exactly {en, ja, zh-Hans, zh-Hant, id}; `platformio.ini` `custom_i18n_builtin_langs` lists exactly those codes; every locale defines the two confirmation-question keys; `gen_i18n.py` generates code for exactly 5 languages | `test/test_supported_languages.py` (ctest: `supported_languages`) |
| R2 | Font unified on Noto Sans JP (OFL), CJK covered by the same font, no full glyph set resident in RAM | `NotoSansJP[wght].ttf` + `OFL.txt` present under `lib/EpdFont/builtinFonts/source/NotoSansJP/`; TTF name table says "Noto Sans JP"; subset script + SD-font catalog entry exist; subsets are loaded from SD by the existing SdCardFont pipeline, not compiled into RAM | `test/test_supported_languages.py::FontTest`, `docs/FORK_CHANGES.md` |
| R3a | txt keeps working (regression) | Existing `TxtToHtmlTest` passes unchanged; txt output has no Markdown markup applied | `test/txt_to_html/` |
| R3b | md gets real Markdown rendering | Headings `#`..`######` → `<h1>`..`<h6>`, `**bold**`, `*italic*`, `` `code` ``, fenced code blocks (escaped, no inline markup), `-`/`*` and `1.` lists, `[text](url)` links, `>` quotes, `---` rules; HTML in source is escaped; conversion is streaming (reader callback, line-buffered, bounded line length) | `test/markdown_to_html/` (ctest: `MarkdownToHtmlTest`) |
| R3c | epub keeps working (regression) | Existing host test suite (epub-adjacent: `content_opf_parser`, `chapter_html_slim_parser`, `zip_file`, `inflate_*`) passes | `test/` (existing) |
| R3d | pdf opens as a text book | `.pdf` recognised everywhere txt/epub are (`hasPdfExtension`, `hasReflowableBookExtension`, library index); text extraction handles literal `(…) Tj`, hex `<…> Tj`, `'…' TJ` arrays, `Td/TD/T*` line breaks, FlateDecode streams via the repo's miniz/uzlib-style inflate callback, escaped parens; extraction is page-streaming — the extractor never holds more than one content stream + one output page buffer; encrypted / non-PDF input fails cleanly | `test/pdf_text/` (ctest: `PdfTextTest`), `test/supported_formats/` (ctest: `SupportedFormatsTest`) |
| R4 | Cache lives on SD (user wording: "SSD"), LRU + size cap, RAM holds only the current page | Default cache root `/.crosspoint/cache` (configurable), default cap 64 MiB (configurable); LRU index persisted as a file in the cache dir; eviction removes least-recently-used entries first and never the entry just written; accounting is by on-disk file size; index survives restart (reload from file) | `test/file_cache/` (ctest: `FileCacheTest`) |

## Honest limitations (declared up front, not discovered later)

- **PDF is text-extraction only.** No image/vector rendering, no embedded-font
  ToUnicode CMap support in v1 (UTF-16BE hex strings with a BOM are decoded;
  other encoded glyphs may come out as raw bytes), no JavaScript/forms, and
  encrypted PDFs are rejected. Scanned-image PDFs produce little or no text.
  The alternative the task allowed — page-at-a-time rendering — is the layout
  the EPUB engine already applies to the extracted text: one page in RAM.
- **Markdown is a deliberate subset** (see R3b). No tables, no images, no
  nested lists beyond one level, no reference-style links, no raw HTML
  pass-through (source HTML is escaped).
- **New UI translations (ja / zh-Hans / zh-Hant) are partial**: a hand-translated
  core set; the i18n generator fills the remainder from English and warns.
  Coverage is printed by `gen_i18n.py` and recorded in `docs/FORK_CHANGES.md`.
  Indonesian and English were already complete/near-complete upstream.
- **Firmware build**: the full PlatformIO/ESP32 build needs the pioarduino
  platform download; what was verified in the development VM is recorded in
  `docs/FORK_CHANGES.md` (host tests + generator + syntax checks). Anything
  not run is listed as unverified there, never claimed as passed.

## CI gates (`.github/workflows/shiftleft.yml`)

1. Python acceptance tests (languages, fonts, package hygiene).
2. Host unit tests: CMake configure + build + `ctest` (includes the three new
   suites above plus the upstream suite).
3. `gen_i18n.py` regeneration must be a no-op diff for the generated headers
   that are checked in (guards against editing YAML without regenerating).
