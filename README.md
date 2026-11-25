# CrossPoint Reader — CrossPoint-DrKaiVer

**日本語** | [English](#english) | [简体中文](#简体中文) | [繁體中文](#繁體中文) | [Bahasa Indonesia](#bahasa-indonesia) | [한국어](#한국어)

CrossPoint Reader のフォーク版です。Xteink X3 / X4 用（X4Pro 用ではありません）。

- UI言語は日本語 / 英語 / 簡体字 / 繁体字 / インドネシア語の5言語のみ
- フォントは Noto Sans JP が主フォント。JPに無い簡体字は Noto Sans SC 補助、ハングルは Noto Sans KR 補助で補完します（韓国語の本の本文・ファイル名が表示できます。UI言語に韓国語は含まれません）
- フォントは SD カード上に 8/10/12/14/16/18pt を用意。小さいUI文字（ファイル名・本のタイトル・ボタン）も化けません
- 対応形式： txt / epub / md / pdf（PDFはテキスト抽出のみ）
- ファイルブラウザの並び替え（名前 / 日付 / 種類、フォルダ優先）
- SDカード上のファイルキャッシュ（`/.crosspoint/cache`、LRU、既定64MiB）
- OSバックアップ / リストア（SDの `/.crosspoint/backup/` へ、世代管理つき）

**入れ方**

1. [Releases](https://github.com/drkai-lab/CrossPoint-DrKaiVer/releases) から SD キットをダウンロードし、中身を SD カードのルートへコピー
2. 初めて入れる場合は PC の Chrome / Edge で [Flash Tools](https://crosspointreader.com/#flash-tools) を開き、USB-C 接続して「Custom .bin」で `firmware-default.bin` を書く（純正ファームウェアには SD 更新の項目がありません）。すでに CrossPoint が入っている本体は、設定 → General →「SD Card Firmware Update」から更新できます
3. 設定 → Reader → Font Family で **NotoSansJP** を選ぶ

変更の詳細は `docs/FORK_CHANGES.md` を見てください。上流ベースは `8503b7f` で、配布ファームウェアは `drkai-ver` ブランチからビルドしています（`develop` には上流の最新とのマージも入っています）。

---

## English

A fork of CrossPoint Reader for the Xteink X3 / X4 (not for the X4Pro).

- UI languages: Japanese / English / Simplified Chinese / Traditional Chinese / Indonesian only
- Main font is Noto Sans JP. Simplified-Chinese glyphs JP lacks come from a Noto Sans SC supplement, and Hangul from a Noto Sans KR supplement, so Korean book text and file names render too (Korean is not a UI language)
- Fonts ship on the SD card at 8/10/12/14/16/18pt, so small UI text (file names, book titles, buttons) renders correctly
- Formats: txt / epub / md / pdf (PDF is text extraction only)
- File browser sorting (name / date / type, folders first)
- File cache on the SD card (`/.crosspoint/cache`, LRU, 64 MiB default cap)
- OS backup / restore to SD (`/.crosspoint/backup/`, with generations)

**Install**

1. Download the SD kit from [Releases](https://github.com/drkai-lab/CrossPoint-DrKaiVer/releases) and copy its contents to the root of your SD card
2. First install: open the [Flash Tools](https://crosspointreader.com/#flash-tools) in Chrome / Edge on a PC, connect the device via USB-C, and flash `firmware-default.bin` via "Custom .bin" (stock firmware has no SD update menu). If CrossPoint is already installed, update via Settings → General → "SD Card Firmware Update"
3. In Settings → Reader → Font Family, select **NotoSansJP**

See `docs/FORK_CHANGES.md` for the full list of changes. Based on upstream commit `8503b7f`; the release firmware is built from the `drkai-ver` branch (`develop` additionally merges newer upstream changes).

---

## 简体中文

CrossPoint Reader 的分支版本，适用于 Xteink X3 / X4（不适用于 X4Pro）。

- 界面语言仅包含：日语 / 英语 / 简体中文 / 繁体中文 / 印尼语
- 主字体为 Noto Sans JP。JP 缺失的简体字形由 Noto Sans SC 补充字体补齐，韩文字形由 Noto Sans KR 补充字体补齐，因此韩语书籍的正文和文件名也能正常显示（韩语不是界面语言）
- 字体以 8/10/12/14/16/18pt 存放在 SD 卡上，较小的界面文字（文件名、书名、按钮）也不会乱码
- 支持格式：txt / epub / md / pdf（PDF 仅提取文本）
- 文件浏览器排序（按名称 / 日期 / 类型，文件夹优先）
- SD 卡上的文件缓存（`/.crosspoint/cache`，LRU，默认上限 64 MiB）
- 系统备份 / 恢复到 SD 卡（`/.crosspoint/backup/`，带世代管理）

**安装方法**

1. 从 [Releases](https://github.com/drkai-lab/CrossPoint-DrKaiVer/releases) 下载 SD 套件，将其内容复制到 SD 卡根目录
2. 首次安装：请在电脑上用 Chrome / Edge 打开 [Flash Tools](https://crosspointreader.com/#flash-tools)，通过 USB-C 连接设备，使用 "Custom .bin" 写入 `firmware-default.bin`（原厂固件没有 SD 更新菜单）。如果设备已安装 CrossPoint，可通过 设置 → General → "SD Card Firmware Update" 更新
3. 在 设置 → Reader → Font Family 中选择 **NotoSansJP**

全部变更见 `docs/FORK_CHANGES.md`。基于上游提交 `8503b7f`；发布固件由 `drkai-ver` 分支构建（`develop` 还合并了上游的更新）。

---

## 繁體中文

CrossPoint Reader 的分支版本，適用於 Xteink X3 / X4（不適用於 X4Pro）。

- 介面語言僅包含：日語 / 英語 / 簡體中文 / 繁體中文 / 印尼語
- 主字型為 Noto Sans JP。JP 缺失的簡體字形由 Noto Sans SC 補充字型補齊，韓文字形由 Noto Sans KR 補充字型補齊，因此韓語書籍的正文和檔案名稱也能正常顯示（韓語不是介面語言）
- 字型以 8/10/12/14/16/18pt 存放於 SD 卡上，較小的介面文字（檔案名稱、書名、按鈕）也不會亂碼
- 支援格式：txt / epub / md / pdf（PDF 僅擷取文字）
- 檔案瀏覽器排序（按名稱 / 日期 / 類型，資料夾優先）
- SD 卡上的檔案快取（`/.crosspoint/cache`，LRU，預設上限 64 MiB）
- 系統備份 / 還原到 SD 卡（`/.crosspoint/backup/`，附世代管理）

**安裝方法**

1. 從 [Releases](https://github.com/drkai-lab/CrossPoint-DrKaiVer/releases) 下載 SD 套件，將其內容複製到 SD 卡根目錄
2. 首次安裝：請在電腦上用 Chrome / Edge 開啟 [Flash Tools](https://crosspointreader.com/#flash-tools)，透過 USB-C 連接裝置，使用 "Custom .bin" 寫入 `firmware-default.bin`（原廠韌體沒有 SD 更新選單）。如果裝置已安裝 CrossPoint，可透過 設定 → General → "SD Card Firmware Update" 更新
3. 在 設定 → Reader → Font Family 中選擇 **NotoSansJP**

全部變更見 `docs/FORK_CHANGES.md`。基於上游提交 `8503b7f`；發布韌體由 `drkai-ver` 分支建置（`develop` 還合併了上游的更新）。

---

## Bahasa Indonesia

Fork dari CrossPoint Reader untuk Xteink X3 / X4 (bukan untuk X4Pro).

- Bahasa antarmuka hanya: Jepang / Inggris / Mandarin Sederhana / Mandarin Tradisional / Indonesia
- Font utama adalah Noto Sans JP. Glif Mandarin Sederhana yang tidak ada di JP dilengkapi dari font tambahan Noto Sans SC, dan Hangul dari font tambahan Noto Sans KR, sehingga teks buku dan nama file berbahasa Korea juga tampil dengan benar (bahasa Korea bukan bahasa antarmuka)
- Font tersedia di kartu SD dalam ukuran 8/10/12/14/16/18pt, jadi teks antarmuka kecil (nama file, judul buku, tombol) tampil dengan benar
- Format: txt / epub / md / pdf (PDF hanya ekstraksi teks)
- Pengurutan peramban file (nama / tanggal / jenis, folder diutamakan)
- Cache file di kartu SD (`/.crosspoint/cache`, LRU, batas bawaan 64 MiB)
- Pencadangan / pemulihan OS ke SD (`/.crosspoint/backup/`, dengan generasi)

**Cara memasang**

1. Unduh kit SD dari [Releases](https://github.com/drkai-lab/CrossPoint-DrKaiVer/releases) dan salin isinya ke akar kartu SD
2. Pemasangan pertama: buka [Flash Tools](https://crosspointreader.com/#flash-tools) di Chrome / Edge pada PC, hubungkan perangkat lewat USB-C, lalu tulis `firmware-default.bin` lewat "Custom .bin" (firmware bawaan tidak memiliki menu pembaruan SD). Jika CrossPoint sudah terpasang, perbarui lewat Pengaturan → General → "SD Card Firmware Update"
3. Di Pengaturan → Reader → Font Family, pilih **NotoSansJP**

Lihat `docs/FORK_CHANGES.md` untuk daftar lengkap perubahan. Berdasarkan commit hulu `8503b7f`; firmware rilis dibangun dari cabang `drkai-ver` (`develop` juga menggabungkan perubahan hulu yang lebih baru).

---

## 한국어

Xteink X3 / X4용 CrossPoint Reader 포크입니다(X4Pro용이 아닙니다).

- UI 언어는 일본어 / 영어 / 중국어 간체 / 중국어 번체 / 인도네시아어 5개만 제공
- 기본 글꼴은 Noto Sans JP입니다. JP에 없는 간체자 글리프는 Noto Sans SC 보조 글꼴로, 한글은 Noto Sans KR 보조 글꼴로 보완하므로 한국어 책 본문과 파일 이름도 표시됩니다(한국어는 UI 언어가 아닙니다)
- 글꼴은 SD 카드에 8/10/12/14/16/18pt로 들어가 있어 작은 UI 문자(파일 이름, 책 제목, 버튼)도 깨지지 않습니다
- 지원 형식: txt / epub / md / pdf(PDF는 텍스트 추출만)
- 파일 브라우저 정렬(이름 / 날짜 / 종류, 폴더 우선)
- SD 카드 파일 캐시(`/.crosspoint/cache`, LRU, 기본 상한 64MiB)
- OS 백업 / 복원(SD의 `/.crosspoint/backup/`, 세대 관리 포함)

**설치 방법**

1. [Releases](https://github.com/drkai-lab/CrossPoint-DrKaiVer/releases)에서 SD 키트를 내려받아 내용을 SD 카드 루트에 복사
2. 처음 설치할 때는 PC의 Chrome / Edge에서 [Flash Tools](https://crosspointreader.com/#flash-tools)를 열고 USB-C로 기기를 연결한 뒤 "Custom .bin"으로 `firmware-default.bin`을 기록합니다(순정 펌웨어에는 SD 업데이트 메뉴가 없습니다). 이미 CrossPoint가 설치된 기기는 설정 → General → "SD Card Firmware Update"에서 업데이트할 수 있습니다
3. 설정 → Reader → Font Family에서 **NotoSansJP**를 선택

변경 사항 전체는 `docs/FORK_CHANGES.md`를 보세요. 업스트림 커밋 `8503b7f` 기반이며, 배포 펌웨어는 `drkai-ver` 브랜치에서 빌드했습니다(`develop`에는 업스트림 최신 변경과의 병합도 들어 있습니다).

---

[![Fund contributors](https://img.shields.io/badge/%F0%9F%91%91_Fund_contributors-royalty.dev-BB953A?style=for-the-badge&labelColor=1a1a1a)](https://app.royalty.dev/crosspoint-reader/crosspoint-reader)

CrossPoint is open-source e-reader firmware - community-built, fully hackable, free forever. It's maintained by a growing community of developers and readers who believe your device should do what you want - not what a manufacturer decided for you.

### Now running on:

- **ESP32C3-based** Xteink X4 and X3.
- **ESP32S3-based** Xteink X4Pro and X4Classic, Seeed reTerminal Sticky, M5PaperMono, Metalio E-Ink 4, EEGOREAD A4 and Boox Picco.

Check [our Devices page](https://crosspointreader.com/devices) for the full list.

![CrossPoint Reader running on Xteink device](./docs/images/cover.jpg)

> If you're planning to buy an Xteink device, consider purchasing an **X3/X4 Developer Edition** through https://crosspointreader.com. CrossPoint receives a small share of each sale, helping fund development costs.

## What can CrossPoint do?

- **Reader engine**: EPUB 2/3 rendering with embedded-style option, image handling, hyphenation, kerning, adaptive table layouts, native CJK ruby annotations, chapter navigation, footnotes, bookmarks, dictionary lookups ([StarDict](docs/dictionary.md)), go-to-percent, auto page turn, orientation control, focus reading, KOReader progress sync and more.

- **Various formats**: native handling for `.epub`, `.xtc/.xtch`, `.txt`, and `.bmp`.

- **Touch reading**: follow EPUB links and look up words in the dictionary on touch-enabled devices.

- **Highlights and clippings**: highlight text in EPUBs and save excerpts as clippings. Use the reader menu on button devices, or hold a word and drag the selection handles on touch devices. Look up selected words or phrases in the dictionary. Enable Sync Clippings to sync saved excerpts through a compatible CrossPoint Sync server. See [Clippings and Highlights](USER_GUIDE.md#53-clippings-and-highlights).

- **Screenshots.**

- **Custom fonts**: install your favorite fonts on the SD card.

- **Tilt page turn (X3 and Sticky)**.

- **Bluetooth page turners**: pair a remote under Settings → Connectivity → Bluetooth Page Turner. Map short and long presses to page turns, chapter navigation, the reader menu, or clipping selection.

- **USB Drive mode (X4Pro)**: access the SD card as USB mass storage.

- **Library workflow**: indexed title/author search, recently-added and alphabetical views, multilingual grouping, folder browser, recent books, and SD-cache management.

- **Wireless workflows**:
  
  - File transfer web UI
  - EPUB Optimizer
  - Web settings UI/API (edit many device settings from browser)
  - WebSocket fast uploads
  - WebDAV handler
  - AP mode (hotspot) and STA mode (join existing Wi-Fi), both with QR helpers
  - Calibre wireless connect flow
  - OPDS browser with saved servers (up to 8), search, pagination, and direct download
  - OTA update checks and installs from GitHub releases

- **Customization**: night mode, multiple themes (Classic, Lyra, Lyra Extended, RoundedRaff), sleep screen modes including transparent overlays, front/side button remapping, status bar controls, power-button behavior, refresh cadence, and more.

- **Localization**: 34 UI languages and counting, including CJK font fallback and RTL support.

- **SD card plugins**: install plugins for book catalogs, downloads, and tools in the device's web interface. Enable Plugins under Settings → Connectivity to show them on Home. On-device plugins use existing firmware screens and operations. JavaScript runs in a browser on your phone or computer, and its page must stay open while it works. Plugins follow [CrossPoint's reading-focused scope](SCOPE.md#out-of-scope). See the [plugin guide](docs/sd-plugins.md).

### Coming soon:

- More themes.

- Much more! stay tuned.

---

## USB-locked devices (Xteink Unlocker)

Some Xteink units purchased from third-party stores (e.g. AliExpress) ship with USB flashing locked from the factory.
If your device is locked, you will need to use the **Xteink Unlocker** tool available at
https://crosspointreader.com/#unlock-tool before you can flash CrossPoint.

**You do not need this tool if you bought your device directly from xteink.com.** Those units are not locked.

**Not sure if your device is locked?** Power it on, connect the USB-C cable, and try flashing via the web flasher first (see
[Install firmware](#install-firmware) below). If the browser's serial device picker does not show your device, try a different
USB port or browser before assuming the device is locked. Only reach for the unlocker if the device still doesn't appear.

> ### ⚠️ WARNING: READ THIS BEFORE USING THE UNLOCKER ⚠️
> 
> **The only officially supported firmwares in the unlock tool are CrossPoint and CrossInk.**
> 
> Flashing any other firmware on a USB-locked device may **permanently brick the device** or leave it **permanently
> stuck on that firmware with no recovery path**. Once USB flashing is re-locked, your only way back is via OTA, and if
> the firmware you flashed doesn't support OTA, **there is no way out**.

## Install firmware

### Web installer (recommended)

1. Connect your device to your computer via USB-C and wake/unlock the device
2. Go to https://crosspointreader.com/#flash-tools, select your device (X3, X4, Xteink X4Pro, Seeed reTerminal Sticky, or M5PaperMono), and choose an official CrossPoint release.

### Web installer (specific version)

1. Connect your device to your computer via USB-C and wake/unlock the device
2. Download the firmware file for your device from [Releases](https://github.com/crosspoint-reader/crosspoint-reader/releases), or compile yourself.
3. Go to https://crosspointreader.com/#flash-tools, select your device, click "Custom .bin" and upload the firmware file.

### Revert to Official Firmware

To revert to the official firmware, you can also flash the latest official firmware using https://crosspointreader.com/#flash-tools.

### Command line

1. Install [`esptool`](https://github.com/espressif/esptool):

```bash
pip install esptool
```

2. Download the firmware file for your device from the [releases page](https://github.com/crosspoint-reader/crosspoint-reader/releases).
3. Connect your device via USB-C.
4. Find the device port. On Linux, run `dmesg` after connecting. On macOS:

```bash
log stream --predicate 'subsystem == "com.apple.iokit"' --info
```

5. Flash an X3 or X4:

```bash
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

   Flash an Xteink X4Pro, Seeed reTerminal Sticky, or M5PaperMono:

```bash
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

### Manual

See [Development quick start](#development-quick-start) below.

---

## Custom SD-card fonts

On devices with external RAM enabled in CrossPoint, copy `.ttf`, `.otf`, or `.ttc` files to the SD card and select them as reader fonts. Put one file in `/fonts/` or `/.fonts/`, or put one family's files in a subfolder. See the [SD card font guide](./docs/sd-card-fonts.md) for the folder layout and styles.

On other devices, convert the font to `.cpfont` first. `.cpfont` files also work on devices with external RAM enabled and have better performance. No firmware reflash is needed to add fonts.

To make `.cpfont` files:

1. Go to https://crosspointreader.com/fonts and open the "SD-card font builder" form.
2. Upload up to four styles (regular, bold, italic, bold-italic), set the family name, point sizes, and Unicode range.
3. Download the generated `.cpfont` files.
4. Copy them to your SD card under `/fonts/YourFont/` (or `/.fonts/YourFont/` to hide the folder).
5. Select the font on the device from the font settings.

Conversion runs the firmware repo's `lib/EpdFont/scripts/fontconvert_sdcard.py` script unmodified, so output matches a local host build.

---

## Documentation

- [User Guide](./USER_GUIDE.md)
- [Web server usage](./docs/webserver.md)
- [Web server endpoints](./docs/webserver-endpoints.md)
- [Project scope](./SCOPE.md)
- [Contributing docs](./docs/contributing/README.md)
- [Touch and UI development](./docs/contributing/touch-and-ui.md) - how to build new screens on the FreeInkUI activity bases (UiListActivity and friends), plus build envs for the non-Xteink touch devices

---

## Development quick start

### Prerequisites

- [pioarduino PlatformIO Core](https://github.com/pioarduino/platformio-core) or [VS Code + pioarduino IDE](https://github.com/pioarduino/pioarduino-vscode-ide)
- Python 3.8+
- `clang-format` 21
- USB-C cable supporting data transfer

### Setup

```bash
git clone --recursive https://github.com/crosspoint-reader/crosspoint-reader
cd crosspoint-reader

# if cloned without --recursive:
git submodule update --init --recursive
```

### Nix/NixOS

Nix/NixOS users can enter the development shell with either `nix develop` (flakes) or `nix-shell`:

```bash
nix develop -f nix
# or
nix-shell nix
```

To flash a connected ESP32-C3 device, enable PlatformIO's udev rules in your NixOS configuration:

```nix
services.udev.packages = with pkgs; [ platformio-core.udev ];
```

After rebuilding the system configuration, reconnect the device or reload udev rules.

### Build / flash / monitor

```bash
pio run --target upload
```

### Contributor pre-PR checks

```bash
./bin/clang-format-fix
pio check -e default
pio run -e default
```

### Debugging

After flashing the new features, it’s recommended to capture detailed logs from the serial port.

First, make sure all required Python packages are installed:

```python
python3 -m pip install pyserial colorama matplotlib
```

After that run the script:

```sh
# For Linux
# This was tested on Debian and should work on most Linux systems.
python3 scripts/debugging_monitor.py

# For macOS
python3 scripts/debugging_monitor.py /dev/cu.usbmodem2101
```

Minor adjustments may be required for Windows.

---

## Internals

CrossPoint Reader is pretty aggressive about caching data down to the SD card to minimise RAM usage. The ESP32-C3 only has ~380KB of usable RAM, so we have to be careful. A lot of the decisions made in the design of the firmware were based on this constraint.

### Data caching

The first time chapters of a book are loaded, they are cached to the SD card. Subsequent loads are served from the
cache. This cache directory exists at `.crosspoint` on the SD card. The structure is as follows:

```text
.crosspoint/
├── epub_<hash>/         # one directory per book, named by content hash
│   ├── progress.bin     # reading position (chapter, page, etc.)
│   ├── cover.bmp        # generated cover image
│   ├── book.bin         # metadata: title, author, spine, TOC
│   ├── css_rules.cache  # parsed CSS rule cache
│   ├── img_*            # rendered image cache files
│   └── sections/        # per-chapter layout cache
│       ├── 0.bin
│       ├── 1.bin
│       └── ...
├── settings.json        # device settings
├── state.json           # resume/runtime state
└── recent.json          # recent books list
```

Removing `/.crosspoint` clears all cached metadata and forces a full regeneration on next open. Book deletes, overwrites, and moves done through the firmware or web UI clear or re-key matching caches; manual SD-card edits may leave stale cache directories behind.

For more details on the internal file structures, see the [file formats document](./docs/file-formats.md).

---

## Contributing

Contributions are welcome. If you're new to the codebase, start with the [contributing docs](./docs/contributing/README.md). For things to work on, check the [ideas discussion board](https://github.com/crosspoint-reader/crosspoint-reader/discussions/categories/ideas) — leave a comment before starting so we don't duplicate effort.

Everyone here is a volunteer, so please be respectful and patient. For governance and community expectations, see [GOVERNANCE.md](./GOVERNANCE.md).

---

## Community forks

One of the best things about open source is that anyone can take the code in a different direction. If you need something outside CrossPoint's [scope](./SCOPE.md), check out the community forks:

- [CrossInk](https://github.com/uxjulia/CrossInk) — UX focused with minimal reading stats and broader customizations for the reading experience.

- [papyrix-reader](https://github.com/bigbag/papyrix-reader) — Adds FB2 and MD format support. Actively maintained with Arabic script support. Custom themes.

- [inx](https://github.com/obijuankenobiii/inx) — Completely reimagines the user interface with tabbed navigation.

- [Witch(hunt) Reader](https://github.com/jpirnay/witchhunt-reader) — More faithful CSS styling and background work for slightly snappier interaction. Weather information panel. Markdown support.

**Note:** Many of these features will make their way into CrossPoint over time. Each project chooses its own priorities and tradeoffs.

Want to build your own device? Be sure to check out the [Silkscreen](https://silkscreenreader.com) project or [OnePage Reader](https://github.com/MoveCall/onepage-reader).

---

CrossPoint Reader is **not affiliated with Xteink or any device manufacturer**.
