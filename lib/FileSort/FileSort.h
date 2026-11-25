#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Pure ordering logic for directory listings (file browser / library file
// lists). Kept free of HAL and UI types so host tests can pin the ordering
// contract without a device: see test/file_sort.
//
// Ordering contract:
// - Directories always come before files, for every key and direction.
//   This preserves the long-standing FsHelpers::sortFileList behaviour the
//   file browser shipped with; a direction change never buries folders
//   under files.
// - The direction applies to the primary key only. Ties on the primary
//   key always fall back to the name, natural ascending, so a descending
//   date list still reads A..Z inside one day instead of flipping
//   unpredictably, and equal inputs keep a deterministic order
//   (sortEntries is stable on top of that).
namespace FileSort {

enum class Key : uint8_t { Name = 0, Date = 1, Type = 2, Count };
enum class Direction : uint8_t { Ascending = 0, Descending = 1, Count };

// One row of a directory listing. `name` is the raw entry name exactly as
// the file browser stores it (directories carry a trailing '/').
// `modificationTime` is HalFile::modificationTime(): the FAT modification
// date/time packed as (date << 16) | time by the storage layer. The FAT
// fields are stored most-significant-first (year, month, day, then hour,
// minute, 2-second ticks), so the packed integer compares chronologically
// with a plain integer compare; the library index already relies on the
// same value. 0 means "no timestamp" (or a filesystem that did not report
// one) and sorts as the oldest possible time.
struct Entry {
  std::string name;
  uint32_t modificationTime = 0;
  bool isDirectory = false;
};

// Lowercase extension without the dot ("epub" for "Book.EPUB"); "" when the
// name has no extension or is a directory entry.
std::string extensionLower(const std::string& name);

// Rank of a file's kind for Key::Type, ascending. The fork's book formats
// come first in reading order (txt, md, epub, pdf), then the remaining
// formats the browser lists (xtc, images), then everything else:
//   txt 0, md 1, epub 2, pdf 3, xtc/xtch 4, jpg/jpeg/png/bmp/gif 5, other 6
// Directories have no kind; they are grouped first before ranks apply.
int typeRank(const std::string& name);

// Three-way compare under the contract above: <0 when `a` lists before
// `b`, >0 when after, 0 when the two entries are indistinguishable.
int compareEntries(const Entry& a, const Entry& b, Key key, Direction direction);

bool entryLess(const Entry& a, const Entry& b, Key key, Direction direction);

// Stable in-place sort of a whole listing.
void sortEntries(std::vector<Entry>& entries, Key key, Direction direction);

// Stored-setting conversion (CrossPointSettings keeps plain uint8_t
// fields). Out-of-range stored values fall back to Name / Ascending so a
// corrupt or newer settings file can never select a phantom order.
Key keyFromStored(uint8_t value);
Direction directionFromStored(uint8_t value);

}  // namespace FileSort
