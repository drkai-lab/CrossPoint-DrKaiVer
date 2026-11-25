#include "FileCacheHal.h"

#include <HalStorage.h>

FileCache::FileOps makeHalFileCacheOps() {
  FileCache::FileOps ops;
  ops.ctx = nullptr;
  ops.exists = [](void*, const std::string& path) { return Storage.exists(path.c_str()); };
  ops.fileSize = [](void*, const std::string& path) -> size_t {
    HalFile f;
    if (!Storage.openFileForRead("FCACHE", path, f)) return 0;
    return f.size();
  };
  ops.remove = [](void*, const std::string& path) { return Storage.remove(path.c_str()); };
  ops.readFile = [](void*, const std::string& path, std::string& out) {
    return Storage.readFileToString("FCACHE", path, 64 * 1024, out);
  };
  ops.writeFile = [](void*, const std::string& path, const std::string& data) {
    return Storage.writeFile(path.c_str(), String(data.c_str()));
  };
  return ops;
}
