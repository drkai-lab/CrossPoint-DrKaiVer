// Host tests for lib/OsBackup — the pure core of the fork's OS backup /
// restore feature. Written before the implementation (Shift-Left): the
// contracts pinned here are
//   * SHA-256 matches the published test vectors, streamed in any chunking.
//   * Chunked copy never holds the whole image and reports progress.
//   * The ESP image length walk matches the bootloader layout math that
//     FirmwareFlasher::validateImageFile uses (padEnd = (pos + 16) & ~15,
//     plus a 32-byte SHA trailer when hash_appended is set).
//   * A manifest round-trips, and every restore precondition (partition
//     configuration, size, hash, image header) fails closed with its own
//     distinct result so the UI can say *why* a restore was refused.
//   * Backup generation listing is name-sorted (timestamp names sort
//     chronologically) and the prune plan keeps the newest N.

#include <OsBackup.h>
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

using namespace os_backup;

// In-memory ByteSource over a std::string.
class MemSource final : public ByteSource {
 public:
  explicit MemSource(const std::string& data) : data_(data) {}
  size_t size() const override { return data_.size(); }
  bool readAt(uint64_t offset, uint8_t* out, size_t len) override {
    if (offset + len > data_.size()) return false;
    std::memcpy(out, data_.data() + offset, len);
    return true;
  }

 private:
  const std::string& data_;
};

class MemSink final : public ByteSink {
 public:
  bool write(const uint8_t* data, size_t len) override {
    if (failAfter >= 0 && static_cast<int64_t>(bytes.size() + len) > failAfter) return false;
    bytes.append(reinterpret_cast<const char*>(data), len);
    return true;
  }
  std::string bytes;
  int64_t failAfter = -1;  // >=0: refuse writes that would pass this offset
};

std::string pattern(size_t n, uint32_t seed = 7) {
  std::string s;
  s.resize(n);
  uint32_t x = seed;
  for (size_t i = 0; i < n; i++) {
    x = x * 1664525u + 1013904223u;
    s[i] = static_cast<char>((x >> 24) & 0xFF);
  }
  return s;
}

// Build a minimal but layout-accurate ESP app image: 24-byte header,
// two segments, 16-byte-aligned padding with the checksum byte last of
// the pad, and an optional 32-byte SHA-256 trailer.
std::string makeEspImage(bool hashAppended, uint16_t chipId = 0x0005) {
  std::string img;
  auto u8 = [&](uint8_t v) { img.push_back(static_cast<char>(v)); };
  auto u16 = [&](uint16_t v) {
    u8(static_cast<uint8_t>(v & 0xFF));
    u8(static_cast<uint8_t>(v >> 8));
  };
  auto u32 = [&](uint32_t v) {
    for (int i = 0; i < 4; i++) u8(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
  };
  // esp_image_header_t (24 bytes)
  u8(0xE9);          // magic
  u8(2);             // segment count
  u8(0);             // flash mode
  u8(0);             // flash size/freq
  u32(0x40000000);   // entry addr
  u8(0);             // wp pin
  u8(0); u8(0); u8(0);  // spi pin drv
  u16(chipId);       // chip id @12
  u8(0);             // min chip rev
  for (int i = 0; i < 8; i++) u8(0);  // reserved
  u8(hashAppended ? 1 : 0);           // hash_appended @23
  // segment 1: 100 bytes
  u32(0x3C000000); u32(100);
  img += pattern(100, 11);
  // segment 2: 37 bytes
  u32(0x42000000); u32(37);
  img += pattern(37, 23);
  // pad: checksum byte is the last byte of the 16-byte aligned pad block
  const size_t padEnd = (img.size() + 16) & ~static_cast<size_t>(15);
  while (img.size() < padEnd - 1) u8(0x00);
  u8(0xEF);  // checksum byte (value irrelevant to the length walk)
  if (hashAppended) img += pattern(32, 31);  // SHA-256 trailer
  return img;
}

Manifest sampleManifest() {
  Manifest m;
  m.createdUtc = "2026-11-05T13:12:44";
  m.appVersion = "2.0.0";
  m.board = "x4";
  m.runningSlot = "ota_0";
  m.partitionConfigId = std::string(64, 'a');
  m.firmwareSize = 5169719;
  m.firmwareSha256 = std::string(64, 'b');
  m.hasNvs = true;
  m.nvsSize = 20480;
  m.nvsSha256 = std::string(64, 'c');
  m.configFiles = {"settings.json", "state.json", "wifi.json"};
  return m;
}

TEST(Sha256, KnownVectors) {
  EXPECT_EQ(sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(sha256Hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  // The 56-byte FIPS vector (padding edge: length lands in the last block).
  EXPECT_EQ(sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(Sha256, StreamingMatchesOneShot) {
  const std::string data = pattern(200000, 3);
  Sha256 streamed;
  for (size_t i = 0; i < data.size(); i += 999) {
    const size_t n = std::min<size_t>(999, data.size() - i);
    streamed.update(reinterpret_cast<const uint8_t*>(data.data()) + i, n);
  }
  EXPECT_EQ(streamed.finishHex(), sha256Hex(data));
}

TEST(ChunkCopy, SizesAroundChunkBoundary) {
  for (size_t n : {0u, 1u, 8191u, 8192u, 8193u, 200000u}) {
    const std::string data = pattern(n, 5);
    MemSource src(data);
    MemSink sink;
    std::string hash;
    ASSERT_TRUE(copyAndHash(src, 0, n, sink, kChunkSize, &hash, nullptr, nullptr)) << "size " << n;
    EXPECT_EQ(sink.bytes, data) << "size " << n;
    EXPECT_EQ(hash, sha256Hex(data)) << "size " << n;
  }
}

TEST(ChunkCopy, SmallChunkSizeAndOffset) {
  const std::string data = pattern(10000, 9);
  MemSource src(data);
  MemSink sink;
  std::string hash;
  ASSERT_TRUE(copyAndHash(src, 2500, 5000, sink, 512, &hash, nullptr, nullptr));
  EXPECT_EQ(sink.bytes, data.substr(2500, 5000));
  EXPECT_EQ(hash, sha256Hex(data.substr(2500, 5000)));
}

TEST(ChunkCopy, SinkFailureAborts) {
  const std::string data = pattern(50000, 13);
  MemSource src(data);
  MemSink sink;
  sink.failAfter = 10000;
  std::string hash;
  EXPECT_FALSE(copyAndHash(src, 0, data.size(), sink, kChunkSize, &hash, nullptr, nullptr));
}

TEST(ChunkCopy, ProgressReportsBytesDone) {
  const std::string data = pattern(20000, 17);
  MemSource src(data);
  MemSink sink;
  std::vector<size_t> seen;
  ASSERT_TRUE(copyAndHash(src, 0, data.size(), sink, 8192, nullptr,
                          [](size_t done, size_t total, void* ctx) {
                            static_cast<std::vector<size_t>*>(ctx)->push_back(done);
                            EXPECT_EQ(total, 20000u);
                          },
                          &seen));
  ASSERT_FALSE(seen.empty());
  EXPECT_EQ(seen.back(), 20000u);
  for (size_t i = 1; i < seen.size(); i++) EXPECT_GT(seen[i], seen[i - 1]);
}

TEST(HashSource, TamperChangesHash) {
  std::string data = pattern(5000, 19);
  MemSource src(data);
  std::string h1, h2;
  ASSERT_TRUE(hashSource(src, 0, data.size(), kChunkSize, h1, nullptr, nullptr));
  data[123] = static_cast<char>(data[123] ^ 0xFF);
  MemSource src2(data);
  ASSERT_TRUE(hashSource(src2, 0, data.size(), kChunkSize, h2, nullptr, nullptr));
  EXPECT_NE(h1, h2);
  EXPECT_EQ(h1, sha256Hex(pattern(5000, 19)));
}

TEST(EspImage, LengthWalkWithoutHash) {
  const std::string img = makeEspImage(false);
  MemSource src(img);
  uint64_t len = 0;
  ASSERT_TRUE(computeEspImageLength(src, &len));
  EXPECT_EQ(len, img.size());
  EXPECT_EQ(len, 192u);  // 24 + (8+100) + (8+37) = 177 -> padEnd 192
}

TEST(EspImage, LengthWalkWithHash) {
  const std::string img = makeEspImage(true);
  MemSource src(img);
  uint64_t len = 0;
  ASSERT_TRUE(computeEspImageLength(src, &len));
  EXPECT_EQ(len, img.size());
  EXPECT_EQ(len, 224u);  // 192 + 32-byte SHA trailer
}

TEST(EspImage, LengthWalkRejectsTruncated) {
  std::string img = makeEspImage(true);
  img.resize(img.size() - 10);  // trailer cut short
  MemSource src(img);
  uint64_t len = 0;
  EXPECT_FALSE(computeEspImageLength(src, &len));
}

TEST(EspImage, LengthWalkRejectsBadMagicAndZeroSegments) {
  std::string img = makeEspImage(false);
  img[0] = 0x00;
  MemSource src(img);
  uint64_t len = 0;
  EXPECT_FALSE(computeEspImageLength(src, &len));

  std::string img2 = makeEspImage(false);
  img2[1] = 0;  // segment count 0
  MemSource src2(img2);
  EXPECT_FALSE(computeEspImageLength(src2, &len));
}

TEST(EspImage, LengthWalkRejectsSegmentOverrun) {
  std::string img = makeEspImage(false);
  img.resize(60);  // inside segment 1's payload
  MemSource src(img);
  uint64_t len = 0;
  EXPECT_FALSE(computeEspImageLength(src, &len));
}

TEST(EspImage, HeaderParse) {
  const std::string img = makeEspImage(true, 0x000D);
  EspImageHeader hdr;
  ASSERT_TRUE(parseEspImageHeader(reinterpret_cast<const uint8_t*>(img.data()), img.size(), &hdr));
  EXPECT_EQ(hdr.segmentCount, 2);
  EXPECT_EQ(hdr.chipId, 0x000D);
  EXPECT_TRUE(hdr.hashAppended);
  EXPECT_FALSE(parseEspImageHeader(reinterpret_cast<const uint8_t*>(img.data()), 10, &hdr));
  EXPECT_FALSE(parseEspImageHeader(nullptr, 24, &hdr));
}

TEST(PartitionConfigId, StableAndOrderInsensitive) {
  const std::vector<PartitionInfo> a = {
      {"nvs", "data", "nvs", 0x9000, 0x5000},
      {"app0", "app", "ota_0", 0x10000, 0x640000},
      {"app1", "app", "ota_1", 0x650000, 0x640000},
  };
  const std::vector<PartitionInfo> shuffled = {a[2], a[0], a[1]};
  EXPECT_EQ(partitionConfigId(a), partitionConfigId(shuffled));
  EXPECT_EQ(partitionConfigId(a).size(), 64u);
}

TEST(PartitionConfigId, ChangesWithLayout) {
  const std::vector<PartitionInfo> a = {{"app0", "app", "ota_0", 0x10000, 0x640000}};
  const std::vector<PartitionInfo> b = {{"app0", "app", "ota_0", 0x10000, 0x500000}};
  const std::vector<PartitionInfo> c = {{"app", "app", "ota_0", 0xE0000, 0xE70000}};  // Picco layout
  EXPECT_NE(partitionConfigId(a), partitionConfigId(b));
  EXPECT_NE(partitionConfigId(a), partitionConfigId(c));
}

TEST(Manifest, RoundTrip) {
  const Manifest m = sampleManifest();
  const std::string json = manifestToJson(m);
  Manifest back;
  std::string err;
  ASSERT_TRUE(manifestFromJson(json, &back, &err)) << err;
  EXPECT_EQ(back.formatVersion, m.formatVersion);
  EXPECT_EQ(back.createdUtc, m.createdUtc);
  EXPECT_EQ(back.appVersion, m.appVersion);
  EXPECT_EQ(back.board, m.board);
  EXPECT_EQ(back.runningSlot, m.runningSlot);
  EXPECT_EQ(back.partitionConfigId, m.partitionConfigId);
  EXPECT_EQ(back.firmwareSize, m.firmwareSize);
  EXPECT_EQ(back.firmwareSha256, m.firmwareSha256);
  EXPECT_EQ(back.hasNvs, m.hasNvs);
  EXPECT_EQ(back.nvsSize, m.nvsSize);
  EXPECT_EQ(back.nvsSha256, m.nvsSha256);
  EXPECT_EQ(back.configFiles, m.configFiles);
}

TEST(Manifest, RejectsGarbageAndMissingFields) {
  Manifest m;
  std::string err;
  EXPECT_FALSE(manifestFromJson("not json at all", &m, &err));
  EXPECT_FALSE(manifestFromJson("{}", &m, &err));
  // Right shape, wrong backup format tag.
  std::string json = manifestToJson(sampleManifest());
  const size_t pos = json.find("crosspoint-os-backup");
  ASSERT_NE(pos, std::string::npos);
  json.replace(pos, 20, "something-else-0000");
  EXPECT_FALSE(manifestFromJson(json, &m, &err));
}

TEST(Manifest, RejectsBadHashShape) {
  std::string json = manifestToJson(sampleManifest());
  const size_t pos = json.find(std::string(64, 'b'));
  ASSERT_NE(pos, std::string::npos);
  json.replace(pos, 64, "xyz");
  Manifest m;
  std::string err;
  EXPECT_FALSE(manifestFromJson(json, &m, &err));
}

TEST(CheckRestore, AllGoodPasses) {
  const Manifest m = sampleManifest();
  const std::string img = makeEspImage(false);
  EXPECT_EQ(checkRestore(m, m.partitionConfigId, 0x640000, m.firmwareSize, m.firmwareSha256,
                         reinterpret_cast<const uint8_t*>(img.data())),
            RestoreCheck::Ok);
}

TEST(CheckRestore, EachGuardFailsClosed) {
  const Manifest m = sampleManifest();
  const std::string img = makeEspImage(false);
  const auto* hdr = reinterpret_cast<const uint8_t*>(img.data());

  EXPECT_EQ(checkRestore(m, std::string(64, 'z'), 0x640000, m.firmwareSize, m.firmwareSha256, hdr),
            RestoreCheck::PartitionConfigMismatch);
  EXPECT_EQ(checkRestore(m, m.partitionConfigId, 0x640000, m.firmwareSize + 1, m.firmwareSha256, hdr),
            RestoreCheck::FirmwareSizeMismatch);
  EXPECT_EQ(checkRestore(m, m.partitionConfigId, 0x640000, m.firmwareSize, std::string(64, 'e'), hdr),
            RestoreCheck::FirmwareHashMismatch);
  EXPECT_EQ(checkRestore(m, m.partitionConfigId, /*destCapacity=*/1024, m.firmwareSize, m.firmwareSha256, hdr),
            RestoreCheck::ImageTooLarge);

  std::string badMagic = img;
  badMagic[0] = 0x42;
  EXPECT_EQ(checkRestore(m, m.partitionConfigId, 0x640000, m.firmwareSize, m.firmwareSha256,
                         reinterpret_cast<const uint8_t*>(badMagic.data())),
            RestoreCheck::ImageHeaderInvalid);
  EXPECT_EQ(checkRestore(m, m.partitionConfigId, 0x640000, m.firmwareSize, m.firmwareSha256, nullptr),
            RestoreCheck::ImageHeaderInvalid);
}

TEST(BackupNames, Validation) {
  EXPECT_TRUE(isValidBackupName("20261105-131500"));
  EXPECT_TRUE(isValidBackupName("20261105-131500-2"));
  EXPECT_TRUE(isValidBackupName("backup-003"));
  EXPECT_FALSE(isValidBackupName(""));
  EXPECT_FALSE(isValidBackupName(".."));
  EXPECT_FALSE(isValidBackupName("../escape"));
  EXPECT_FALSE(isValidBackupName("a/b"));
  EXPECT_FALSE(isValidBackupName("white space"));
  EXPECT_FALSE(isValidBackupName(".hidden"));
}

TEST(BackupNames, SortAndPrune) {
  std::vector<std::string> names = {"20261105-090000", "backup-002", "20261104-210000", "backup-001",
                                    "20261105-131500"};
  const auto sorted = sortBackupNames(names);
  EXPECT_EQ(sorted.front(), "20261104-210000");
  EXPECT_EQ(sorted.back(), "backup-002");  // plain lexicographic order overall

  // Prune returns the oldest entries: keeping the newest 2 of 5 drops the
  // first 3 in sorted order.
  const auto prune = selectPrune(sorted, 2);
  EXPECT_EQ(prune.size(), 3u);
  EXPECT_EQ(prune.front(), "20261104-210000");
  EXPECT_EQ(prune.back(), "20261105-131500");
}

TEST(BackupNames, PruneKeepsNewestAndHonoursZero) {
  const std::vector<std::string> sorted = {"a-001", "a-002", "a-003"};
  EXPECT_TRUE(selectPrune(sorted, 3).empty());
  EXPECT_TRUE(selectPrune(sorted, 5).empty());
  EXPECT_TRUE(selectPrune(sorted, 0).empty());  // 0 = pruning disabled
  const auto prune = selectPrune(sorted, 1);
  ASSERT_EQ(prune.size(), 2u);
  EXPECT_EQ(prune[0], "a-001");
  EXPECT_EQ(prune[1], "a-002");
}

}  // namespace
