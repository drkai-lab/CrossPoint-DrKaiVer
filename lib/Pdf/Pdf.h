#pragma once

#include <Print.h>

#include <memory>
#include <string>
#include <string_view>

class BookMetadataCache;

// Device-side PDF book support (docs/SHIFT_LEFT.md R3d): a PDF is converted
// once into a plain-text file inside its SD cache directory
// (/.crosspoint/epub_<hash>/pdf_text.txt) by the streaming PdfText
// extractor, then read through the exact same cache/section pipeline as a
// .txt book. Only the extracted text file is paged from SD; the PDF itself
// is never fully loaded into RAM (extraction holds at most one content
// stream, see PdfText.h).
class Pdf {
 public:
  static bool isPdf(std::string_view path);
  static std::string extractedTextPath(const std::string& cachePath);
  // Cache is valid iff the extracted text exists and the recorded source
  // size matches the current PDF file size.
  static bool validateCache(const std::string& filepath, const std::string& cachePath);
  static bool buildPdfCache(const std::string& filepath, const std::string& cachePath,
                            std::unique_ptr<BookMetadataCache>& bookMetadataCache);
  static bool streamPdfToHtml(const std::string& cachePath, Print& out);
  static bool extractedTextSize(const std::string& cachePath, size_t* size);

  // LRU cap for extracted-text files under the book-cache root, enforced
  // through FileCache after each extraction. Set from Settings at boot
  // (main.cpp); defaults to FileCache::kDefaultMaxBytes (64 MiB).
  static void setCacheMaxBytes(uint64_t bytes);
  static uint64_t cacheMaxBytes();
};
