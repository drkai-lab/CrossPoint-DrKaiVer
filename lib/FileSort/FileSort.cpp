#include "FileSort.h"

#include <FsHelpers.h>

#include <algorithm>
#include <cctype>

namespace FileSort {

namespace {

int sign(const int v) { return (v > 0) - (v < 0); }

// Three-way natural compare on the shared FsHelpers ordering, so the Name
// key matches FsHelpers::sortFileList exactly. naturalLess is a strict-weak
// "less" only, so ask it both ways; when it cannot separate the names
// (case variants of the same text) fall back to byte order to keep the
// comparator a strict weak ordering for std::stable_sort.
int compareNames(const std::string& a, const std::string& b) {
  if (a == b) return 0;
  if (FsHelpers::naturalLess(a, b)) return -1;
  if (FsHelpers::naturalLess(b, a)) return 1;
  return a < b ? -1 : 1;
}

}  // namespace

std::string extensionLower(const std::string& name) {
  if (!name.empty() && name.back() == '/') return "";  // directory entry
  const size_t dot = name.rfind('.');
  if (dot == std::string::npos || dot == 0) return "";
  std::string ext = name.substr(dot + 1);
  for (char& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return ext;
}

int typeRank(const std::string& name) {
  const std::string ext = extensionLower(name);
  if (ext == "txt") return 0;
  if (ext == "md") return 1;
  if (ext == "epub") return 2;
  if (ext == "pdf") return 3;
  if (ext == "xtc" || ext == "xtch") return 4;
  if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "bmp" || ext == "gif") return 5;
  return 6;
}

int compareEntries(const Entry& a, const Entry& b, const Key key, const Direction direction) {
  // Directories first, always (see the contract in FileSort.h).
  if (a.isDirectory != b.isDirectory) return a.isDirectory ? -1 : 1;

  int primary = 0;
  switch (key) {
    case Key::Date:
      // Packed FAT date/time compares chronologically as an integer
      // (see FileSort.h); 0 (no timestamp) is the oldest value.
      primary = a.modificationTime < b.modificationTime ? -1 : (a.modificationTime > b.modificationTime ? 1 : 0);
      break;
    case Key::Type:
      primary = sign(typeRank(a.name) - typeRank(b.name));
      break;
    case Key::Name:
    case Key::Count:
      primary = compareNames(a.name, b.name);
      break;
  }
  if (primary != 0) {
    return direction == Direction::Descending ? -primary : primary;
  }
  // Tie-break: name, natural ascending, regardless of direction.
  return compareNames(a.name, b.name);
}

bool entryLess(const Entry& a, const Entry& b, const Key key, const Direction direction) {
  return compareEntries(a, b, key, direction) < 0;
}

void sortEntries(std::vector<Entry>& entries, const Key key, const Direction direction) {
  std::stable_sort(entries.begin(), entries.end(),
                   [key, direction](const Entry& a, const Entry& b) { return entryLess(a, b, key, direction); });
}

Key keyFromStored(const uint8_t value) {
  return value < static_cast<uint8_t>(Key::Count) ? static_cast<Key>(value) : Key::Name;
}

Direction directionFromStored(const uint8_t value) {
  return value < static_cast<uint8_t>(Direction::Count) ? static_cast<Direction>(value) : Direction::Ascending;
}

}  // namespace FileSort
