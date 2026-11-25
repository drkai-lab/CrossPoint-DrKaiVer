#pragma once

#include <OsBackup.h>

#include <string>
#include <vector>

// Device-side adapter for the fork's OS backup / restore feature. All of the
// safety-critical pure logic (chunked SHA-256 copies, ESP image length walk,
// manifest handling, fail-closed restore preconditions) lives in
// lib/OsBackup; this layer only binds it to esp_partition, HalStorage and
// firmware_flash.
//
// What a backup contains (SD /.crosspoint/backup/<name>/):
//   firmware.bin  - the running app partition image, trimmed to its real
//                   ESP image length (the rest of the slot is 0xFF padding)
//   nvs.bin       - raw NVS partition, kept as a PC-level recovery artifact.
//                   It is NOT written back on-device: the live NVS driver
//                   owns that flash, and every user-facing store (settings,
//                   state, recents, Wi-Fi, OPDS) is a JSON file on SD.
//   config/*.json - copies of the /.crosspoint/*.json stores listed above
//   manifest.json - written LAST, after a read-back SHA-256 verification of
//                   firmware.bin/nvs.bin. Its presence marks a valid backup.
//
// Restore writes the image into the *inactive* OTA slot through the same
// firmware_flash::flashFromSdPath path the SD firmware update uses (raw
// partition write + otadata switch), so the running OS stays untouched
// until the bootloader picks the restored slot after a restart — and a
// failed restore never switches the boot partition.
namespace os_backup_service {

enum class Error {
  Ok,
  StorageNotReady,
  NoRunningPartition,
  NoNextPartition,
  ImageLengthFailed,
  CreateDirFailed,
  IoWriteFailed,
  IoReadFailed,
  VerifyFailed,       // read-back hash differed; backup dir removed
  BackupNotFound,
  ManifestInvalid,
  RestoreCheckFailed,  // lib gate refused; see RestoreCheck out-param
  ImageInvalid,        // firmware_flash::validateImageFile refused the image
  FlashFailed,         // flashFromSdPath failed; boot partition NOT switched
  ConfigRestoreFailed, // OS restored, but one or more config files failed
};

const char* errorName(Error);

struct BackupEntry {
  std::string name;  // directory name under /.crosspoint/backup
  os_backup::Manifest manifest;
  bool valid = false;  // manifest.json present and parsed
};

// SHA-256 partition-layout identifier of the *current* device, in the same
// canonical form manifests record. Empty string when no partitions found.
std::string currentPartitionConfigId();

// All backup generations on SD, ascending by name (chronological for
// timestamp names). Entries whose manifest is missing/corrupt are returned
// with valid=false so the UI can show but not offer them for restore.
std::vector<BackupEntry> listBackups();

// Create a new backup generation. Progress reports firmware-copy bytes.
// On any failure the partial directory is removed. outName receives the
// generation name on success.
Error createBackup(os_backup::ProgressCb onProgress, void* ctx, std::string* outName);

// Restore generation `name`. Runs the full gate before touching flash:
// manifest parse -> partition-config match -> SD size match -> SD SHA-256
// match -> ESP header check -> firmware_flash image validation -> flash to
// inactive slot + otadata switch -> config files copied back. When the lib
// gate refuses, *failedCheck (if non-null) says which precondition failed
// and no flash write has happened.
Error restoreBackup(const std::string& name, os_backup::ProgressCb onProgress, void* ctx,
                    os_backup::RestoreCheck* failedCheck);

// Remove one generation completely (files, config/, the directory itself).
Error deleteBackup(const std::string& name);

}  // namespace os_backup_service
