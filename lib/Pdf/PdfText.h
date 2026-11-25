#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Streaming, text-only PDF extractor (docs/SHIFT_LEFT.md R3d).
//
// Design for the ESP32-C3 RAM budget: the file is read through a seekable
// reader callback, one object stream at a time is held in memory, and
// extracted text is emitted through a sink callback page by page. The whole
// PDF and the whole extracted text are never resident at once.
//
// Supported: uncompressed and FlateDecode content streams (inflate is
// injected; on device it is backed by miniz, in tests by a stored-block
// inflater), literal and hex strings, Tj / TJ / ' / " operators, and
// Td / TD / T* / ET line breaks. Page count comes from /Type /Page objects.
//
// Not supported (declared limits): embedded-font ToUnicode CMaps (UTF-16BE
// hex strings with a BOM are decoded; other encodings may surface as raw
// bytes), image/vector rendering, forms, JavaScript, encrypted PDFs.
class PdfText {
 public:
  struct Reader {
    void* ctx = nullptr;
    // Reads up to `size` bytes at the current position; returns bytes read,
    // 0 at EOF, <0 on error (same contract as TxtToHtml's reader).
    int (*read)(void* ctx, uint8_t* buf, size_t size) = nullptr;
    bool (*seek)(void* ctx, size_t offset) = nullptr;
    size_t (*size)(void* ctx) = nullptr;
  };

  // Inflate one complete zlib/deflate stream. Returns false on error.
  using InflateFn = bool (*)(const uint8_t* in, size_t inLen, std::string& out);
  // Text sink: receives extracted UTF-8 text fragments (page text, with
  // '\n' line breaks and '\f' page breaks). Return false to abort.
  using SinkFn = bool (*)(void* ctx, const char* data, size_t len);

  // Scans the file, extracts text stream by stream. Sets *pageCountOut
  // (when non-null) to the number of /Type /Page objects found.
  static bool extractText(const Reader& reader, SinkFn sink, void* sinkCtx, InflateFn inflate,
                          size_t* pageCountOut);
};
