#include "OsBackup.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace os_backup {

namespace {

// ------------------------------------------------------------ SHA-256 core
constexpr uint32_t kShaK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void shaTransform(uint32_t state[8], const uint8_t* block) {
  uint32_t w[64];
  for (int i = 0; i < 16; i++) {
    w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) | (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | static_cast<uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; i++) {
    const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
  for (int i = 0; i < 64; i++) {
    const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const uint32_t ch = (e & f) ^ (~e & g);
    const uint32_t t1 = h + s1 + ch + kShaK[i] + w[i];
    const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t t2 = s0 + maj;
    h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  state[0] += a; state[1] += b; state[2] += c; state[3] += d;
  state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

std::string toHex(const uint8_t* data, size_t len) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(len * 2);
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = kHex[data[i] >> 4];
    out[i * 2 + 1] = kHex[data[i] & 0xF];
  }
  return out;
}

uint32_t readLe32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

// ------------------------------------------------------------ JSON helpers
std::string jsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 2);
  for (const char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<uint8_t>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

// Find `"key"` followed by ':' at or after `from`; returns the offset just
// past the colon, or npos. Keys in our flat schema are unique.
size_t findValuePos(const std::string& json, const char* key, size_t from = 0) {
  const std::string needle = std::string("\"") + key + "\"";
  size_t pos = json.find(needle, from);
  if (pos == std::string::npos) return std::string::npos;
  pos += needle.size();
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == ':')) pos++;
  return pos;
}

bool parseJsonString(const std::string& json, size_t pos, std::string* out, size_t* endPos) {
  if (pos >= json.size() || json[pos] != '"') return false;
  pos++;
  std::string result;
  while (pos < json.size()) {
    const char c = json[pos++];
    if (c == '"') {
      *out = result;
      if (endPos) *endPos = pos;
      return true;
    }
    if (c == '\\') {
      if (pos >= json.size()) return false;
      const char esc = json[pos++];
      switch (esc) {
        case '"': result += '"'; break;
        case '\\': result += '\\'; break;
        case '/': result += '/'; break;
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 't': result += '\t'; break;
        case 'b': result += '\b'; break;
        case 'f': result += '\f'; break;
        case 'u': {
          if (pos + 4 > json.size()) return false;
          unsigned code = 0;
          for (int i = 0; i < 4; i++) {
            const char h = json[pos++];
            code <<= 4;
            if (h >= '0' && h <= '9') code += static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f') code += static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') code += static_cast<unsigned>(h - 'A' + 10);
            else return false;
          }
          // Our writer only escapes control characters (< 0x20); anything
          // beyond single-byte range is not part of the schema.
          if (code > 0x7F) return false;
          result += static_cast<char>(code);
          break;
        }
        default: return false;
      }
    } else {
      result += c;
    }
  }
  return false;  // unterminated
}

bool jsonStringField(const std::string& json, const char* key, std::string* out) {
  const size_t pos = findValuePos(json, key);
  if (pos == std::string::npos) return false;
  return parseJsonString(json, pos, out, nullptr);
}

bool jsonUintField(const std::string& json, const char* key, uint64_t* out) {
  const size_t pos = findValuePos(json, key);
  if (pos == std::string::npos || pos >= json.size()) return false;
  if (json[pos] < '0' || json[pos] > '9') return false;
  uint64_t value = 0;
  size_t i = pos;
  while (i < json.size() && json[i] >= '0' && json[i] <= '9') {
    value = value * 10 + static_cast<uint64_t>(json[i] - '0');
    i++;
  }
  *out = value;
  return true;
}

bool jsonBoolField(const std::string& json, const char* key, bool* out) {
  const size_t pos = findValuePos(json, key);
  if (pos == std::string::npos) return false;
  if (json.compare(pos, 4, "true") == 0) {
    *out = true;
    return true;
  }
  if (json.compare(pos, 5, "false") == 0) {
    *out = false;
    return true;
  }
  return false;
}

bool isBareFileName(const std::string& name) {
  if (name.empty() || name.size() > 64) return false;
  if (name == "." || name == "..") return false;
  for (const char c : name) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                    c == '-' || c == '_';
    if (!ok) return false;
  }
  return true;
}

}  // namespace

// ------------------------------------------------------------------ Sha256
Sha256::Sha256()
    : state_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19} {}

void Sha256::update(const uint8_t* data, size_t len) {
  totalLen_ += len;
  while (len > 0) {
    const size_t take = std::min(len, sizeof(block_) - blockLen_);
    std::memcpy(block_ + blockLen_, data, take);
    blockLen_ += take;
    data += take;
    len -= take;
    if (blockLen_ == sizeof(block_)) {
      shaTransform(state_, block_);
      blockLen_ = 0;
    }
  }
}

void Sha256::finish(uint8_t out[32]) {
  const uint64_t bitLen = totalLen_ * 8;
  const uint8_t padStart = 0x80;
  update(&padStart, 1);
  const uint8_t zero = 0;
  while (blockLen_ != 56) update(&zero, 1);
  uint8_t lenBytes[8];
  for (int i = 0; i < 8; i++) lenBytes[i] = static_cast<uint8_t>((bitLen >> (56 - 8 * i)) & 0xFF);
  // Feed the length directly (update() would count it into totalLen_ too,
  // but the digest is already fully determined by the padded block).
  for (int i = 0; i < 8; i++) {
    block_[blockLen_++] = lenBytes[i];
    if (blockLen_ == sizeof(block_)) {
      shaTransform(state_, block_);
      blockLen_ = 0;
    }
  }
  for (int i = 0; i < 8; i++) {
    out[i * 4] = static_cast<uint8_t>(state_[i] >> 24);
    out[i * 4 + 1] = static_cast<uint8_t>(state_[i] >> 16);
    out[i * 4 + 2] = static_cast<uint8_t>(state_[i] >> 8);
    out[i * 4 + 3] = static_cast<uint8_t>(state_[i]);
  }
}

std::string Sha256::finishHex() {
  uint8_t digest[32];
  finish(digest);
  return toHex(digest, sizeof(digest));
}

std::string sha256Hex(const uint8_t* data, size_t len) {
  Sha256 h;
  if (len > 0) h.update(data, len);
  return h.finishHex();
}

std::string sha256Hex(const std::string& data) {
  return sha256Hex(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

bool isHex64(const std::string& s) {
  if (s.size() != 64) return false;
  for (const char c : s) {
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    if (!ok) return false;
  }
  return true;
}

// ------------------------------------------------------------- streaming I/O
bool copyAndHash(ByteSource& src, uint64_t srcOffset, uint64_t length, ByteSink& sink, size_t chunkSize,
                 std::string* hashHexOut, ProgressCb onProgress, void* ctx) {
  if (chunkSize == 0) chunkSize = kChunkSize;
  if (srcOffset + length > src.size()) return false;
  auto buf = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[chunkSize]);
  if (!buf) return false;
  Sha256 hash;
  uint64_t done = 0;
  while (done < length) {
    const size_t want = static_cast<size_t>(std::min<uint64_t>(chunkSize, length - done));
    if (!src.readAt(srcOffset + done, buf.get(), want)) return false;
    if (!sink.write(buf.get(), want)) return false;
    hash.update(buf.get(), want);
    done += want;
    if (onProgress) onProgress(static_cast<size_t>(done), static_cast<size_t>(length), ctx);
  }
  if (hashHexOut) *hashHexOut = hash.finishHex();
  return true;
}

bool hashSource(ByteSource& src, uint64_t offset, uint64_t length, size_t chunkSize, std::string& hashHexOut,
                ProgressCb onProgress, void* ctx) {
  if (chunkSize == 0) chunkSize = kChunkSize;
  if (offset + length > src.size()) return false;
  auto buf = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[chunkSize]);
  if (!buf) return false;
  Sha256 hash;
  uint64_t done = 0;
  while (done < length) {
    const size_t want = static_cast<size_t>(std::min<uint64_t>(chunkSize, length - done));
    if (!src.readAt(offset + done, buf.get(), want)) return false;
    hash.update(buf.get(), want);
    done += want;
    if (onProgress) onProgress(static_cast<size_t>(done), static_cast<size_t>(length), ctx);
  }
  hashHexOut = hash.finishHex();
  return true;
}

// ---------------------------------------------------------- partition layout
std::string partitionConfigId(const std::vector<PartitionInfo>& partitions) {
  std::vector<PartitionInfo> sorted = partitions;
  std::sort(sorted.begin(), sorted.end(), [](const PartitionInfo& a, const PartitionInfo& b) {
    if (a.offset != b.offset) return a.offset < b.offset;
    return a.name < b.name;
  });
  std::string canonical;
  for (const auto& p : sorted) {
    canonical += p.name;
    canonical += '|';
    canonical += p.type;
    canonical += '|';
    canonical += p.subtype;
    canonical += '|';
    canonical += std::to_string(p.offset);
    canonical += '|';
    canonical += std::to_string(p.size);
    canonical += '\n';
  }
  return sha256Hex(canonical);
}

// -------------------------------------------------------------- ESP app image
bool parseEspImageHeader(const uint8_t* data, size_t len, EspImageHeader* out) {
  if (!data || !out || len < kEspHeaderSize) return false;
  if (data[0] != kEspImageMagic) return false;
  const uint8_t segments = data[1];
  if (segments == 0 || segments > kMaxSegments) return false;
  out->segmentCount = segments;
  out->chipId = static_cast<uint16_t>(data[12] | (static_cast<uint16_t>(data[13]) << 8));
  out->hashAppended = data[23] != 0;
  return true;
}

bool computeEspImageLength(ByteSource& src, uint64_t* lengthOut) {
  const uint64_t available = src.size();
  if (available < kEspHeaderSize) return false;
  uint8_t header[kEspHeaderSize];
  if (!src.readAt(0, header, sizeof(header))) return false;
  EspImageHeader hdr;
  if (!parseEspImageHeader(header, sizeof(header), &hdr)) return false;

  uint64_t pos = kEspHeaderSize;
  for (uint8_t i = 0; i < hdr.segmentCount; i++) {
    if (pos + kEspSegmentHeaderSize > available) return false;
    uint8_t segHdr[kEspSegmentHeaderSize];
    if (!src.readAt(pos, segHdr, sizeof(segHdr))) return false;
    const uint32_t dataLen = readLe32(segHdr + 4);
    pos += kEspSegmentHeaderSize;
    if (pos + dataLen > available) return false;
    pos += dataLen;
  }
  const uint64_t padEnd = (pos + 16) & ~static_cast<uint64_t>(15);
  const uint64_t total = padEnd + (hdr.hashAppended ? kSha256TrailerSize : 0);
  if (total > available) return false;
  if (lengthOut) *lengthOut = total;
  return true;
}

// ------------------------------------------------------------------ manifest
std::string manifestToJson(const Manifest& m) {
  std::string out;
  out += "{\n";
  out += "  \"format\": \"" + std::string(kManifestFormatTag) + "\",\n";
  out += "  \"formatVersion\": " + std::to_string(m.formatVersion) + ",\n";
  out += "  \"createdUtc\": \"" + jsonEscape(m.createdUtc) + "\",\n";
  out += "  \"appVersion\": \"" + jsonEscape(m.appVersion) + "\",\n";
  out += "  \"board\": \"" + jsonEscape(m.board) + "\",\n";
  out += "  \"runningSlot\": \"" + jsonEscape(m.runningSlot) + "\",\n";
  out += "  \"partitionConfigId\": \"" + jsonEscape(m.partitionConfigId) + "\",\n";
  out += "  \"firmwareSize\": " + std::to_string(m.firmwareSize) + ",\n";
  out += "  \"firmwareSha256\": \"" + jsonEscape(m.firmwareSha256) + "\",\n";
  out += "  \"hasNvs\": " + std::string(m.hasNvs ? "true" : "false") + ",\n";
  out += "  \"nvsSize\": " + std::to_string(m.nvsSize) + ",\n";
  out += "  \"nvsSha256\": \"" + jsonEscape(m.nvsSha256) + "\",\n";
  out += "  \"configFiles\": [";
  for (size_t i = 0; i < m.configFiles.size(); i++) {
    if (i > 0) out += ", ";
    out += "\"" + jsonEscape(m.configFiles[i]) + "\"";
  }
  out += "]\n";
  out += "}\n";
  return out;
}

bool manifestFromJson(const std::string& json, Manifest* out, std::string* err) {
  auto fail = [&](const char* msg) {
    if (err) *err = msg;
    return false;
  };
  if (!out) return false;
  Manifest m;

  std::string format;
  if (!jsonStringField(json, "format", &format) || format != kManifestFormatTag) {
    return fail("format tag missing or not a crosspoint OS backup");
  }
  uint64_t version = 0;
  if (!jsonUintField(json, "formatVersion", &version) || version != static_cast<uint64_t>(kManifestFormatVersion)) {
    return fail("unsupported manifest formatVersion");
  }
  m.formatVersion = static_cast<int>(version);
  if (!jsonStringField(json, "createdUtc", &m.createdUtc)) return fail("createdUtc missing");
  if (!jsonStringField(json, "appVersion", &m.appVersion) || m.appVersion.empty()) return fail("appVersion missing");
  if (!jsonStringField(json, "board", &m.board) || m.board.empty()) return fail("board missing");
  if (!jsonStringField(json, "runningSlot", &m.runningSlot)) return fail("runningSlot missing");
  if (m.runningSlot != "ota_0" && m.runningSlot != "ota_1") return fail("runningSlot not an OTA slot");
  if (!jsonStringField(json, "partitionConfigId", &m.partitionConfigId) || !isHex64(m.partitionConfigId)) {
    return fail("partitionConfigId missing or not a SHA-256 hex digest");
  }
  if (!jsonUintField(json, "firmwareSize", &m.firmwareSize) || m.firmwareSize == 0) {
    return fail("firmwareSize missing or zero");
  }
  if (!jsonStringField(json, "firmwareSha256", &m.firmwareSha256) || !isHex64(m.firmwareSha256)) {
    return fail("firmwareSha256 missing or not a SHA-256 hex digest");
  }
  if (!jsonBoolField(json, "hasNvs", &m.hasNvs)) return fail("hasNvs missing");
  if (!jsonUintField(json, "nvsSize", &m.nvsSize)) return fail("nvsSize missing");
  if (!jsonStringField(json, "nvsSha256", &m.nvsSha256)) return fail("nvsSha256 missing");
  if (m.hasNvs) {
    if (m.nvsSize == 0) return fail("hasNvs set but nvsSize is zero");
    if (!isHex64(m.nvsSha256)) return fail("nvsSha256 not a SHA-256 hex digest");
  }

  // configFiles: flat array of bare file names.
  m.configFiles.clear();
  const size_t arrPos = findValuePos(json, "configFiles");
  if (arrPos == std::string::npos || arrPos >= json.size() || json[arrPos] != '[') {
    return fail("configFiles missing or not an array");
  }
  size_t pos = arrPos + 1;
  while (true) {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r' ||
                                 json[pos] == ',')) {
      pos++;
    }
    if (pos >= json.size()) return fail("configFiles array unterminated");
    if (json[pos] == ']') break;
    std::string name;
    size_t endPos = pos;
    if (!parseJsonString(json, pos, &name, &endPos)) return fail("configFiles entry is not a string");
    if (!isBareFileName(name)) return fail("configFiles entry is not a bare file name");
    m.configFiles.push_back(name);
    pos = endPos;
  }

  *out = m;
  return true;
}

// ----------------------------------------------------------- restore pre-check
const char* restoreCheckName(RestoreCheck check) {
  switch (check) {
    case RestoreCheck::Ok: return "OK";
    case RestoreCheck::PartitionConfigMismatch: return "PARTITION_CONFIG_MISMATCH";
    case RestoreCheck::FirmwareSizeMismatch: return "FIRMWARE_SIZE_MISMATCH";
    case RestoreCheck::ImageTooLarge: return "IMAGE_TOO_LARGE";
    case RestoreCheck::FirmwareHashMismatch: return "FIRMWARE_HASH_MISMATCH";
    case RestoreCheck::ImageHeaderInvalid: return "IMAGE_HEADER_INVALID";
  }
  return "UNKNOWN";
}

RestoreCheck checkRestore(const Manifest& m, const std::string& currentPartitionConfigId, uint64_t destCapacity,
                          uint64_t actualFirmwareSize, const std::string& actualFirmwareSha256,
                          const uint8_t* imageHeader24) {
  if (currentPartitionConfigId != m.partitionConfigId) return RestoreCheck::PartitionConfigMismatch;
  if (actualFirmwareSize != m.firmwareSize) return RestoreCheck::FirmwareSizeMismatch;
  if (m.firmwareSize > destCapacity) return RestoreCheck::ImageTooLarge;
  if (actualFirmwareSha256 != m.firmwareSha256) return RestoreCheck::FirmwareHashMismatch;
  EspImageHeader hdr;
  if (!parseEspImageHeader(imageHeader24, kEspHeaderSize, &hdr)) return RestoreCheck::ImageHeaderInvalid;
  return RestoreCheck::Ok;
}

// -------------------------------------------------------------- backup naming
bool isValidBackupName(const std::string& name) {
  if (name.empty() || name.size() > 40) return false;
  const char first = name.front();
  const bool firstOk =
      (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || (first >= '0' && first <= '9');
  if (!firstOk) return false;
  if (name.find("..") != std::string::npos) return false;
  for (const char c : name) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                    c == '-' || c == '_';
    if (!ok) return false;
  }
  return true;
}

std::vector<std::string> sortBackupNames(std::vector<std::string> names) {
  std::sort(names.begin(), names.end());
  names.erase(std::remove_if(names.begin(), names.end(),
                             [](const std::string& n) { return !isValidBackupName(n); }),
              names.end());
  return names;
}

std::vector<std::string> selectPrune(const std::vector<std::string>& sortedNames, size_t keep) {
  if (keep == 0 || sortedNames.size() <= keep) return {};
  return std::vector<std::string>(sortedNames.begin(), sortedNames.begin() + (sortedNames.size() - keep));
}

}  // namespace os_backup
