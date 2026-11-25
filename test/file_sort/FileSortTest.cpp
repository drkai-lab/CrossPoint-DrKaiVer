#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "FileSort.h"
#include "FsHelpers.h"

namespace {

using FileSort::Direction;
using FileSort::Entry;
using FileSort::Key;

Entry file(const std::string& name, const uint32_t time = 0) { return Entry{name, time, false}; }
Entry dir(const std::string& name, const uint32_t time = 0) { return Entry{name, time, true}; }

std::vector<std::string> namesOf(const std::vector<Entry>& entries) {
  std::vector<std::string> out;
  out.reserve(entries.size());
  for (const auto& e : entries) out.push_back(e.name);
  return out;
}

std::vector<std::string> sortedNames(std::vector<Entry> entries, const Key key, const Direction direction) {
  FileSort::sortEntries(entries, key, direction);
  return namesOf(entries);
}

// FAT packed timestamps, (date << 16) | time, as HalFile::modificationTime
// returns them: 2024-01-15 10:30:00 and 2025-06-02 08:00:00.
constexpr uint32_t kOlder = (45u << 25) | (1u << 21) | (15u << 16) | (10u << 11) | (30u << 5);
constexpr uint32_t kNewer = (46u << 25) | (6u << 21) | (2u << 16) | (8u << 11);

TEST(FileSortName, NaturalAscendingCaseInsensitive) {
  // file2 < file10 (numeric runs compare by value), case does not matter.
  EXPECT_EQ(sortedNames({file("file10.txt"), file("File2.txt"), file("file1.txt")}, Key::Name, Direction::Ascending),
            (std::vector<std::string>{"file1.txt", "File2.txt", "file10.txt"}));
}

TEST(FileSortName, DescendingReversesNameOrder) {
  EXPECT_EQ(sortedNames({file("b.txt"), file("a.txt"), file("c10.txt"), file("c2.txt")}, Key::Name,
                        Direction::Descending),
            (std::vector<std::string>{"c10.txt", "c2.txt", "b.txt", "a.txt"}));
}

TEST(FileSortName, DefaultMatchesLegacySortFileList) {
  // The pre-existing browser order (FsHelpers::sortFileList) is the Name /
  // Ascending order; the new default must not reshuffle existing libraries.
  std::vector<std::string> legacy = {"zeta/", "book10.epub", "Book2.epub", "alpha/", "a.txt"};
  FsHelpers::sortFileList(legacy);
  std::vector<Entry> entries;
  for (const auto& n : legacy) entries.push_back(Entry{n, 0, !n.empty() && n.back() == '/'});
  // Rebuild from the unsorted input for a fair comparison.
  entries = {dir("zeta/"), file("book10.epub"), file("Book2.epub"), dir("alpha/"), file("a.txt")};
  EXPECT_EQ(sortedNames(entries, Key::Name, Direction::Ascending), legacy);
}

TEST(FileSortFolders, DirectoriesFirstForEveryKeyAndDirection) {
  const std::vector<Entry> input = {file("a.txt", kNewer), dir("zdir/"), file("b.txt", kOlder), dir("adir/")};
  for (const auto key : {Key::Name, Key::Date, Key::Type}) {
    for (const auto direction : {Direction::Ascending, Direction::Descending}) {
      const auto out = sortedNames(input, key, direction);
      ASSERT_EQ(out.size(), 4u);
      EXPECT_EQ(out[0].back(), '/') << "key=" << static_cast<int>(key) << " dir=" << static_cast<int>(direction);
      EXPECT_EQ(out[1].back(), '/');
    }
  }
  // Folders among themselves still follow the key (name here).
  EXPECT_EQ(sortedNames(input, Key::Name, Direction::Ascending),
            (std::vector<std::string>{"adir/", "zdir/", "a.txt", "b.txt"}));
  EXPECT_EQ(sortedNames(input, Key::Name, Direction::Descending),
            (std::vector<std::string>{"zdir/", "adir/", "b.txt", "a.txt"}));
}

TEST(FileSortDate, AscendingOldestFirstUnknownTimeIsOldest) {
  EXPECT_EQ(sortedNames({file("new.txt", kNewer), file("unknown.txt", 0), file("old.txt", kOlder)}, Key::Date,
                        Direction::Ascending),
            (std::vector<std::string>{"unknown.txt", "old.txt", "new.txt"}));
}

TEST(FileSortDate, DescendingNewestFirst) {
  EXPECT_EQ(sortedNames({file("old.txt", kOlder), file("new.txt", kNewer), file("unknown.txt", 0)}, Key::Date,
                        Direction::Descending),
            (std::vector<std::string>{"new.txt", "old.txt", "unknown.txt"}));
}

TEST(FileSortDate, TiesFallBackToNameAscendingInBothDirections) {
  const std::vector<Entry> input = {file("b.txt", kOlder), file("a.txt", kOlder), file("c.txt", kNewer)};
  EXPECT_EQ(sortedNames(input, Key::Date, Direction::Ascending),
            (std::vector<std::string>{"a.txt", "b.txt", "c.txt"}));
  // Primary key flips, the name tie-break does not.
  EXPECT_EQ(sortedNames(input, Key::Date, Direction::Descending),
            (std::vector<std::string>{"c.txt", "a.txt", "b.txt"}));
}

TEST(FileSortType, AscendingByKindThenName) {
  // Defined kind order: txt < md < epub < pdf < xtc < images < other.
  EXPECT_EQ(sortedNames({file("z.pdf"), file("a.epub"), file("m.md"), file("b.txt"), file("c.png"), file("d.xtc"),
                         file("e.bin"), file("f.txt")},
                        Key::Type, Direction::Ascending),
            (std::vector<std::string>{"b.txt", "f.txt", "m.md", "a.epub", "z.pdf", "d.xtc", "c.png", "e.bin"}));
}

TEST(FileSortType, DescendingByKindThenNameAscendingWithinKind) {
  EXPECT_EQ(sortedNames({file("b.txt"), file("a.epub"), file("f.txt"), file("z.pdf")}, Key::Type,
                        Direction::Descending),
            (std::vector<std::string>{"z.pdf", "a.epub", "b.txt", "f.txt"}));
}

TEST(FileSortType, RankTableAndExtensionParsing) {
  EXPECT_EQ(FileSort::typeRank("a.txt"), 0);
  EXPECT_EQ(FileSort::typeRank("a.MD"), 1);
  EXPECT_EQ(FileSort::typeRank("a.epub"), 2);
  EXPECT_EQ(FileSort::typeRank("a.PDF"), 3);
  EXPECT_EQ(FileSort::typeRank("a.xtch"), 4);
  EXPECT_EQ(FileSort::typeRank("a.jpg"), 5);
  EXPECT_EQ(FileSort::typeRank("a.gif"), 5);
  EXPECT_EQ(FileSort::typeRank("a.bin"), 6);
  EXPECT_EQ(FileSort::typeRank("noext"), 6);
  EXPECT_EQ(FileSort::extensionLower("Book.EPUB"), "epub");
  EXPECT_EQ(FileSort::extensionLower("folder/"), "");
  EXPECT_EQ(FileSort::extensionLower("noext"), "");
}

TEST(FileSortStability, EqualEntriesKeepInputOrder) {
  // Identical name+time+kind entries must not be reordered (stable sort),
  // distinguishable only by their position payload in modificationTime's
  // place: use identical entries and check the comparator never orders them.
  const Entry a = file("same.txt", kOlder);
  const Entry b = file("same.txt", kOlder);
  for (const auto key : {Key::Name, Key::Date, Key::Type}) {
    EXPECT_EQ(FileSort::compareEntries(a, b, key, Direction::Ascending), 0);
    EXPECT_EQ(FileSort::compareEntries(a, b, key, Direction::Descending), 0);
    EXPECT_FALSE(FileSort::entryLess(a, b, key, Direction::Ascending));
    EXPECT_FALSE(FileSort::entryLess(b, a, key, Direction::Ascending));
  }
  std::vector<Entry> entries = {file("x.txt", kOlder), file("same.txt", kOlder), file("same.txt", kOlder)};
  FileSort::sortEntries(entries, Key::Date, Direction::Ascending);
  EXPECT_EQ(namesOf(entries), (std::vector<std::string>{"same.txt", "same.txt", "x.txt"}));
}

TEST(FileSortStored, StoredValuesClampToValidOrders) {
  EXPECT_EQ(FileSort::keyFromStored(0), Key::Name);
  EXPECT_EQ(FileSort::keyFromStored(1), Key::Date);
  EXPECT_EQ(FileSort::keyFromStored(2), Key::Type);
  EXPECT_EQ(FileSort::keyFromStored(99), Key::Name);
  EXPECT_EQ(FileSort::directionFromStored(0), Direction::Ascending);
  EXPECT_EQ(FileSort::directionFromStored(1), Direction::Descending);
  EXPECT_EQ(FileSort::directionFromStored(42), Direction::Ascending);
}

TEST(FileSortSampleBooks, EmulatorSampleSetExpectedOrders) {
  // The emulator's sample books (~/workspace/.../emulator/sd/books) with
  // illustrative timestamps; integration re-checks these orders on screen.
  const std::vector<Entry> books = {file("sample.txt", kOlder), file("sample.md", kNewer), file("sample.epub", kOlder),
                                    file("sample.pdf", kNewer)};
  EXPECT_EQ(sortedNames(books, Key::Name, Direction::Ascending),
            (std::vector<std::string>{"sample.epub", "sample.md", "sample.pdf", "sample.txt"}));
  EXPECT_EQ(sortedNames(books, Key::Type, Direction::Ascending),
            (std::vector<std::string>{"sample.txt", "sample.md", "sample.epub", "sample.pdf"}));
  EXPECT_EQ(sortedNames(books, Key::Date, Direction::Descending),
            (std::vector<std::string>{"sample.md", "sample.pdf", "sample.epub", "sample.txt"}));
}

}  // namespace
