#include "FileCache.h"

#include <algorithm>
#include <vector>

FileCache::FileCache(FileOps ops, std::string root, uint64_t maxBytes)
    : ops_(ops), root_(std::move(root)), maxBytes_(maxBytes == 0 ? kDefaultMaxBytes : maxBytes) {
  while (root_.size() > 1 && root_.back() == '/') root_.pop_back();
}

std::string FileCache::entryPath(const std::string& key) const {
  std::string clean;
  // Flatten ".." components so keys cannot escape the cache root.
  size_t start = 0;
  while (start <= key.size()) {
    const size_t slash = key.find('/', start);
    const std::string part = key.substr(start, slash == std::string::npos ? slash : slash - start);
    if (part == "..") {
      clean += "__";
    } else if (!part.empty() && part != ".") {
      if (!clean.empty()) clean += "/";
      clean += part;
    }
    if (slash == std::string::npos) break;
    start = slash + 1;
  }
  if (clean.empty()) clean = "_";
  return root_ + "/" + clean;
}

void FileCache::recordAccess(const std::string& key, const uint64_t accessSeq) {
  if (entries_.size() >= kMaxEntries && entries_.count(key) == 0) {
    // Index itself must stay bounded in RAM: drop the coldest entry.
    enforceLimit(key);
    if (entries_.size() >= kMaxEntries) return;
  }
  Entry entry;
  entry.size = ops_.fileSize ? ops_.fileSize(ops_.ctx, entryPath(key)) : 0;
  entry.lastAccess = accessSeq;
  entries_[key] = entry;
}

uint64_t FileCache::lastAccess(const std::string& key) const {
  const auto it = entries_.find(key);
  return it == entries_.end() ? 0 : it->second.lastAccess;
}

size_t FileCache::totalBytes() const {
  size_t total = 0;
  for (const auto& [key, entry] : entries_) total += entry.size;
  return total;
}

bool FileCache::enforceLimit(const std::string& protectKey) {
  while (totalBytes() > maxBytes_) {
    // Find the least-recently-used entry that is not protected.
    const std::string* victim = nullptr;
    uint64_t oldest = UINT64_MAX;
    for (const auto& [key, entry] : entries_) {
      if (key == protectKey) continue;
      if (entry.lastAccess < oldest) {
        oldest = entry.lastAccess;
        victim = &key;
      }
    }
    if (victim == nullptr) return true;  // only the protected entry remains
    const std::string victimKey = *victim;
    if (ops_.remove) ops_.remove(ops_.ctx, entryPath(victimKey));
    entries_.erase(victimKey);
  }
  return true;
}

bool FileCache::saveIndex() const {
  if (!ops_.writeFile) return false;
  std::string out = "FILECACHE_INDEX_V1\n";
  for (const auto& [key, entry] : entries_) {
    out += key + "\t" + std::to_string(entry.size) + "\t" + std::to_string(entry.lastAccess) + "\n";
  }
  return ops_.writeFile(ops_.ctx, indexPath(), out);
}

bool FileCache::loadIndex() {
  if (!ops_.readFile) return false;
  std::string data;
  if (!ops_.readFile(ops_.ctx, indexPath(), data)) return false;
  entries_.clear();
  size_t pos = data.find('\n');
  if (pos == std::string::npos || data.compare(0, pos, "FILECACHE_INDEX_V1") != 0) return false;
  pos++;
  while (pos < data.size()) {
    const size_t eol = data.find('\n', pos);
    const std::string line = data.substr(pos, eol == std::string::npos ? eol : eol - pos);
    pos = (eol == std::string::npos) ? data.size() : eol + 1;
    const size_t t1 = line.find('\t');
    const size_t t2 = t1 == std::string::npos ? t1 : line.find('\t', t1 + 1);
    if (t1 == std::string::npos || t2 == std::string::npos) continue;
    Entry entry;
    entry.size = static_cast<size_t>(std::stoull(line.substr(t1 + 1, t2 - t1 - 1)));
    entry.lastAccess = std::stoull(line.substr(t2 + 1));
    entries_[line.substr(0, t1)] = entry;
    if (entries_.size() >= kMaxEntries) break;
  }
  return true;
}
