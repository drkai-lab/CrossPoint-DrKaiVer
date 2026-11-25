#include "OsBackupService.h"

#include <Arduino.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "CrossPointSettings.h"
#include "network/FirmwareBoardTag.h"
#include "network/FirmwareFlasher.h"

#ifndef CROSSPOINT_VERSION
#define CROSSPOINT_VERSION "unknown"
#endif

namespace os_backup_service {
namespace {

using namespace os_backup;

constexpr const char* kModule = "OSBACKUP";
// User-facing stores copied into every backup (bare names under
// /.crosspoint/). Missing files are skipped silently at backup time.
constexpr const char* kConfigCandidates[] = {"settings.json", "state.json", "recent.json", "wifi.json", "opds.json"};

// ByteSource over a flash partition (running app slot, NVS).
class PartitionSource final : public ByteSource {
 public:
  explicit PartitionSource(const esp_partition_t* part) : part_(part) {}
  size_t size() const override { return part_->size; }
  bool readAt(uint64_t offset, uint8_t* out, size_t len) override {
    if (offset + len > part_->size) return false;
    return esp_partition_read(part_, static_cast<size_t>(offset), out, len) == ESP_OK;
  }

 private:
  const esp_partition_t* part_;
};

// ByteSource over an open SD file.
class FileSource final : public ByteSource {
 public:
  explicit FileSource(HalFile&& file) : file_(std::move(file)) {}
  size_t size() const override { return static_cast<size_t>(file_.fileSize64()); }
  bool readAt(uint64_t offset, uint8_t* out, size_t len) override {
    if (!file_.seek64(offset)) return false;
    size_t got = 0;
    while (got < len) {
      const int r = file_.read(out + got, len - got);
      if (r <= 0) return false;
      got += static_cast<size_t>(r);
    }
    return true;
  }
  void close() { file_.close(); }

 private:
  mutable HalFile file_;  // size() is const; HalFile's accessors are not
};

// ByteSink over an open SD file (truncating write).
class FileSink final : public ByteSink {
 public:
  explicit FileSink(HalFile&& file) : file_(std::move(file)) {}
  bool write(const uint8_t* data, size_t len) override { return file_.write(data, len) == len; }
  void close() {
    file_.flush();
    file_.close();
  }

 private:
  HalFile file_;
};

std::vector<PartitionInfo> collectPartitions() {
  std::vector<PartitionInfo> out;
  esp_partition_iterator_t it =
      esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  for (; it; it = esp_partition_next(it)) {
    const esp_partition_t* p = esp_partition_get(it);
    if (!p) continue;
    PartitionInfo info;
    info.name = p->label ? p->label : "";
    info.type = std::to_string(static_cast<int>(p->type));
    info.subtype = std::to_string(static_cast<int>(p->subtype));
    info.offset = p->address;
    info.size = p->size;
    out.push_back(std::move(info));
  }
  esp_partition_iterator_release(it);
  return out;
}

std::string backupDirPath(const std::string& name) { return std::string(kBackupRootPath) + "/" + name; }

bool removeBackupContents(const std::string& name) {
  const std::string dir = backupDirPath(name);
  bool ok = true;
  for (const char* file : {kFirmwareFileName, kNvsFileName, kManifestFileName}) {
    const std::string path = dir + "/" + file;
    if (Storage.exists(path.c_str()) && !Storage.remove(path.c_str())) ok = false;
  }
  const std::string configDir = dir + "/" + kConfigDirName;
  for (const char* candidate : kConfigCandidates) {
    const std::string path = configDir + "/" + candidate;
    if (Storage.exists(path.c_str()) && !Storage.remove(path.c_str())) ok = false;
  }
  if (Storage.exists(configDir.c_str()) && !Storage.rmdir(configDir.c_str())) ok = false;
  if (Storage.exists(dir.c_str()) && !Storage.rmdir(dir.c_str())) ok = false;
  return ok;
}

// Chunked file -> file copy reusing the lib engine (hash discarded).
bool copyFile(const std::string& from, const std::string& to) {
  HalFile in;
  if (!Storage.openFileForRead(kModule, from, in) || !in) return false;
  HalFile out;
  if (!Storage.openFileForWrite(kModule, to, out) || !out) return false;
  FileSource src(std::move(in));
  FileSink sink(std::move(out));
  const bool ok = copyAndHash(src, 0, src.size(), sink, kChunkSize, nullptr, nullptr, nullptr);
  sink.close();
  src.close();
  return ok;
}

// Next free generation name: RTC timestamp when the clock looks real
// (year >= 2024), else a zero-padded sequence. Collisions get "-2", "-3".
std::string nextBackupName() {
  struct tm tmNow {};
  std::string base;
  if (halClock.isAvailable() && halClock.localTime(tmNow) && tmNow.tm_year >= 124) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d", tmNow.tm_year + 1900, tmNow.tm_mon + 1,
                  tmNow.tm_mday, tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
    base = buf;
  } else {
    const auto existing = listBackups();
    char buf[32];
    std::snprintf(buf, sizeof(buf), "backup-%03u", static_cast<unsigned>(existing.size() + 1));
    base = buf;
  }
  std::string name = base;
  for (int suffix = 2; Storage.exists(backupDirPath(name).c_str()); suffix++) {
    name = base + "-" + std::to_string(suffix);
  }
  return name;
}

std::string utcTimestampString() {
  struct tm tmNow {};
  if (halClock.isAvailable() && halClock.localTime(tmNow) && tmNow.tm_year >= 124) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d", tmNow.tm_year + 1900, tmNow.tm_mon + 1,
                  tmNow.tm_mday, tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
    return buf;
  }
  return "";
}

}  // namespace

const char* errorName(Error e) {
  switch (e) {
    case Error::Ok: return "OK";
    case Error::StorageNotReady: return "STORAGE_NOT_READY";
    case Error::NoRunningPartition: return "NO_RUNNING_PARTITION";
    case Error::NoNextPartition: return "NO_NEXT_PARTITION";
    case Error::ImageLengthFailed: return "IMAGE_LENGTH_FAILED";
    case Error::CreateDirFailed: return "CREATE_DIR_FAILED";
    case Error::IoWriteFailed: return "IO_WRITE_FAILED";
    case Error::IoReadFailed: return "IO_READ_FAILED";
    case Error::VerifyFailed: return "VERIFY_FAILED";
    case Error::BackupNotFound: return "BACKUP_NOT_FOUND";
    case Error::ManifestInvalid: return "MANIFEST_INVALID";
    case Error::RestoreCheckFailed: return "RESTORE_CHECK_FAILED";
    case Error::ImageInvalid: return "IMAGE_INVALID";
    case Error::FlashFailed: return "FLASH_FAILED";
    case Error::ConfigRestoreFailed: return "CONFIG_RESTORE_FAILED";
  }
  return "UNKNOWN";
}

std::string currentPartitionConfigId() {
  const auto partitions = collectPartitions();
  if (partitions.empty()) return "";
  return partitionConfigId(partitions);
}

std::vector<BackupEntry> listBackups() {
  std::vector<BackupEntry> out;
  auto root = Storage.open(kBackupRootPath);
  if (!root || !root.isDirectory()) return out;
  root.rewindDirectory();
  char nameBuf[64];
  for (auto entry = root.openNextFile(); entry; entry = root.openNextFile()) {
    const bool isDir = entry.isDirectory();
    entry.getName(nameBuf, sizeof(nameBuf));
    entry.close();
    if (!isDir || !isValidBackupName(nameBuf)) continue;
    BackupEntry item;
    item.name = nameBuf;
    std::string json;
    const std::string manifestPath = backupDirPath(item.name) + "/" + kManifestFileName;
    std::string parseErr;
    if (Storage.readFileToString(kModule, manifestPath, 8192, json) &&
        manifestFromJson(json, &item.manifest, &parseErr)) {
      item.valid = true;
    }
    out.push_back(std::move(item));
  }
  root.close();
  std::sort(out.begin(), out.end(),
            [](const BackupEntry& a, const BackupEntry& b) { return a.name < b.name; });
  return out;
}

Error createBackup(ProgressCb onProgress, void* ctx, std::string* outName) {
  if (!Storage.ready()) return Error::StorageNotReady;

  const esp_partition_t* running = esp_ota_get_running_partition();
  if (!running) return Error::NoRunningPartition;
  if (running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 && running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_1) {
    // Single-slot layouts (Picco) have no inactive slot to restore into;
    // the Settings menu hides this feature there, so this is defense only.
    LOG_ERR(kModule, "running partition is not an OTA slot");
    return Error::NoNextPartition;
  }
  PartitionSource runningSrc(running);
  uint64_t imageLen = 0;
  if (!computeEspImageLength(runningSrc, &imageLen) || imageLen == 0) {
    LOG_ERR(kModule, "cannot determine running image length");
    return Error::ImageLengthFailed;
  }

  const std::string name = nextBackupName();
  const std::string dir = backupDirPath(name);
  if (!Storage.ensureDirectoryExists(kBackupRootPath) || !Storage.mkdir(dir.c_str()) ||
      !Storage.mkdir((dir + "/" + kConfigDirName).c_str())) {
    return Error::CreateDirFailed;
  }
  auto bail = [&](Error e) {
    removeBackupContents(name);
    return e;
  };

  // 1) Firmware image: partition -> SD, hashing as bytes pass.
  std::string firmwareHash;
  {
    HalFile file;
    const std::string path = dir + "/" + kFirmwareFileName;
    if (!Storage.openFileForWrite(kModule, path, file) || !file) return bail(Error::IoWriteFailed);
    FileSink sink(std::move(file));
    const bool ok = copyAndHash(runningSrc, 0, imageLen, sink, kChunkSize, &firmwareHash, onProgress, ctx);
    sink.close();
    if (!ok) return bail(Error::IoWriteFailed);
  }

  // 2) NVS raw image (recovery artifact; never restored on-device).
  Manifest manifest;
  const esp_partition_t* nvs =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, nullptr);
  if (nvs) {
    PartitionSource nvsSrc(nvs);
    HalFile file;
    const std::string path = dir + "/" + kNvsFileName;
    if (!Storage.openFileForWrite(kModule, path, file) || !file) return bail(Error::IoWriteFailed);
    FileSink sink(std::move(file));
    std::string nvsHash;
    const bool ok = copyAndHash(nvsSrc, 0, nvs->size, sink, kChunkSize, &nvsHash, nullptr, nullptr);
    sink.close();
    if (!ok) return bail(Error::IoWriteFailed);
    manifest.hasNvs = true;
    manifest.nvsSize = nvs->size;
    manifest.nvsSha256 = nvsHash;
  }

  // 3) Config JSON stores.
  for (const char* candidate : kConfigCandidates) {
    const std::string from = std::string("/.crosspoint/") + candidate;
    if (!Storage.exists(from.c_str())) continue;
    const std::string to = dir + "/" + kConfigDirName + "/" + candidate;
    if (copyFile(from, to)) {
      manifest.configFiles.push_back(candidate);
    } else {
      LOG_ERR(kModule, "config copy failed: %s", candidate);
    }
  }

  // 4) Read-back verification of what actually landed on SD. A backup that
  //    fails here is removed, never left behind as if it were valid.
  {
    HalFile file;
    const std::string path = dir + "/" + kFirmwareFileName;
    if (!Storage.openFileForRead(kModule, path, file) || !file) return bail(Error::IoReadFailed);
    FileSource src(std::move(file));
    std::string readBack;
    const bool ok = hashSource(src, 0, imageLen, kChunkSize, readBack, nullptr, nullptr);
    src.close();
    if (!ok || readBack != firmwareHash) {
      LOG_ERR(kModule, "firmware read-back verify failed");
      return bail(Error::VerifyFailed);
    }
  }
  if (manifest.hasNvs) {
    HalFile file;
    const std::string path = dir + "/" + kNvsFileName;
    if (!Storage.openFileForRead(kModule, path, file) || !file) return bail(Error::IoReadFailed);
    FileSource src(std::move(file));
    std::string readBack;
    const bool ok = hashSource(src, 0, manifest.nvsSize, kChunkSize, readBack, nullptr, nullptr);
    src.close();
    if (!ok || readBack != manifest.nvsSha256) {
      LOG_ERR(kModule, "nvs read-back verify failed");
      return bail(Error::VerifyFailed);
    }
  }

  // 5) Manifest last: its presence is the validity marker.
  manifest.createdUtc = utcTimestampString();
  manifest.appVersion = CROSSPOINT_VERSION;
  manifest.board = std::string(board_tag::boardName(), board_tag::boardNameLen());
  manifest.runningSlot =
      running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ? "ota_0" : "ota_1";
  manifest.partitionConfigId = currentPartitionConfigId();
  manifest.firmwareSize = imageLen;
  manifest.firmwareSha256 = firmwareHash;
  const std::string manifestPath = dir + "/" + kManifestFileName;
  if (!Storage.writeFile(manifestPath.c_str(), String(manifestToJson(manifest).c_str()))) {
    return bail(Error::IoWriteFailed);
  }

  // 6) Prune old generations down to the configured keep count.
  const uint8_t keep = SETTINGS.osBackupKeep;
  if (keep > 0) {
    const auto entries = listBackups();
    std::vector<std::string> names;
    for (const auto& e : entries) names.push_back(e.name);
    for (const auto& old : selectPrune(sortBackupNames(names), keep)) {
      if (old != name) deleteBackup(old);
    }
  }

  LOG_INF(kModule, "backup created: %s (%u bytes)", name.c_str(), static_cast<unsigned>(imageLen));
  if (outName) *outName = name;
  return Error::Ok;
}

Error restoreBackup(const std::string& name, ProgressCb onProgress, void* ctx, RestoreCheck* failedCheck) {
  if (failedCheck) *failedCheck = RestoreCheck::Ok;
  if (!Storage.ready()) return Error::StorageNotReady;
  if (!isValidBackupName(name)) return Error::BackupNotFound;
  const std::string dir = backupDirPath(name);
  if (!Storage.exists(dir.c_str())) return Error::BackupNotFound;

  std::string json;
  Manifest manifest;
  std::string parseErr;
  const std::string manifestPath = dir + "/" + kManifestFileName;
  if (!Storage.readFileToString(kModule, manifestPath, 8192, json) ||
      !manifestFromJson(json, &manifest, &parseErr)) {
    LOG_ERR(kModule, "manifest invalid for %s: %s", name.c_str(), parseErr.c_str());
    return Error::ManifestInvalid;
  }

  const esp_partition_t* dest = esp_ota_get_next_update_partition(nullptr);
  if (!dest) return Error::NoNextPartition;

  const std::string firmwarePath = dir + "/" + kFirmwareFileName;
  HalFile file;
  if (!Storage.openFileForRead(kModule, firmwarePath, file) || !file) return Error::IoReadFailed;
  FileSource src(std::move(file));
  const uint64_t actualSize = src.size();
  std::string actualHash;
  if (!hashSource(src, 0, actualSize, kChunkSize, actualHash, nullptr, nullptr)) {
    src.close();
    return Error::IoReadFailed;
  }
  uint8_t header[kEspHeaderSize] = {};
  const bool headerOk = src.readAt(0, header, sizeof(header));
  src.close();
  if (!headerOk) return Error::IoReadFailed;

  const RestoreCheck check =
      checkRestore(manifest, currentPartitionConfigId(), dest->size, actualSize, actualHash, header);
  if (check != RestoreCheck::Ok) {
    LOG_ERR(kModule, "restore refused for %s: %s", name.c_str(), restoreCheckName(check));
    if (failedCheck) *failedCheck = check;
    return Error::RestoreCheckFailed;
  }

  // Defense in depth: the same full image-integrity validation the SD
  // firmware update runs (header, segment walk, XOR checksum, SHA trailer,
  // chip id, board tag) before flashFromSdPath is allowed to write.
  const auto vr = firmware_flash::validateImageFile(firmwarePath.c_str(), dest->size);
  if (vr != firmware_flash::Result::OK) {
    LOG_ERR(kModule, "image validation failed for %s: %s", name.c_str(), firmware_flash::resultName(vr));
    return Error::ImageInvalid;
  }

  // Flash into the inactive slot and switch otadata. alreadyValidated=false:
  // flashFromSdPath re-validates the file itself (SD is removable media;
  // do not trust the pass above — same stance as SdFirmwareUpdateActivity).
  const auto fr = firmware_flash::flashFromSdPath(firmwarePath.c_str(), onProgress, ctx, false);
  if (fr != firmware_flash::Result::OK) {
    LOG_ERR(kModule, "flash failed for %s: %s", name.c_str(), firmware_flash::resultName(fr));
    return Error::FlashFailed;  // otadata untouched: current OS still boots
  }

  // Config stores last, immediately before the caller restarts, so nothing
  // re-saves the live settings over the restored ones in between.
  bool configOk = true;
  for (const auto& configName : manifest.configFiles) {
    const std::string from = dir + "/" + kConfigDirName + "/" + configName;
    const std::string to = std::string("/.crosspoint/") + configName;
    if (!Storage.exists(from.c_str()) || !copyFile(from, to)) {
      LOG_ERR(kModule, "config restore failed: %s", configName.c_str());
      configOk = false;
    }
  }
  LOG_INF(kModule, "restore complete: %s", name.c_str());
  return configOk ? Error::Ok : Error::ConfigRestoreFailed;
}

Error deleteBackup(const std::string& name) {
  if (!isValidBackupName(name)) return Error::BackupNotFound;
  if (!Storage.exists(backupDirPath(name).c_str())) return Error::BackupNotFound;
  return removeBackupContents(name) ? Error::Ok : Error::IoWriteFailed;
}

}  // namespace os_backup_service
