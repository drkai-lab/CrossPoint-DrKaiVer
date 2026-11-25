#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "Print.h"
#include "TxtToHtml.h"

namespace {

class StringPrint : public Print {
 public:
  std::string str;
  size_t write(uint8_t b) override {
    str.push_back(static_cast<char>(b));
    return 1;
  }
  size_t write(const uint8_t* buffer, size_t size) override {
    str.append(reinterpret_cast<const char*>(buffer), size);
    return size;
  }
};

std::string md(std::string_view content) {
  StringPrint out;
  EXPECT_TRUE(TxtToHtml::stream("test.md", content, out));
  return out.str;
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

TEST(MarkdownToHtmlTest, Headings) {
  EXPECT_TRUE(contains(md("# Title\n"), "<h1>Title</h1>"));
  EXPECT_TRUE(contains(md("### Sub\n"), "<h3>Sub</h3>"));
  EXPECT_TRUE(contains(md("###### Deep\n"), "<h6>Deep</h6>"));
}

TEST(MarkdownToHtmlTest, InlineEmphasisAndCode) {
  const std::string out = md("A **bold** and *italic* and `x<y` end\n");
  EXPECT_TRUE(contains(out, "<b>bold</b>"));
  EXPECT_TRUE(contains(out, "<i>italic</i>"));
  EXPECT_TRUE(contains(out, "<code>x&lt;y</code>"));
}

TEST(MarkdownToHtmlTest, FencedCodeBlockIsEscapedAndLiteral) {
  const std::string out = md("```\n**not bold** <tag>\n```\n");
  EXPECT_TRUE(contains(out, "<pre>"));
  EXPECT_TRUE(contains(out, "**not bold** &lt;tag&gt;"));
  EXPECT_FALSE(contains(out, "<b>not bold</b>"));
}

TEST(MarkdownToHtmlTest, Lists) {
  const std::string out = md("- one\n- two\n\n1. first\n2. second\n");
  EXPECT_TRUE(contains(out, "<ul>"));
  EXPECT_TRUE(contains(out, "<li>one</li>"));
  EXPECT_TRUE(contains(out, "<ol>"));
  EXPECT_TRUE(contains(out, "<li>first</li>"));
}

TEST(MarkdownToHtmlTest, LinkQuoteAndRule) {
  const std::string out = md("[site](https://example.com)\n\n> quoted\n\n---\n");
  EXPECT_TRUE(contains(out, "<a href=\"https://example.com\">site</a>"));
  EXPECT_TRUE(contains(out, "<blockquote>quoted</blockquote>"));
  EXPECT_TRUE(contains(out, "<hr />"));
}

TEST(MarkdownToHtmlTest, RawHtmlIsEscapedNotPassedThrough) {
  const std::string out = md("<script>alert(1)</script>\n");
  EXPECT_FALSE(contains(out, "<script>"));
  EXPECT_TRUE(contains(out, "&lt;script&gt;"));
}

TEST(MarkdownToHtmlTest, JapaneseTextPassesThrough) {
  const std::string out = md("# 見出し\n\n本文です。**強調**\n");
  EXPECT_TRUE(contains(out, "<h1>見出し</h1>"));
  EXPECT_TRUE(contains(out, "本文です。<b>強調</b>"));
}

TEST(MarkdownToHtmlTest, TxtFilesAreNotMarkdownProcessed) {
  StringPrint out;
  ASSERT_TRUE(TxtToHtml::stream("test.txt", "# not a heading\n", out));
  EXPECT_FALSE(contains(out.str, "<h1>"));
}

TEST(MarkdownToHtmlTest, MarkdownCacheVersionDiffersFromTxt) {
  EXPECT_NE(std::string(TxtToHtml::cacheVersionTag("a.md")), std::string(TxtToHtml::cacheVersionTag("a.txt")));
}

}  // namespace
