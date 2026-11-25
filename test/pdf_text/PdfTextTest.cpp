#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "PdfText.h"

namespace {

// Minimal uncompressed (stored) zlib stream builder for FlateDecode fixtures:
// zlib header 0x78 0x01 + stored blocks + adler32. Good enough for the
// extractor's inflate callback contract in tests.
std::string zlibStored(const std::string& raw) {
  std::string out;
  out.push_back(static_cast<char>(0x78));
  out.push_back(static_cast<char>(0x01));
  size_t pos = 0;
  do {
    const size_t chunk = std::min<size_t>(raw.size() - pos, 65535);
    const bool last = (pos + chunk == raw.size());
    out.push_back(last ? 1 : 0);
    out.push_back(static_cast<char>(chunk & 0xFF));
    out.push_back(static_cast<char>((chunk >> 8) & 0xFF));
    const uint16_t nlen = static_cast<uint16_t>(~chunk);
    out.push_back(static_cast<char>(nlen & 0xFF));
    out.push_back(static_cast<char>((nlen >> 8) & 0xFF));
    out.append(raw.substr(pos, chunk));
    pos += chunk;
  } while (pos < raw.size());
  uint32_t a = 1, b = 0;
  for (unsigned char c : raw) {
    a = (a + c) % 65521;
    b = (b + a) % 65521;
  }
  const uint32_t adler = (b << 16) | a;
  out.push_back(static_cast<char>((adler >> 24) & 0xFF));
  out.push_back(static_cast<char>((adler >> 16) & 0xFF));
  out.push_back(static_cast<char>((adler >> 8) & 0xFF));
  out.push_back(static_cast<char>(adler & 0xFF));
  return out;
}

std::string makePdf(const std::string& streamDictExtra, const std::string& streamData) {
  std::string pdf = "%PDF-1.4\n";
  pdf += "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";
  pdf += "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n";
  pdf += "3 0 obj\n<< /Type /Page /Parent 2 0 R /Contents 4 0 R >>\nendobj\n";
  pdf += "4 0 obj\n<< /Length " + std::to_string(streamData.size()) + streamDictExtra + " >>\nstream\n";
  pdf += streamData;
  pdf += "\nendstream\nendobj\ntrailer\n<< /Root 1 0 R >>\n%%EOF\n";
  return pdf;
}

// Test inflate: uses zlib stored-block decoding only (fixtures use it too).
// On device this callback is backed by miniz; here a tiny stored inflater
// keeps the test free of external deps while exercising the same contract.
bool storedInflate(const uint8_t* in, size_t inLen, std::string& out) {
  if (inLen < 2) return false;
  size_t pos = 2;  // skip zlib header
  while (pos < inLen) {
    const uint8_t hdr = in[pos++];
    const bool last = hdr & 1;
    if (((hdr >> 1) & 3) != 0) return false;  // stored only
    if (pos + 4 > inLen) return false;
    const uint16_t len = in[pos] | (in[pos + 1] << 8);
    pos += 4;
    if (pos + len > inLen) return false;
    out.append(reinterpret_cast<const char*>(in + pos), len);
    pos += len;
    if (last) return true;
  }
  return false;
}

struct MemReader {
  std::string data;
  size_t pos = 0;
};

PdfText::Reader makeReader(MemReader& mem) {
  PdfText::Reader reader;
  reader.ctx = &mem;
  reader.read = [](void* ctx, uint8_t* buf, size_t size) -> int {
    auto* m = static_cast<MemReader*>(ctx);
    if (m->pos >= m->data.size()) return 0;
    const size_t n = std::min(size, m->data.size() - m->pos);
    memcpy(buf, m->data.data() + m->pos, n);
    m->pos += n;
    return static_cast<int>(n);
  };
  reader.seek = [](void* ctx, size_t offset) -> bool {
    auto* m = static_cast<MemReader*>(ctx);
    if (offset > m->data.size()) return false;
    m->pos = offset;
    return true;
  };
  reader.size = [](void* ctx) -> size_t { return static_cast<MemReader*>(ctx)->data.size(); };
  return reader;
}

std::string extract(const std::string& pdf, bool& ok) {
  MemReader mem{pdf, 0};
  std::string out;
  size_t pages = 0;
  ok = PdfText::extractText(
      makeReader(mem),
      [](void* ctx, const char* data, size_t len) {
        static_cast<std::string*>(ctx)->append(data, len);
        return true;
      },
      &out, storedInflate, &pages);
  EXPECT_EQ(pages, 1u);
  return out;
}

TEST(PdfTextTest, RejectsNonPdf) {
  MemReader mem{"not a pdf at all", 0};
  std::string out;
  EXPECT_FALSE(PdfText::extractText(
      makeReader(mem),
      [](void*, const char*, size_t) { return true; }, &out, storedInflate, nullptr));
}

TEST(PdfTextTest, LiteralStringTj) {
  bool ok = false;
  const std::string text = extract(makePdf("", "BT /F1 12 Tf (Hello PDF) Tj ET"), ok);
  ASSERT_TRUE(ok);
  EXPECT_NE(text.find("Hello PDF"), std::string::npos);
}

TEST(PdfTextTest, EscapedParensAndNewlineInLiteral) {
  bool ok = false;
  const std::string text = extract(makePdf("", "BT (a \\(b\\) c) Tj ET"), ok);
  ASSERT_TRUE(ok);
  EXPECT_NE(text.find("a (b) c"), std::string::npos);
}

TEST(PdfTextTest, HexStringAndTjArray) {
  bool ok = false;
  const std::string text = extract(makePdf("", "BT <486578> Tj [(Ar) 120 (ray)] TJ ET"), ok);
  ASSERT_TRUE(ok);
  EXPECT_NE(text.find("Hex"), std::string::npos);
  EXPECT_NE(text.find("Array"), std::string::npos);
}

TEST(PdfTextTest, FlateDecodeStream) {
  bool ok = false;
  const std::string content = "BT (Flated text) Tj ET";
  const std::string text = extract(makePdf(" /Filter /FlateDecode", zlibStored(content)), ok);
  ASSERT_TRUE(ok);
  EXPECT_NE(text.find("Flated text"), std::string::npos);
}

TEST(PdfTextTest, TextPositioningAddsLineBreaks) {
  bool ok = false;
  const std::string text = extract(makePdf("", "BT (line1) Tj 0 -14 Td (line2) Tj ET"), ok);
  ASSERT_TRUE(ok);
  const size_t first = text.find("line1");
  const size_t nl = text.find('\n', first);
  const size_t second = text.find("line2");
  ASSERT_NE(first, std::string::npos);
  ASSERT_NE(nl, std::string::npos);
  ASSERT_NE(second, std::string::npos);
  EXPECT_LT(nl, second);
}

}  // namespace
