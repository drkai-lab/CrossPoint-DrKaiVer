#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <string>

#include "FileCache.h"

namespace {

// In-memory stand-in for the SD card: the same FileOps contract the device
// adapter implements on top of HalStorage.
struct FakeFs {
  std::map<std::string, std::string> files;  // path -> contents
};

FileCache::FileOps fakeOps(FakeFs& fs) {
  FileCache::FileOps ops;
  ops.ctx = &fs;
  ops.exists = [](void* c, const std::string& p) { return static_cast<FakeFs*>(c)->files.count(p) > 0; };
  ops.fileSize = [](void* c, const std::string& p) -> size_t {
    auto* f = static_cast<FakeFs*>(c);
    const auto it = f->files.find(p);
    return it == f->files.end() ? 0 : it->second.size();
  };
  ops.remove = [](void* c, const std::string& p) { return static_cast<FakeFs*>(c)->files.erase(p) > 0; };
  ops.readFile = [](void* c, const std::string& p, std::string& out) {
    auto* f = static_cast<FakeFs*>(c);
    const auto it = f->files.find(p);
    if (it == f->files.end()) return false;
    out = it->second;
    return true;
  };
  ops.writeFile = [](void* c, const std::string& p, const std::string& data) {
    static_cast<FakeFs*>(c)->files[p] = data;
    return true;
  };
  return ops;
}

TEST(FileCacheTest, DefaultsMatchSpec) {
  EXPECT_EQ(FileCache::kDefaultMaxBytes, 64ull * 1024 * 1024);
  EXPECT_EQ(std::string(FileCache::kDefaultRoot), "/.crosspoint/cache");
}

TEST(FileCacheTest, EntryPathIsUnderRootAndStable) {
  FakeFs fs;
  FileCache cache(fakeOps(fs), "/.crosspoint/cache", 1024);
  EXPECT_EQ(cache.entryPath("bookA/page1"), cache.entryPath("bookA/page1"));
  EXPECT_EQ(cache.entryPath("bookA/page1").rfind("/.crosspoint/cache/", 0), 0u);
  // Keys must not escape the cache root.
  EXPECT_EQ(cache.entryPath("../settings.json").find(".."), std::string::npos);
}

TEST(FileCacheTest, LruEvictsOldestFirstAndKeepsNewest) {
  FakeFs fs;
  FileCache cache(fakeOps(fs), "/cache", 100);
  fs.files[cache.entryPath("a")] = std::string(40, 'a');
  fs.files[cache.entryPath("b")] = std::string(40, 'b');
  fs.files[cache.entryPath("c")] = std::string(40, 'c');
  cache.recordAccess("a", 1);
  cache.recordAccess("b", 2);
  cache.recordAccess("c", 3);
  // Total 120 > cap 100: writing c must evict a (LRU), keep b and c.
  EXPECT_TRUE(cache.enforceLimit("c"));
  EXPECT_FALSE(fs.files.count(cache.entryPath("a")));
  EXPECT_TRUE(fs.files.count(cache.entryPath("b")));
  EXPECT_TRUE(fs.files.count(cache.entryPath("c")));
}

TEST(FileCacheTest, TouchUpdatesRecency) {
  FakeFs fs;
  FileCache cache(fakeOps(fs), "/cache", 100);
  fs.files[cache.entryPath("a")] = std::string(40, 'a');
  fs.files[cache.entryPath("b")] = std::string(40, 'b');
  fs.files[cache.entryPath("c")] = std::string(40, 'c');
  cache.recordAccess("a", 1);
  cache.recordAccess("b", 2);
  cache.recordAccess("a", 3);  // a is now most-recent
  cache.recordAccess("c", 4);
  EXPECT_TRUE(cache.enforceLimit("c"));
  EXPECT_TRUE(fs.files.count(cache.entryPath("a")));
  EXPECT_FALSE(fs.files.count(cache.entryPath("b")));
}

TEST(FileCacheTest, IndexPersistsAndReloads) {
  FakeFs fs;
  {
    FileCache cache(fakeOps(fs), "/cache", 100);
    fs.files[cache.entryPath("a")] = std::string(10, 'a');
    cache.recordAccess("a", 7);
    ASSERT_TRUE(cache.saveIndex());
  }
  FileCache reloaded(fakeOps(fs), "/cache", 100);
  ASSERT_TRUE(reloaded.loadIndex());
  EXPECT_EQ(reloaded.totalBytes(), 10u);
  EXPECT_EQ(reloaded.lastAccess("a"), 7u);
}

TEST(FileCacheTest, OversizeSingleEntryIsKeptButReported) {
  FakeFs fs;
  FileCache cache(fakeOps(fs), "/cache", 10);
  fs.files[cache.entryPath("big")] = std::string(50, 'x');
  cache.recordAccess("big", 1);
  // The just-written entry is never evicted by its own enforcement pass.
  EXPECT_TRUE(cache.enforceLimit("big"));
  EXPECT_TRUE(fs.files.count(cache.entryPath("big")));
}

}  // namespace
