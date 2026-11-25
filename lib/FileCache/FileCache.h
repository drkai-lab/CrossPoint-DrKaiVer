#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

// File-backed LRU cache rooted on the SD card (docs/SHIFT_LEFT.md R4).
//
// The user's "cache to SSD" requirement maps to this device's SD card: all
// payloads (EPUB section caches, extracted PDF text, glyph/layout caches)
// live as files under the cache root; RAM holds only the small LRU index
// (key -> size + last-access counter, capped at kMaxEntries) and the page
// currently being rendered by the caller.
//
// Storage is injected through FileOps: on device the adapter in
// FileCacheHal.cpp backs it with HalStorage; in host tests an in-memory
// fake verifies the eviction policy without an SD card.
class FileCache {
 public:
  static constexpr uint64_t kDefaultMaxBytes = 64ull * 1024 * 1024;
  static constexpr const char* kDefaultRoot = "/.crosspoint/cache";
  static constexpr size_t kMaxEntries = 512;
  static constexpr const char* kIndexFileName = "lru_index.txt";

  struct FileOps {
    void* ctx = nullptr;
    bool (*exists)(void* ctx, const std::string& path) = nullptr;
    size_t (*fileSize)(void* ctx, const std::string& path) = nullptr;
    bool (*remove)(void* ctx, const std::string& path) = nullptr;
    bool (*readFile)(void* ctx, const std::string& path, std::string& out) = nullptr;
    bool (*writeFile)(void* ctx, const std::string& path, const std::string& data) = nullptr;
  };

  FileCache(FileOps ops, std::string root, uint64_t maxBytes);

  // On-disk path for a cache key. Keys are sanitised: any '/' is kept as a
  // directory separator inside the root, but ".." components are flattened
  // so a key can never escape the cache root.
  std::string entryPath(const std::string& key) const;
  std::string indexPath() const { return root_ + "/" + kIndexFileName; }

  // Records that `key`'s file was written/read now (access counter is a
  // monotonic sequence supplied by the caller, e.g. seconds or a counter,
  // so tests are deterministic). Size is re-read from storage.
  void recordAccess(const std::string& key, uint64_t accessSeq);
  uint64_t lastAccess(const std::string& key) const;
  size_t totalBytes() const;

  // Evicts least-recently-used entries until total <= maxBytes.
  // `protectKey` (the entry just written / currently displayed) is never
  // evicted, even if it alone exceeds the cap.
  bool enforceLimit(const std::string& protectKey);

  bool saveIndex() const;
  bool loadIndex();

  uint64_t maxBytes() const { return maxBytes_; }
  const std::string& root() const { return root_; }

 private:
  struct Entry {
    size_t size = 0;
    uint64_t lastAccess = 0;
  };
  FileOps ops_;
  std::string root_;
  uint64_t maxBytes_;
  std::map<std::string, Entry> entries_;
};
