#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Pure core of the fork's OS (firmware) backup / restore feature.
//
// Everything in this header is free of HAL, Arduino and ESP-IDF types so the
// host tests in test/os_backup can pin the safety contract without a device:
// chunked streaming copies with incremental SHA-256, the ESP app-image
// length walk, manifest generation/parsing, and the fail-closed restore
// preconditions. The device-side adapter (src/OsBackupService) binds these
// to esp_partition / HalStorage / firmware_flash.
//
// Backup layout on SD (written by the adapter, described by the manifest):
//   /.crosspoint/backup/<name>/firmware.bin   running app partition image,
//                                             trimmed to the real image
//                                             length (not the padded slot)
//   /.crosspoint/backup/<name>/nvs.bin        raw NVS partition (recovery
//                                             artifact for PC-level repair;
//                                             NOT restored on-device)
//   /.crosspoint/backup/<name>/config/*.json  copies of /.crosspoint/*.json
//   /.crosspoint/backup/<name>/manifest.json  written LAST: its presence is
//                                             what makes a backup "valid"
namespace os_backup {

// SD paths / file names shared by the adapter, the UI and the emulator.
inline constexpr const char* kBackupRootPath = "/.crosspoint/backup";
inline constexpr const char* kFirmwareFileName = "firmware.bin";
inline constexpr const char* kNvsFileName = "nvs.bin";
inline constexpr const char* kManifestFileName = "manifest.json";
inline constexpr const char* kConfigDirName = "config";
inline constexpr const char* kManifestFormatTag = "crosspoint-os-backup";
inline constexpr int kManifestFormatVersion = 1;

// Streaming chunk for every copy/hash pass. 8 KiB matches the fork's
// Markdown streaming chunk and keeps peak RAM at one buffer + SHA state.
inline constexpr size_t kChunkSize = 8192;

// ---------------------------------------------------------------- SHA-256
// Small self-contained SHA-256 (FIPS 180-4) so host tests and firmware use
// bit-identical code with no mbedtls/host-crypto divergence.
class Sha256 {
 public:
  Sha256();
  void update(const uint8_t* data, size_t len);
  // Terminal: pads and writes the 32-byte digest. The object must not be
  // updated afterwards.
  void finish(uint8_t out[32]);
  std::string finishHex();

 private:
  uint32_t state_[8];
  uint64_t totalLen_ = 0;
  uint8_t block_[64];
  size_t blockLen_ = 0;
};

std::string sha256Hex(const uint8_t* data, size_t len);
std::string sha256Hex(const std::string& data);
bool isHex64(const std::string& s);

// ------------------------------------------------------- byte stream I/O
// Random-access byte source (a flash partition on device, a file in the
// emulator, memory in tests). readAt must fill exactly `len` bytes or fail.
class ByteSource {
 public:
  virtual ~ByteSource() = default;
  virtual size_t size() const = 0;
  virtual bool readAt(uint64_t offset, uint8_t* out, size_t len) = 0;
};

class ByteSink {
 public:
  virtual ~ByteSink() = default;
  virtual bool write(const uint8_t* data, size_t len) = 0;
};

// Progress callback: `done` bytes of `total` processed so far.
using ProgressCb = void (*)(size_t done, size_t total, void* ctx);

// Copy `length` bytes starting at `srcOffset`, streaming in `chunkSize`
// pieces (never a whole-image buffer), feeding SHA-256 as bytes pass.
// On success returns true and, when hashHexOut is non-null, the hex digest
// of exactly the copied range. Any read/write failure aborts with false.
bool copyAndHash(ByteSource& src, uint64_t srcOffset, uint64_t length, ByteSink& sink, size_t chunkSize,
                 std::string* hashHexOut, ProgressCb onProgress, void* ctx);

// Hash a range in place (used for the read-back verification pass and for
// restore pre-checks). Same streaming/progress contract as copyAndHash.
bool hashSource(ByteSource& src, uint64_t offset, uint64_t length, size_t chunkSize, std::string& hashHexOut,
                ProgressCb onProgress, void* ctx);

// ------------------------------------------------------ partition layout
struct PartitionInfo {
  std::string name;
  std::string type;
  std::string subtype;
  uint32_t offset = 0;
  uint32_t size = 0;
};

// Stable identifier of a partition table: SHA-256 hex over the canonical
// (offset-sorted) "name|type|subtype|offset|size" lines. A backup only
// restores onto a device whose computed id matches the manifest's, so an
// image built for a different layout (e.g. Picco's single slot) is refused
// before any flash write starts.
std::string partitionConfigId(const std::vector<PartitionInfo>& partitions);

// ---------------------------------------------------------- ESP app image
inline constexpr uint8_t kEspImageMagic = 0xE9;
inline constexpr size_t kEspHeaderSize = 24;
inline constexpr size_t kEspSegmentHeaderSize = 8;
inline constexpr size_t kSha256TrailerSize = 32;
inline constexpr uint8_t kMaxSegments = 16;

struct EspImageHeader {
  uint8_t segmentCount = 0;
  uint16_t chipId = 0;
  bool hashAppended = false;
};

// Parse the 24-byte esp_image_header_t prefix. False on short input, bad
// magic, or an implausible segment count (0 or > kMaxSegments).
bool parseEspImageHeader(const uint8_t* data, size_t len, EspImageHeader* out);

// Walk the segment table and compute the true image length, using exactly
// the layout math FirmwareFlasher::validateImageFile verifies on device:
//   pos = 24 + sum(8 + segmentDataLen)
//   padEnd = (pos + 16) & ~15      (checksum byte is the last pad byte)
//   total = padEnd + (hashAppended ? 32 : 0)
// False when the walk runs past the available bytes (truncated image).
bool computeEspImageLength(ByteSource& src, uint64_t* lengthOut);

// --------------------------------------------------------------- manifest
struct Manifest {
  int formatVersion = kManifestFormatVersion;
  std::string createdUtc;         // "YYYY-MM-DDTHH:MM:SS" (device RTC, may be "")
  std::string appVersion;         // CROSSPOINT_VERSION of the backed-up OS
  std::string board;              // board tag name, e.g. "x4"
  std::string runningSlot;        // "ota_0" / "ota_1" the image was read from
  std::string partitionConfigId;  // partitionConfigId() at backup time
  uint64_t firmwareSize = 0;
  std::string firmwareSha256;     // hex, over exactly firmwareSize bytes
  bool hasNvs = false;
  uint64_t nvsSize = 0;
  std::string nvsSha256;
  std::vector<std::string> configFiles;  // bare file names under config/
};

std::string manifestToJson(const Manifest& m);

// Strict-enough parser for the flat schema manifestToJson emits. Fails on
// a wrong format tag/version, missing fields, non-hex digests, or config
// file names that are not bare names (a manifest must never be able to
// point the restore outside the backup directory).
bool manifestFromJson(const std::string& json, Manifest* out, std::string* err);

// ------------------------------------------------------ restore pre-check
enum class RestoreCheck {
  Ok,
  PartitionConfigMismatch,  // different board / partition layout
  FirmwareSizeMismatch,     // file on SD is not the size the manifest claims
  ImageTooLarge,            // does not fit the destination OTA slot
  FirmwareHashMismatch,     // bytes on SD do not match the manifest digest
  ImageHeaderInvalid,       // not an ESP app image header
};

const char* restoreCheckName(RestoreCheck check);

// Fail-closed gate the restore path runs BEFORE any flash write: every
// input is measured fresh by the caller (current partition config id,
// destination slot capacity, actual SD file size, actual SD file SHA-256,
// first 24 bytes of the SD file). Any mismatch refuses the restore.
RestoreCheck checkRestore(const Manifest& m, const std::string& currentPartitionConfigId, uint64_t destCapacity,
                          uint64_t actualFirmwareSize, const std::string& actualFirmwareSha256,
                          const uint8_t* imageHeader24);

// --------------------------------------------------------- backup naming
// Backup directory names: "YYYYMMDD-HHMMSS" (optionally "-2", "-3", ... on
// collision) when the RTC is plausible, else "backup-NNN". The validator
// keeps path traversal out of every consumer of a directory listing.
bool isValidBackupName(const std::string& name);

// Ascending lexicographic order — timestamp names sort chronologically,
// sequence names sort numerically thanks to zero padding.
std::vector<std::string> sortBackupNames(std::vector<std::string> names);

// Given the sorted list, return the oldest entries to delete so that at
// most `keep` generations remain. keep == 0 disables pruning.
std::vector<std::string> selectPrune(const std::vector<std::string>& sortedNames, size_t keep);

}  // namespace os_backup
