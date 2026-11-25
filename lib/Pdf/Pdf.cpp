#include "Pdf.h"

#include <Epub/BookMetadataCache.h>
#include <FileCache.h>
#include <FileCacheHal.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <InflateStream.h>
#include <Logging.h>
#include <Memory.h>
#include <TxtToHtml.h>
#include <Utf8.h>

#include <cstring>

#include "PdfText.h"

namespace {

std::string sourceSizePath(const std::string& cachePath) { return cachePath + "/pdf_src_size.txt"; }

// One-shot-output streaming inflate backed by miniz (lib/miniz): streaming
// mode allocates a 32 KiB window transiently for the scope of one stream.
bool pdfInflate(const uint8_t* in, size_t inLen, std::string& out) {
  InflateStream inflate;
  if (!inflate.init(true)) return false;
  inflate.setSource(in, inLen);
  inflate.setZlibWrapped();
  uint8_t buf[4096];
  size_t produced = 0;
  constexpr size_t kMaxInflatedBytes = 4 * 1024 * 1024;  // declared cap
  while (true) {
    const InflateStream::Status status = inflate.readAtMost(buf, sizeof(buf), &produced);
    if (produced > 0) {
      out.append(reinterpret_cast<const char*>(buf), produced);
      if (out.size() > kMaxInflatedBytes) {
        inflate.deinit();
        return false;
      }
    }
    if (status == InflateStream::Status::Done) {
      inflate.deinit();
      return true;
    }
    if (status == InflateStream::Status::Error) {
      inflate.deinit();
      return false;
    }
  }
}

struct ExtractCtx {
  HalFile* src;
  HalFile* dst;
};

}  // namespace

namespace {
uint64_t gCacheMaxBytes = FileCache::kDefaultMaxBytes;
}  // namespace

void Pdf::setCacheMaxBytes(uint64_t bytes) { gCacheMaxBytes = bytes == 0 ? FileCache::kDefaultMaxBytes : bytes; }
uint64_t Pdf::cacheMaxBytes() { return gCacheMaxBytes; }

bool Pdf::isPdf(std::string_view path) { return FsHelpers::hasPdfExtension(path); }

std::string Pdf::extractedTextPath(const std::string& cachePath) { return cachePath + "/pdf_text.txt"; }

bool Pdf::validateCache(const std::string& filepath, const std::string& cachePath) {
  const std::string textPath = extractedTextPath(cachePath);
  if (!Storage.exists(textPath.c_str())) return false;
  std::string recorded;
  if (!Storage.readFileToString("PDF", sourceSizePath(cachePath), 32, recorded)) return false;
  HalFile src;
  if (!Storage.openFileForRead("PDF", filepath, src)) return false;
  return recorded == std::to_string(src.size());
}

bool Pdf::buildPdfCache(const std::string& filepath, const std::string& cachePath,
                        std::unique_ptr<BookMetadataCache>& bookMetadataCache) {
  LOG_DBG("PDF", "Extracting text for PDF book: %s", filepath.c_str());
  if (!Storage.exists(cachePath.c_str())) {
    Storage.mkdir(cachePath.c_str());
  }

  HalFile src, dst;
  if (!Storage.openFileForRead("PDF", filepath, src)) {
    LOG_ERR("PDF", "Failed to open PDF: %s", filepath.c_str());
    return false;
  }
  const size_t srcSize = src.size();
  const std::string textPath = extractedTextPath(cachePath);
  if (!Storage.openFileForWrite("PDF", textPath, dst)) {
    LOG_ERR("PDF", "Failed to open extracted-text cache for write: %s", textPath.c_str());
    return false;
  }

  ExtractCtx ctx{&src, &dst};
  PdfText::Reader reader;
  reader.ctx = &ctx;
  reader.read = [](void* c, uint8_t* buf, size_t size) { return static_cast<ExtractCtx*>(c)->src->read(buf, size); };
  reader.seek = [](void* c, size_t offset) { return static_cast<ExtractCtx*>(c)->src->seek(offset); };
  reader.size = [](void* c) { return static_cast<ExtractCtx*>(c)->src->size(); };

  size_t pageCount = 0;
  const bool ok = PdfText::extractText(
      reader,
      [](void* c, const char* data, size_t len) {
        auto* ctx2 = static_cast<ExtractCtx*>(c);
        return ctx2->dst->write(reinterpret_cast<const uint8_t*>(data), len) == len;
      },
      &ctx, pdfInflate, &pageCount);
  dst.flush();
  dst.close();
  src.close();
  if (!ok) {
    LOG_ERR("PDF", "Text extraction failed (unsupported or encrypted PDF?): %s", filepath.c_str());
    Storage.remove(textPath.c_str());
    return false;
  }
  LOG_DBG("PDF", "Extracted text from %zu page(s) of %s", pageCount, filepath.c_str());

  // Register the extracted text in the shared SD file cache and enforce
  // the LRU cap: the coldest extracted texts are evicted first (their
  // books simply re-extract on next open; validateCache notices the
  // missing file). RAM cost: the bounded FileCache index only.
  {
    const size_t slash = cachePath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      FileCache cache(makeHalFileCacheOps(), cachePath.substr(0, slash), gCacheMaxBytes);
      cache.loadIndex();
      const std::string key = cachePath.substr(slash + 1) + "/pdf_text.txt";
      cache.recordAccess(key, millis());
      cache.enforceLimit(key);
      cache.saveIndex();
    }
  }

  if (!Storage.writeFile(sourceSizePath(cachePath).c_str(), String(std::to_string(srcSize).c_str()))) {
    LOG_ERR("PDF", "Failed to record PDF source size");
    return false;
  }

  // Build the same spine/TOC book.bin a .txt book gets, titled by the PDF
  // filename. The spine content ("content.html") is generated on demand
  // from pdf_text.txt by streamPdfToHtml().
  if (!bookMetadataCache->beginWrite()) return false;
  if (!bookMetadataCache->beginContentOpfPass()) return false;
  bookMetadataCache->createSpineEntry("content.html");
  if (!bookMetadataCache->endContentOpfPass()) return false;
  if (!bookMetadataCache->beginTocPass()) return false;
  const std::string title = FsHelpers::getFileNameWithoutExtension(filepath);
  bookMetadataCache->createTocEntry(title, "content.html", "", 0);
  if (!bookMetadataCache->endTocPass()) return false;
  if (!bookMetadataCache->endWrite()) return false;

  BookMetadataCache::BookMetadata bookMetadata;
  bookMetadata.title = utf8ComposeNfc(title);
  bookMetadata.language = "en";
  if (!bookMetadataCache->buildBookBin(textPath, bookMetadata)) {
    LOG_ERR("PDF", "Could not build book.bin for PDF");
    return false;
  }
  bookMetadataCache->cleanupTmpFiles();
  bookMetadataCache = makeUniqueNoThrow<BookMetadataCache>(cachePath);
  if (!bookMetadataCache || !bookMetadataCache->load()) {
    LOG_ERR("PDF", "Failed to reload cache after PDF build");
    return false;
  }
  return true;
}

bool Pdf::streamPdfToHtml(const std::string& cachePath, Print& out) {
  HalFile src;
  const std::string textPath = extractedTextPath(cachePath);
  if (!Storage.openFileForRead("PDF", textPath, src)) {
    LOG_ERR("PDF", "Extracted text missing: %s", textPath.c_str());
    return false;
  }
  return TxtToHtml::stream(
      "pdf_text.txt", &src, [](void* ctx, uint8_t* buf, size_t size) { return static_cast<HalFile*>(ctx)->read(buf, size); },
      out);
}

bool Pdf::extractedTextSize(const std::string& cachePath, size_t* size) {
  HalFile f;
  if (!Storage.openFileForRead("PDF", extractedTextPath(cachePath), f)) return false;
  if (size) *size = f.size();
  return true;
}
