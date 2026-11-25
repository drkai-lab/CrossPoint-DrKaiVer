#include <gtest/gtest.h>

#include <string_view>

#include "FsHelpers.h"

namespace {

TEST(SupportedFormatsTest, AllFourFormatsAreReflowableBooks) {
  EXPECT_TRUE(FsHelpers::hasReflowableBookExtension("book.txt"));
  EXPECT_TRUE(FsHelpers::hasReflowableBookExtension("book.md"));
  EXPECT_TRUE(FsHelpers::hasReflowableBookExtension("book.epub"));
  EXPECT_TRUE(FsHelpers::hasReflowableBookExtension("book.pdf"));
}

TEST(SupportedFormatsTest, PdfExtensionIsCaseInsensitive) {
  EXPECT_TRUE(FsHelpers::hasPdfExtension(std::string_view("BOOK.PDF")));
  EXPECT_TRUE(FsHelpers::hasPdfExtension(std::string_view("book.Pdf")));
  EXPECT_FALSE(FsHelpers::hasPdfExtension(std::string_view("book.pdfx")));
  EXPECT_FALSE(FsHelpers::hasPdfExtension(std::string_view("book.pdf.txt")));
}

TEST(SupportedFormatsTest, NonBooksStayRejected) {
  EXPECT_FALSE(FsHelpers::hasReflowableBookExtension("image.png"));
  EXPECT_FALSE(FsHelpers::hasReflowableBookExtension("notes.docx"));
}

}  // namespace
