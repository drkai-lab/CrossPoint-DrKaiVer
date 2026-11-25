# Host Emulator — design, verification results, and limits

Built for this fork so the "not run" item in `docs/FORK_CHANGES.md`
(on-device flashing / on-device display) can be covered as far as
possible **without hardware**. The emulator lives in
`emulator/` next to this repo (in the delivery ZIP:
`crosspoint-reader-shiftleft/emulator/`).

## What the repo already had (checked first, not assumed)

- `platformio.ini`: only ESP32 target envs (`default`, `x4pro`, …).
  **No** native/simulator env, no QEMU config, no Wokwi config.
- HAL boundary: `lib/hal/HalDisplay.h`, `HalStorage.h`, `HalGPIO.h`, …
  Display geometry comes from the FreeInk SDK:
  `FreeInkDisplay::DISPLAY_WIDTH = 800`, `DISPLAY_HEIGHT = 480`
  (X4 panel, landscape); the reader uses the panel rotated, i.e. a
  **480×800 logical portrait** framebuffer.
- Host test basis: `test/CMakeLists.txt` already compiles individual
  libraries on the host with stub headers
  (`test/fs_helpers/stubs`, `test/chapter_html_slim_parser/stubs`).
  The emulator reuses exactly those stubs and include paths.

## Method chosen, and why

**Primary: host-native emulator.** The fork's real device code is
compiled with g++ and run on the host behind the same injected
contracts the firmware uses:

| Device path | Emulator path |
|---|---|
| `TxtToHtml` / `MarkdownToHtml` (lib/Txt) | same `.cpp`, compiled into `emu_core` |
| `PdfText` (lib/Pdf) + miniz inflate | same `.cpp`; inflate callback backed by zlib (same contract: one zlib-wrapped stream in, text out) |
| `FileCache` (lib/FileCache) via `FileCacheHal` → `HalStorage` → SD | same `FileCache.cpp`; `FileOps` backed by a host directory = the SD root |
| `FsHelpers` format detection | same `.cpp` |
| e-paper panel (HalDisplay/GfxRenderer) | 480×800 framebuffer → PNG (`last_frame.png`, `frames/frame_NNN_*.png`) |
| SD card | host folder (`emulator/sd/`); cache appears at `sd/.crosspoint/cache/` with the persisted `lru_index.txt` |
| Buttons (HalGPIO) | script tokens / interactive commands: `up down confirm back next prev`, `select:<n>` |
| UI strings (gen_i18n from YAML) | read from the same `lib/I18n/translations/*.yaml` files |
| Book font (SdCardFont + Noto Sans JP subsets) | the same subset TTFs rasterised with Pillow/FreeType |

The screen layout (home list, reader page, language list) is the
emulator's own approximation of the firmware activities — it is
**not** pixel-identical to `HomeActivity`/`EpubReaderActivity`
rendering. What is verified is the fork's logic and assets underneath:
conversion, extraction, caching, format gating, language set, and
glyph coverage.

One component is host-substituted and labelled as such wherever it is
used: the **EPUB container unpack**. `lib/Epub` is bound to
`HalStorage`/`GfxRenderer` and does not host-build, so the emulator
unpacks the EPUB zip/OPF/spine with Python and feeds the extracted
text through the **real** `FileCache` (`emu_core cache-put`). EPUB
engine behaviour beyond container/spine/text order is not covered
(see Limits).

**QEMU: attempted, not available, and insufficient anyway.**
`qemu-system-riscv32` / `qemu-system-xtensa` are not installed in the
dev VM and no Espressif QEMU exists under `~/.platformio` /
`~/.espressif`; installing was attempted (`apt-get update` /
`apt-get install qemu-system-misc`) and failed — package lists were
unreachable (`E: Unable to locate package qemu-system-misc`, mirror
resolution timeouts). Even with Espressif's QEMU fork, stock QEMU has
no model of the SSD1677/UC82xx e-paper controllers or of the SPI SD
card this firmware drives, so a boot could at best show early serial
logs before display/storage init — it cannot verify screens, books,
or the SD cache. That is why the host-native route is the primary
one; if a QEMU boot log is ever added it should be treated as a
smoke check only.

## How to run

```sh
cd emulator
./run_emulator.sh                                  # build + samples + home screen
./run_emulator.sh --script "confirm,next,next,back"  # scripted buttons
python3 emulator.py --interactive                  # type: up/down/confirm/back/next/prev
python3 test_emulator.py                           # full automated verification
```

Outputs per run (`emulator/run/`): `frames/*.png`,
`last_frame.png`, `log.jsonl` / `log.txt` (boot, button, open_book,
frame and language events), `state.json`. The harness polls for
frame/state files with a `wait_until` helper — no fixed sleeps.

## Verification results (run 2026-11-05, this VM)

Automated suite `emulator/test_emulator.py`, full log in
`emulator/test_run/results.txt` (also bundled in the delivery ZIP):

| # | Check | Result |
|---|---|---|
| 1 | Format detection by real `FsHelpers`: txt/md/epub/pdf reflowable, png rejected | PASS |
| 2 | Boot → home frame at 480×800 listing the 4 sample books | PASS |
| 3 | Exactly 5 UI languages {ja, en, zh-Hans, zh-Hant, id}; each selectable on the language screen and applied | PASS |
| 4 | txt opens via real `TxtToHtml`; ≥2 pages; `next` reaches page 2, `prev` returns; page frames differ | PASS |
| 5 | md opens via real `MarkdownToHtml`: cached HTML contains `<h1>見出しサンプル</h1>` and `<b>Markdown</b>` | PASS |
| 6 | epub opens: chapters cached on SD in spine order (ch1 before ch2) | PASS |
| 7 | pdf opens via real `PdfText`: 2 pages counted; literal-string text, FlateDecode stream, and UTF-16BE-BOM Japanese text all extracted | PASS |
| 8 | Cache is on the SD folder: `sd/.crosspoint/cache/lru_index.txt` + 4 book entries; nothing cached in the work dir | PASS |
| 9 | Glyphs render with ink, not tofu/blank, via JP primary + SC supplement: ja, zh-Hans, zh-Hant, en, id complete | PASS (was PASS_WITH_LIMITATION before the supplement; see below) |
| 10 | QEMU boot check | NOT_AVAILABLE (see above) |

Captures bundled in `emulator/captures/`: `home_ja.png`,
`home_zh-Hans.png`, `language_screen.png`, `reader_txt_page1.png`,
`reader_md_page1.png`, `reader_epub_page1.png`, `reader_pdf_page1.png`,
`reader_zh-Hans_page1.png`.

### Finding (fixed): Noto Sans JP cannot draw some simplified-Chinese characters

The glyph check originally found three characters used in the samples
that are **absent from Noto Sans JP itself**: 这 (U+8FD9), 简 (U+7B80),
认 (U+8BA4). They rendered blank (0 ink pixels). This was not an
emulator artefact: the Google Fonts *Noto Sans JP* family carries a
Japanese glyph repertoire, and those simplified-only forms do not
exist in the font (verified against the subset TTF cmap: 13,529
codepoints; zh-Hant, ja, en and id sample sets were complete).

The user approved closing the gap with a supplementary font, and it
is now fixed — full details in `docs/FORK_CHANGES.md` §2a:

- The set-based gap is 3,333 codepoints (GB2312 ∪ TGH 2013 ∪ the
  fork's zh YAMLs/samples, minus JP's cmap); all are present in
  Noto Sans SC, which was subset to exactly those codepoints
  (~1.08 MB per weight).
- On device, the SD-font converter merges the supplement into the
  NotoSansJP `.cpfont` as its fallback face (primary glyph wins;
  fallback only on a miss), so the panel path is unchanged.
- The emulator mirrors that contract: `Fonts`/`FallbackFont` in
  `emulator.py` rasterise with Noto Sans JP first and take a glyph
  from the SC supplement only when JP lacks it. Item 9 now checks the
  merged coverage plus ink for an expanded Hans set, and re-run
  captures show the difference: `home_zh-Hans.png` renders 浏览文件
  and the header 简体中文 in full (previously 简 was a gap), and
  `reader_zh-Hans_page1.png` renders 这是简体中文的示例文本 and
  第二页的内容，用于确认翻页 (认 via the supplement) with no blanks.
- Re-run result: **items 1–9 all PASS** (item 10 remains
  NOT_AVAILABLE). Remaining font limits are the BMP scope of the
  `.cpfont` pipeline and the Big5 rare/variant repertoire, both
  documented in FORK_CHANGES §2a.

### File sorting (integrated 2026-11-05)

The sort feature (`docs/FORK_CHANGES.md` §5) lives in
`FileBrowserActivity`, which the emulator does not run — its home
screen is an approximation that lists the SD books by name and does
not read the device's `fileSortKey`/`fileSortDirection` settings.
What the emulator *can* verify honestly, it now does: `emu_core`
links the device's real `lib/FileSort` (`emu_core sort <key> <dir>
<name>=<mtime>...`), and test item 11 sets distinct mtimes on the
four sample books (txt/epub older, md/pdf newer, equal timestamps
inside each pair) and checks all six key × direction orders against
the contract table in FORK_CHANGES §5 — all six match. The
FileBrowserActivity UI path itself (popup, settings persistence,
in-place re-order) is verified by the host `FileSort` suite
(13 tests, part of the 931-test full run) and the firmware build,
not by emulator screens.

| 11 | File sort via real `FileSort`: name/date/type × asc/desc over the 4 sample books with distinct mtimes | PASS |
| 12 | OS backup → tamper detection → restore via real `OsBackup`: manifest written, tampered backup refused (`FIRMWARE_HASH_MISMATCH`), wrong-layout backup refused (`PARTITION_CONFIG_MISMATCH`), intact backup restores byte-for-byte | PASS |

### OS backup / restore (added 2026-11-05)

The OS backup feature (`docs/FORK_CHANGES.md` §6) writes real flash on
device (`esp_partition` reads, `firmware_flash` writes, otadata boot
switch) — none of that exists in a host process, and this section does
not pretend otherwise. What the emulator *does* verify is the part that
decides whether a restore is allowed at all: `emu_core` links the real
`lib/OsBackup` and exposes `osbackup-create` / `osbackup-restore` over
host files playing the roles of the running partition and the SD card.
Test item 12 builds a synthetic but layout-accurate ESP image
(24-byte header, 2 segments, aligned pad + checksum, SHA trailer),
then checks: backup creation produces `/.crosspoint/backup/<name>/`
with a manifest whose size/SHA-256 match the image; flipping one byte
of the backed-up `firmware.bin` makes restore refuse with
`FIRMWARE_HASH_MISMATCH` and write nothing; restoring a good backup
under the *other* partition layout (`picco` instead of `default`,
mirroring the repo's two real `partitions*.csv`) refuses with
`PARTITION_CONFIG_MISMATCH`; and an intact backup under the right
layout restores byte-for-byte. The device-only remainder — partition
reads, flash erase/write timing, the otadata switch and the actual
reboot into the restored slot — is listed below as hardware-only.

## What only real hardware can still confirm

- Flashing `firmware-default.bin` and the panel's actual e-paper
  refresh/ghosting/grayscale behaviour.
- Real SD card timing, hot-swap, and USB mass-storage behaviour.
- `lib/Epub`'s own layout engine rendering (the emulator substitutes
  container unpack + its own paging for the screen).
- Button GPIO behaviour, power/sleep, Wi-Fi, battery — no emulation.
- OS restore's device half: an actual restore writes the inactive OTA
  flash partition and switches otadata, then reboots into the restored
  slot (§6). The emulator verifies the lib/OsBackup decision logic at
  file level only (item 12); the flash write, boot switch and first
  real restore must be confirmed on hardware, with a PC recovery path
  (USB + esptool / web flasher) at hand.
- Performance/RAM on the ESP32-C3 under real book sizes (host runs
  say nothing about the 380 KB budget; the firmware build's static
  RAM/flash figures are in `docs/FORK_CHANGES.md`).
