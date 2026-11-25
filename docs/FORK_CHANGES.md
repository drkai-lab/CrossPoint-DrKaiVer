# Fork Changes — 5 languages / Noto Sans JP / txt+md+epub+pdf / SD file cache

Fork of https://github.com/crosspoint-reader/crosspoint-reader
(upstream commit `8503b7f`), developed shift-left: see `docs/SHIFT_LEFT.md`
for the requirement → test map. Published by the owner as
**CrossPoint-DrKaiVer** (2026-11-25); the changes below were committed
to the owner's fork on the owner's explicit instruction.

## 1. Languages: exactly ja / en / zh-Hans / zh-Hant / id

- Deleted 30 translation files under `lib/I18n/translations/`; kept
  `english.yaml` (EN) and `indonesia.yaml` (ID); added `japanese.yaml`
  (JA, `ja`), `chinese_simplified.yaml` (ZHS, `zh-Hans`) and
  `chinese_traditional.yaml` (ZHT, `zh-Hant`) — upstream had **no**
  Japanese or Chinese UI translation at all.
- `platformio.ini`: `custom_i18n_builtin_langs = en,ja,zhs,zht,id`, so the
  language picker only ever offers these five; a saved language that is
  not built in falls back to English (existing upstream behaviour).
- New locales carry a hand-translated core of 52 keys (boot, home, reader
  errors, Wi-Fi, settings categories, both confirmation questions, the two
  new cache settings). The generator fills the remaining 606 of 658 keys
  from English and warns — coverage: ja/zh-Hans/zh-Hant 52/658 keys
  translated, id 442/658 (upstream state), en is the reference.
- `scripts/gen_i18n.py` output: "Languages: 5", 28,770 B total string
  flash (vs. 34 languages upstream).
- Python tests that named removed locales (`test_confirmation_translations.py`,
  `test_translation_spacing.py`) were updated to the shipped locales.

## 2. Font: Noto Sans JP everywhere

- Source: official Google Fonts repo (`google/fonts`, `ofl/notosansjp`):
  `NotoSansJP[wght].ttf` variable font, 9,589,900 bytes;
  license `OFL.txt` (SIL OFL 1.1, Copyright 2014-2021 Adobe) ships beside it
  in `lib/EpdFont/builtinFonts/source/NotoSansJP/`.
- `scripts/build_noto_sans_jp_subsets.py` instances it at wght 400/700 and
  subsets to Latin + Latin-1, general punctuation, kana, CJK Ext-A +
  Unified Ideographs and fullwidth forms → `NotoSansJP-Regular.ttf`
  (5,228,024 B) and `NotoSansJP-Bold.ttf` (5,223,132 B). One family covers
  en/id (Latin), ja (kana + kanji) and zh-Hans/zh-Hant (unified ideographs);
  verified with fontTools: cmap contains U+3042, U+30A2, U+65E5, U+8A9E,
  U+7E41; OS/2 weights 400/700; family name "Noto Sans JP".
- The subsets are **not** compiled into firmware and never fully loaded
  into RAM: they are registered in `lib/EpdFont/scripts/sd-fonts.yaml`
  (family `NotoSansJP`, sizes 12/14/16/18) so the existing SD-card font
  pipeline (`fontconvert_sdcard.py` → `.cpfont` ranges → `SdCardFont`
  paging from SD) serves glyphs from the card, exactly like upstream CJK
  SD fonts. The pre-existing Latin builtin fonts remain as the fallback
  UI font; book text in the five languages should use NotoSansJP from SD.

### 2a. Simplified-Chinese supplement: Noto Sans SC (user-approved)

The host emulator found that Noto Sans JP itself lacks some
simplified-Chinese glyphs (这 U+8FD9, 简 U+7B80, 认 U+8BA4 rendered
blank — see `docs/EMULATOR.md`). With the user's approval, the gap is
closed by a **supplementary** font while Noto Sans JP stays primary:

- **Gap analysis (set-based).** Needed repertoire = GB2312 (7,445
  codepoints, stdlib codec enumeration) ∪ 通用規範漢字表 TGH 2013
  (8,105, Unicode Unihan `kTGH`) ∪ this fork's zh-Hans/zh-Hant YAMLs
  and the emulator samples = 8,962 codepoints. Of those, **3,333 are
  absent from the JP subset** (Regular == Bold cmap, 13,529) — among
  them 2,769 hanzi in the Hans set — and **every one of the 3,333 is
  present in Noto Sans SC**, so coverage is now complete. The auditable
  list is `lib/EpdFont/builtinFonts/source/NotoSansSC/supplement_codepoints.txt`.
- **Supplement font.** Official Google Fonts `google/fonts`
  (`ofl/notosanssc`): variable font 17,772,300 B + `OFL.txt`, kept in
  `lib/EpdFont/builtinFonts/source/NotoSansSC/`.
  `scripts/build_noto_sans_sc_supplement.py` instances it at wght
  400/700 and subsets to *only* the 3,333 JP-missing codepoints →
  `NotoSansSC-Regular.ttf` **1,080,208 B** and `NotoSansSC-Bold.ttf`
  **1,079,268 B** (~1 MB per weight, vs ~17 MB for the full font).
- **Fallback mechanism (font synthesis at the existing injection
  point).** The renderer pairs every glyph with its own font's data
  (`GfxRenderer::getGlyphBitmap` indexes `fontData->bitmap` by the
  glyph's offset, and SD fonts resolve through their own miss
  context), so a runtime cross-font glyph swap inside
  `EpdFontFamily` would corrupt bitmap lookup; the repo already has
  the correct seam instead: `fontconvert_sdcard.py`'s per-style
  `--fallback-*` face, whose `load_glyph()` uses the fallback face
  **only when the primary face has no glyph** for a codepoint (and
  whose interval validation keeps a codepoint when either face has
  it). Changes: `lib/EpdFont/scripts/build-sd-fonts.py` now honours a
  per-family `fallback:` entry in `sd-fonts.yaml` (other families keep
  the historical NotoSans default); the `NotoSansJP` family declares
  the SC supplement as its Regular/Bold fallback and its intervals
  were extended with the blocks the supplement lives in
  (`greek,cyrillic,symbols` — GB2312 symbol rows — plus
  `(0x2312-0x2312),(0x2460-0x24FF),(0x3100-0x312F),(0x3220-0x3229),
  (0x3400-0x4DBF)`), and a `NotoSansSCSupplement` family is registered
  so the supplement ships in the font manifest (it is a fallback, not
  a standalone reading font). The merged `.cpfont` therefore contains
  JP glyphs for everything JP has and SC glyphs for the rest, in ONE
  `EpdFontData` — glyph/data pairing, the section/page cache keys and
  `SdCardFont` SD paging are unchanged, and no second font is loaded
  into RAM on device.
- **Verified by generation.** `build-sd-fonts.py --only
  NotoSansJP,NotoSansSCSupplement` succeeds; each `NotoSansJP_*.cpfont`
  style validates **16,671 glyphs** (JP's 13,529 + the BMP supplement +
  converter-added ranges) and parsing `NotoSansJP_12.cpfont` back
  shows real bitmaps (non-zero data length) for 这/简/认/专/业/丛/东/㈠/ā
  in both Regular and Bold. `test/test_sc_supplement.py` (6 tests,
  wired into `.github/workflows/shiftleft.yml`) pins the contract:
  supplement cmap == the recorded list, disjoint from JP, known-gap
  characters covered, size < 2 MB, YAML fallback registered.
- **Remaining limits.** (a) 196 supplement codepoints are above the
  BMP (CJK Ext B/H, TGH level-3 rarities); the supplement TTFs contain
  them but the `.cpfont` pipeline — like the JP subset itself — is
  BMP-scoped, so on device they still cannot be served. (b) zh-Hant:
  Big5 was checked — 3,903 codepoints (3,570 hanzi, mostly rare/
  variant forms; the common traditional repertoire is already in JP,
  and the zh-Hant YAML needs exactly one supplement character, which
  is included) are absent from JP; all but four *symbol* codepoints
  (U+02CD, U+203E, U+223C, U+2252 — absent from SC as well, not hanzi)
  exist in SC but are deliberately **not** in the default supplement,
  to keep it at the approved Hans scope/size. Regenerating with
  `build_noto_sans_sc_supplement.py --include-big5` widens it to
  6,627 codepoints at 2,362,652 B per weight if that trade-off is
  ever wanted.

### 2b. UI font sizes 8/10pt and Hangul supplement (2026-11-25, real-device finding)

First real-device run (Xteink X4) showed book bodies and large UI text
rendering Japanese correctly, while file names, book titles and small
button labels rendered as U+FFFD: the UI draws small text at 8/10pt,
and the UI fallback only registers when the SD family ships a .cpfont
at the *same* size — the kit shipped only 12/14/16/18pt. Two changes:

- `sd-fonts.yaml`: the `NotoSansJP` family now generates sizes
  **[8, 10, 12, 14, 16, 18]**. Pipeline verified by regenerating 12pt
  byte-identically to the shipped file.
- **Hangul supplement (Noto Sans KR)**, same set-based approach as §2a:
  `supplement = cmap(Noto Sans KR) − cmap(JP) − cmap(SC supplement)`
  = 14,538 codepoints, including all 11,172 Hangul syllables
  (`scripts/build_noto_sans_kr_supplement.py`). The converter accepts a
  single fallback face per style, so the SC and KR supplements are
  merged into `NotoSansSCKR-Regular/Bold.ttf` (17,869 codepoints) in
  `lib/EpdFont/builtinFonts/source/NotoSansSCKR/`, now the registered
  fallback; `hangul` was added to the family's intervals.
  `test/test_kr_supplement.py` pins the contract (recorded codepoint
  list, disjointness, full Hangul syllable coverage, merged cmap ==
  SC ∪ KR, YAML registration). This adds Korean *glyphs* for book text
  and file names only — Korean is not a UI language; the five-language
  scope of §1 is unchanged. Host-side .cpfont parsing verified real
  bitmaps for Hangul/这/简/认/kana/kanji at every size; on-device
  display is pending the owner's check.

## 3. Formats: txt, md, epub, pdf

- **txt** — unchanged pipeline (`TxtToHtml`); existing tests pass.
- **epub** — unchanged engine; existing host tests cover it.
- **md — newly implemented for real.** Upstream recognised `.md` but
  rendered it as plain text. `lib/Txt/MarkdownToHtml.{h,cpp}` is a
  streaming, line-buffered converter (reader callback, 8 KiB in-chunk,
  lines capped at 4 KiB) wired into `TxtToHtml::stream()` for `.md`;
  `MD_CACHE_VERSION` bumped 1 → 2 so old plain-text caches invalidate.
  Subset: ATX headings, `**bold**`, `*italic*`/`_italic_`, `` `code` ``,
  fenced code (escaped, literal), `-`/`*` and `1.` lists, `[text](url)`,
  `>` quotes, `---` rules. Raw HTML is escaped. No tables/images/nesting.
- **pdf — new, text-extraction only.** `lib/Pdf/PdfText.{h,cpp}` is a
  streaming extractor: chunked scan pass (page count from `/Type /Page`,
  `/Encrypt` rejected), then a stream pass holding at most one content
  stream (dict look-back 1 KiB, raw-stream cap 4 MiB, FlateDecode cap
  512 KiB compressed / 4 MiB inflated via injected inflate — miniz
  `InflateStream` on device, stored-block inflater in tests). Supports
  literal/hex strings, `Tj`/`TJ`/`'`/`"`, `Td`/`TD`/`T*` line breaks,
  UTF-16BE-with-BOM strings. `lib/Pdf/Pdf.{h,cpp}` glues it into the
  reader: extraction writes `pdf_text.txt` into the book's SD cache dir,
  builds the same `book.bin` spine/TOC a `.txt` book gets, and content is
  streamed back through `TxtToHtml`. `FsHelpers::hasPdfExtension` added;
  `.pdf` joins `hasReflowableBookExtension` and the library index, so PDFs
  appear in the file browser, home and library like other books.
  **Limits (honest):** no ToUnicode CMaps (non-BOM encoded text may be
  garbled), no image/vector rendering — scanned PDFs yield little text —
  no forms/JS, encrypted PDFs rejected, oversize streams skipped.

## 4. Cache on SD ("SSD") with LRU + cap

- `lib/FileCache/FileCache.{h,cpp}`: file-backed LRU cache. Root is
  configurable, default `/.crosspoint/cache` (matching the existing
  `/.crosspoint/` convention); cap is configurable, default 64 MiB.
  The persisted index (`lru_index.txt` in the root) maps key → size +
  last-access; RAM holds only that index (≤512 entries). Eviction removes
  least-recently-used files first and never the just-written/protected
  entry. Storage is injected (`FileOps`): `FileCacheHal.cpp` backs it
  with `HalStorage` on device; host tests use an in-memory fake.
- Settings: `CrossPointSettings.cacheRoot` (char[64], default
  `/.crosspoint/cache`) and `cacheMaxMiB` (uint8, default 64, range
  8–248 step 8), persisted/web-exposed via category-less `SettingInfo`
  entries (`cacheRoot` / `cacheMaxMiB`) — same pattern as the OPDS folder.
- Existing per-book EPUB/TXT caches already live on SD under
  `/.crosspoint/epub_<hash>/` (upstream design) and PDF extraction now
  follows the same rule. `FileCache` is wired into `Pdf::buildPdfCache`:
  every extracted text is registered in the index at the book-cache root
  and the cap (from `SETTINGS.cacheMaxMiB`, applied at boot in `main.cpp`)
  is enforced LRU-first; an evicted book re-extracts on next open because
  `Pdf::validateCache` notices the missing text file.

## 5. File browser sorting: name / date / type, ascending / descending

> Integrated 2026-11-05 from `sort-work/sort-feature.patch` (developed in
> the parallel copy `sort-work/repo` while font fallback work was in
> flight in the main tree; applied cleanly, no conflicts).

Upstream, the file browser (`FileBrowserActivity`) had exactly one order:
`FsHelpers::sortFileList` — folders first, then a natural (numeric-aware,
case-insensitive) name sort. The library view already had its own sort
(Recent/Title/Author over index metadata) and is unchanged. The browser
now offers three keys in both directions, chosen on-device:

- **Where:** long-press Confirm on any entry (the existing actions popup)
  → **Sort by** → one of six rows: `Name / Date / Type` ×
  `Ascending / Descending` (labels composed from translated parts, e.g.
  "Date: Descending"). Switching re-orders the current folder in place
  and keeps the highlight on the same entry.
- **Persistence:** the choice is stored in the new settings
  `fileSortKey` / `fileSortDirection` (`CrossPointSettings`), written via
  `SETTINGS.saveToFile()` on selection, so it survives reboot. The pair
  also appears under Settings → System ("Sort by" / "Sort direction"),
  which makes it web-editable through the existing SettingInfo plumbing.
  Out-of-range stored values clamp to Name / Ascending.

### Ordering contract (lib/FileSort)

The comparison logic is a pure, UI/HAL-free library, `lib/FileSort`
(`FileSort::sortEntries` over `Entry{name, modificationTime, isDirectory}`),
unit-tested on the host before the UI was wired (shift-left):

- **Folders first, always** — every key, both directions. This preserves
  the shipped `sortFileList` behaviour; reversing a sort never buries
  folders under files. Name / Ascending reproduces `sortFileList`
  exactly (a host test pins the equivalence).
- **Name:** the existing natural sort (`FsHelpers::naturalLess`):
  case-insensitive, numeric runs compare by value (`file2 < file10`).
- **Date:** file modification time from `HalFile::modificationTime()`,
  captured during the browser's existing directory pass into a parallel
  vector — no second SD walk. That value is the FAT date/time packed as
  `(date << 16) | time`; the FAT fields are most-significant-first, so
  the packed integer compares chronologically (the library index already
  relies on the same value). Resolution is the FAT 2-second tick; a
  missing timestamp is 0 and sorts as oldest. Ascending = oldest first,
  Descending = newest first.
- **Type:** by kind rank, defined as txt (0) < md (1) < epub (2) <
  pdf (3) < xtc/xtch (4) < images jpg/jpeg/png/bmp/gif (5) < other /
  no extension (6); within one kind, name natural ascending.
- **Direction applies to the primary key only.** Ties always fall back
  to name, natural ascending, in both directions, and the sort is stable
  — so a descending date list still reads A→Z within one timestamp.

### Expected orders for the emulator sample books

`emulator/sd/books` (timestamps illustrative: txt/epub older, md/pdf
newer — integration should set distinct mtimes when re-checking):

| Key / direction | Order |
| --- | --- |
| Name ↑ | sample.epub, sample.md, sample.pdf, sample.txt |
| Name ↓ | sample.txt, sample.pdf, sample.md, sample.epub |
| Type ↑ | sample.txt, sample.md, sample.epub, sample.pdf |
| Type ↓ | sample.pdf, sample.epub, sample.md, sample.txt |
| Date ↑ | sample.epub, sample.txt, sample.md, sample.pdf |
| Date ↓ | sample.md, sample.pdf, sample.epub, sample.txt |

(Date rows assume txt = epub = older, md = pdf = newer; equal timestamps
tie-break by name ascending, as above. These orders are pinned by the
`FileSortSampleBooks` host test.)

### i18n

Seven new keys — `STR_SORT_BY`, `STR_SORT_NAME`, `STR_SORT_DATE`,
`STR_SORT_TYPE`, `STR_SORT_DIRECTION`, `STR_SORT_ASCENDING`,
`STR_SORT_DESCENDING` — translated in all five shipped locales
(en/ja/zh-Hans/zh-Hant/id), taking ja/zh-Hans/zh-Hant from 52 to 59
hand-translated keys and id from 442 to 449. `scripts/gen_i18n.py`
regenerates cleanly: 665 keys, "Languages: 5".

### Tests

- New host suite `test/file_sort` (13 tests): name natural asc/desc,
  legacy-order equivalence, folders-first for all 6 key×direction
  combinations, date asc/desc incl. unknown-time and tie-breaks, type
  ranks and within-kind order, comparator stability, stored-value
  clamping, and the emulator sample orders above. Written before the
  implementation (the CMake generate step failed on the missing
  `FileSort.cpp` until it existed — the Red state).
- Full host suite re-run on the sort tree: **931/931 passed** (the
  fork's previous 918 plus the 13 new FileSort tests).
- Firmware build on the sort tree: `pio run -e default` **succeeded**
  (verified in `sort-work/pio-build2.log`: `[SUCCESS]`, no error lines)
  — RAM 20.3% (66,480 / 327,680 B, +8 B vs the fork baseline) and Flash
  79.0% (5,177,831 / 6,553,600 B, +8,112 B), with `lib/FileSort` picked
  up by the normal library discovery. The first attempt caught a real
  bug the host tests could not: `tr()` is a macro over literal key
  names, so the composed popup labels now go through `I18N.get()`.

### Limits

- Sorting exists in the file browser only; the library view keeps its
  own Recent/Title/Author sort. The firmware-picker mode shares the
  browser's stored order but has no sort popup of its own.
- Date sorting is only as good as FAT timestamps: files copied with a
  tool that does not preserve mtimes all share the copy time and then
  order by name (the tie-break), and 2-second resolution is inherent.
- No new heap allocations beyond one transient `Entry` vector per
  re-sort, sized to the folder's entry count like the existing `files`
  vector it mirrors; it is released when the sort returns.

## 6. OS backup / restore to SD (added 2026-11-05)

> Investigation first (repository evidence, not assumptions):
> - **Partition layout** (`partitions.csv`, the `default` env): dual OTA
>   slots — `nvs` 0x5000 @0x9000, `otadata` 0x2000 @0xE000,
>   `app0` (ota_0) 0x640000 @0x10000, `app1` (ota_1) 0x640000 @0x650000,
>   `spiffs`, `coredump`. A safe in-device restore path therefore exists:
>   write the inactive slot, switch boot, reboot.
>   `partitions_picco.csv` is a single-slot layout (+ vendor `safeboot`
>   factory partition) — no inactive slot, so the feature is hidden on
>   Picco, exactly like the existing OTA / SD-update menu entries
>   (`BoardConfig::isPicco()` guard in `SettingsActivity`).
> - **Existing update paths**: Web OTA (`src/network/OtaUpdater.cpp`,
>   esp_ota_* streaming) and SD update (`SdFirmwareUpdateActivity` →
>   `firmware_flash::flashFromSdPath` in `src/network/FirmwareFlasher.cpp`)
>   both land in the inactive slot. The SD path deliberately uses raw
>   `esp_partition_write` + `ota_boot::switchTo` because the stock X3/X4
>   bootloader accepts this fork's patched images while the runtime
>   `esp_image_verify` (used by `esp_ota_end`/`esp_ota_set_boot_partition`)
>   rejects them — so restore reuses `flashFromSdPath`, not the esp_ota
>   API. `firmware_flash::validateImageFile` already implements the full
>   bootloader-equivalent integrity walk (magic, segment table, XOR
>   checksum, SHA-256 trailer, chip id, board tag).
> - **Settings storage**: every user-facing store is a JSON file on SD
>   under `/.crosspoint/` (`CrossPointSettings` → `settings.json`,
>   `CrossPointState` → `state.json`, `RecentBooksStore` → `recent.json`,
>   `WifiCredentialStore` → `wifi.json`, `OpdsServerStore` → `opds.json`).
>   NVS holds no user data (Wi-Fi credentials moved to `wifi.json`; the
>   SDK's NVS auto-connect is explicitly suppressed in
>   `WifiSelectionActivity`). The bootloader itself is never touched.

**What a backup is** — `/.crosspoint/backup/<name>/` on SD, where `<name>`
is the RTC timestamp `YYYYMMDD-HHMMSS` (or `backup-NNN` when the RTC is
not plausible):

| File | Contents |
|---|---|
| `firmware.bin` | Running app partition image, trimmed to the true ESP image length (header/segment walk; the slot's 0xFF tail is not copied) |
| `nvs.bin` | Raw NVS partition — PC-level recovery artifact only (see below) |
| `config/*.json` | Copies of the `/.crosspoint/*.json` stores present at backup time |
| `manifest.json` | Written **last**, after read-back verification; its presence marks the backup valid |

The manifest records: format tag/version, creation time, app version
(`CROSSPOINT_VERSION`), board tag, source slot (`ota_0`/`ota_1`), a
**partition-configuration id** (SHA-256 over the canonicalised partition
table), firmware size + SHA-256, NVS size + SHA-256, and the config file
list.

**Restore flow** (Settings → General → *Restore OS* → pick a generation →
confirmation naming the backup and the restart):

1. Manifest parse (strict: format tag/version, hex digests, bare config
   file names only — a manifest can never point outside its directory).
2. Partition-configuration id must equal the running device's — an image
   from another layout/board is refused before any flash access.
3. The SD file's actual size and SHA-256 must match the manifest.
4. ESP image header check (magic, segment count) via lib/OsBackup.
5. `firmware_flash::validateImageFile` — the full integrity walk.
6. `firmware_flash::flashFromSdPath` writes the **inactive** slot and
   switches otadata (it re-validates internally; SD is removable media).
   Any failure before or during this step leaves the boot partition
   untouched — the current OS still boots.
7. Config JSONs are copied back last, immediately before the restart the
   UI performs, so nothing re-saves live settings over them.

**Safety guards**: confirmation popup (backup name + "device will
restart into the restored OS"); low-battery start block (< 20%, the
threshold `main.cpp` already uses — note it also blocks on USB power if
the gauge reads low; conservative by design); progress display with
"Do not power off!"; boot switch only after a fully verified flash;
backup write failures and read-back hash mismatches delete the partial
generation instead of leaving it looking valid.

**Scope decisions, stated plainly**:
- The **bootloader** and **otadata** are never backed up or written
  directly (otadata changes only through the existing `ota_boot` path).
- **NVS is backed up but not restored on-device**: the live NVS driver
  owns that flash region, and no user data lives there. `nvs.bin` exists
  so a bricked unit can be repaired from a PC (esptool write to the NVS
  offset in the manifest's layout). Config restore covers everything
  user-visible.
- Books, caches and other SD content are not part of an "OS" backup —
  they already live on the same SD card.
- Retention: after each successful backup, generations are pruned to
  the *OS backups to keep* setting (System, default 3, 1–10, also
  web-editable). There is no per-generation delete UI in this version.

**Implementation shape**: the pure engine is `lib/OsBackup`
(self-contained SHA-256, chunked 8 KiB copy/hash over abstract
source/sink, ESP image length walk mirroring `FirmwareFlasher`'s math,
manifest generate/parse, the fail-closed `checkRestore` gate, backup
name listing/pruning) with host tests in `test/os_backup` (23 tests,
written before the implementation). `src/OsBackupService.{h,cpp}` is
the thin device adapter (`esp_partition` reads, `HalStorage` files,
`firmware_flash`); UI is `OsBackupActivity` /
`OsRestoreActivity` under Settings → General, next to *SD Card
Firmware Update*. 15 new i18n keys, translated in all 5 languages
(680 keys total). The emulator verifies the same lib code at file level
(`docs/EMULATOR.md`, item 12) — real flash writes and otadata switching
remain hardware-only verification, and the **first on-device restore
should be done with a PC recovery path at hand** (USB + esptool /
web flasher), like any first bootloader-adjacent change.

## Verification (what was actually run in the dev VM)

- Python acceptance: `test_supported_languages.py` (5 tests),
  `test_confirmation_translations.py`, `test_translation_spacing.py` — OK.
- `gen_i18n.py` regeneration — OK, 5 languages.
- Host C++ tests (CMake, googletest): **918/918 passed**, including the
  new suites MarkdownToHtml (9), PdfText (6), FileCache (6),
  SupportedFormats (3), the TxtToHtml regression suite, and one updated
  upstream expectation (`LibraryBuilderTest` now expects `.pdf` to be a
  library book; `TxtToHtmlTest` expects the Markdown paragraph output).
- Font subset build + fontTools cmap/name verification — OK.
- **Firmware build (PlatformIO, env `default`, ESP32-C3): SUCCESS.**
  `pio run -e default` with the CI-pinned pioarduino core 6.1.19:
  RAM 20.3% (66,464 / 327,680 B), Flash 78.8% (5,165,097 / 6,553,600 B),
  `firmware.bin` produced. `Pdf.cpp`, `PdfText.cpp`, `MarkdownToHtml.cpp`,
  `FileCache*.cpp`, the `Epub.cpp` PDF wiring and the settings wiring all
  compile and link. The build caught two real fork bugs that host tests
  could not: the generated V1 language-migration table referenced removed
  languages (generator now emits `Language::_COUNT` sentinels, preserving
  on-disk indices) and `KeyboardLayoutSet.h` named removed languages
  (now QwertyEn only — the SDK has no JA/ZH layout; Indonesian uses it too).
- **Supplement re-verification (2026-11-05, §2a):** firmware rebuilt
  after the supplement change — `pio run -e default` **SUCCESS** again,
  RAM 20.3% (66,472 / 327,680 B), Flash 78.9% (5,169,719 / 6,553,600 B),
  byte-identical usage to the pre-supplement build (expected: the
  supplement changes offline font tooling, YAML and TTF assets only —
  no firmware C++). Python acceptance now also includes
  `test_sc_supplement.py` (6 tests) — OK; emulator suite re-run:
  items 1–9 PASS (item 9 was PASS_WITH_LIMITATION before the
  supplement), item 10 (QEMU) NOT_AVAILABLE as before.
- **Sort integration (2026-11-05, §5):** `sort-work/sort-feature.patch`
  applied cleanly to this tree (`git apply --check` passed beforehand, no
  conflicts; 14 files). i18n regenerated: 665 keys, "Languages: 5".
  Full host suite re-run on the integrated tree: **931/931 passed**
  (918 fork baseline + 13 new FileSort tests). Firmware rebuilt on the
  integrated tree (`pio run -e default` SUCCESS): RAM 20.3%
  (66,480 / 327,680 B), Flash 79.0% (5,177,831 / 6,553,600 B) — identical
  figures to the sort tree's own build. `firmware-default.bin` refreshed
  from the build output (byte-identical copy). Emulator re-run with the
  new item 11 (real `FileSort` over the sample books): PASS, all six
  key × direction orders match §5 (see `docs/EMULATOR.md`).
- **OS backup/restore (2026-11-05, §6):** host tests written first
  (Red confirmed: CMake generation failed while `lib/OsBackup` did not
  exist), then implemented. Full host suite on the integrated tree:
  **954/954 passed** (931 + 23 new `OsBackup` tests). i18n regenerated:
  680 keys, "Languages: 5"; the Python acceptance tests
  (`test_supported_languages`, `test_confirmation_translations`,
  `test_sc_supplement`, `test_translation_spacing`) all pass.
  Firmware rebuilt (`pio run -e default` **SUCCESS**): RAM 20.3%
  (66,480 / 327,680 B — unchanged), Flash 79.6% (5,214,481 / 6,553,600 B,
  +36,650 B vs the sort build). `firmware-default.bin` refreshed
  (byte-identical to the `.pio` output). Build-environment note: the
  first attempt died silently during LDF dependency scanning, and the
  second reached the link but failed in `checkprogsize` because the
  pioarduino libs package was missing its per-chip
  `pioarduino-build.py.esp32c3` file (repaired with a symlink to the
  bundled `pioarduino-build.py` inside `~/.platformio/packages`, outside
  the repo; the third run is the SUCCESS above, with a firmware.bin
  hash identical to the second run's link output). Emulator suite
  re-run with the new item 12 (real `lib/OsBackup` round-trip): PASS —
  11 PASS + QEMU NOT_AVAILABLE overall (see `docs/EMULATOR.md`).
- **Not run:** on-device (hardware) testing — flashing and reading a real
  PDF/MD book on an X3/X4 remains for the human, per repo `AGENTS.md`.
  This now also covers the first real OS restore (§6).
