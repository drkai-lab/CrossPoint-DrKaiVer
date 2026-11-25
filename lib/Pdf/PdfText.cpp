#include "PdfText.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr size_t kChunkSize = 8192;
// FlateDecode streams are inflated whole (the repo's inflate path is
// one-shot), so compressed streams above this cap are skipped: declared
// limit in docs/SHIFT_LEFT.md. Uncompressed streams are capped higher
// because they are tokenized directly without a second copy.
constexpr size_t kMaxFlateStreamBytes = 512 * 1024;
constexpr size_t kMaxRawStreamBytes = 4 * 1024 * 1024;

void appendUtf8(std::string& out, uint32_t cp) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

// Decode raw string bytes to UTF-8 text: UTF-16BE with BOM when present,
// PDFDocEncoding approximated as Latin-1 otherwise (declared limitation;
// no ToUnicode CMap support).
void decodeBytes(const std::string& raw, std::string& out) {
  if (raw.size() >= 2 && static_cast<uint8_t>(raw[0]) == 0xFE && static_cast<uint8_t>(raw[1]) == 0xFF) {
    for (size_t i = 2; i + 1 < raw.size(); i += 2) {
      appendUtf8(out, (static_cast<uint8_t>(raw[i]) << 8) | static_cast<uint8_t>(raw[i + 1]));
    }
    return;
  }
  for (const unsigned char c : raw) {
    if (c < 0x80) {
      out.push_back(static_cast<char>(c));
    } else {
      appendUtf8(out, c);
    }
  }
}

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool parseLiteral(const std::string& s, size_t& pos, std::string& raw) {
  if (pos >= s.size() || s[pos] != '(') return false;
  pos++;
  int depth = 1;
  while (pos < s.size() && depth > 0) {
    const char c = s[pos++];
    if (c == '\\' && pos < s.size()) {
      const char esc = s[pos++];
      switch (esc) {
        case 'n': raw.push_back('\n'); break;
        case 'r': raw.push_back('\r'); break;
        case 't': raw.push_back('\t'); break;
        case 'b': raw.push_back('\b'); break;
        case 'f': raw.push_back('\f'); break;
        case '(': raw.push_back('('); break;
        case ')': raw.push_back(')'); break;
        case '\\': raw.push_back('\\'); break;
        case '\n': break;
        case '\r':
          if (pos < s.size() && s[pos] == '\n') pos++;
          break;
        default:
          if (esc >= '0' && esc <= '7') {
            int val = esc - '0';
            for (int k = 0; k < 2 && pos < s.size() && s[pos] >= '0' && s[pos] <= '7'; k++) {
              val = val * 8 + (s[pos++] - '0');
            }
            raw.push_back(static_cast<char>(val));
          } else {
            raw.push_back(esc);
          }
          break;
      }
    } else if (c == '(') {
      depth++;
      raw.push_back(c);
    } else if (c == ')') {
      depth--;
      if (depth > 0) raw.push_back(c);
    } else {
      raw.push_back(c);
    }
  }
  return depth == 0;
}

bool parseHex(const std::string& s, size_t& pos, std::string& raw) {
  if (pos >= s.size() || s[pos] != '<' || (pos + 1 < s.size() && s[pos + 1] == '<')) return false;
  pos++;
  int hi = -1;
  while (pos < s.size() && s[pos] != '>') {
    const int v = hexVal(s[pos++]);
    if (v < 0) continue;
    if (hi < 0) {
      hi = v;
    } else {
      raw.push_back(static_cast<char>((hi << 4) | v));
      hi = -1;
    }
  }
  if (pos < s.size()) pos++;
  if (hi >= 0) raw.push_back(static_cast<char>(hi << 4));
  return true;
}

bool isWordChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '*' || c == '\'' || c == '"';
}

bool emitContentText(const std::string& content, PdfText::SinkFn sink, void* sinkCtx) {
  std::string out;
  auto flush = [&]() {
    if (!out.empty()) {
      const bool ok = sink(sinkCtx, out.data(), out.size());
      out.clear();
      return ok;
    }
    return true;
  };
  std::string lastString;
  bool hasString = false;
  bool inText = false;
  size_t i = 0;
  while (i < content.size()) {
    const char c = content[i];
    if (c == '(') {
      std::string raw;
      if (!parseLiteral(content, i, raw)) return false;
      lastString.clear();
      decodeBytes(raw, lastString);
      hasString = true;
      continue;
    }
    if (c == '<' && !(i + 1 < content.size() && content[i + 1] == '<')) {
      std::string raw;
      if (!parseHex(content, i, raw)) return false;
      lastString.clear();
      decodeBytes(raw, lastString);
      hasString = true;
      continue;
    }
    if (isWordChar(c)) {
      size_t j = i;
      while (j < content.size() && isWordChar(content[j])) j++;
      const std::string op = content.substr(i, j - i);
      i = j;
      if (op == "BT") {
        inText = true;
      } else if (op == "ET") {
        if (inText && !out.empty() && out.back() != '\n') out.push_back('\n');
        inText = false;
        if (!flush()) return false;
      } else if (inText && (op == "Tj" || op == "'" || op == "\"")) {
        if (hasString) out += lastString;
        if (op != "Tj") out.push_back('\n');
        hasString = false;
      } else if (inText && (op == "Td" || op == "TD" || op == "T*")) {
        if (!out.empty() && out.back() != '\n') out.push_back('\n');
      }
      continue;
    }
    if (c == '[') {
      i++;
      std::string joined;
      while (i < content.size() && content[i] != ']') {
        if (content[i] == '(') {
          std::string raw;
          if (!parseLiteral(content, i, raw)) return false;
          decodeBytes(raw, joined);
        } else if (content[i] == '<' && !(i + 1 < content.size() && content[i + 1] == '<')) {
          std::string raw;
          if (!parseHex(content, i, raw)) return false;
          decodeBytes(raw, joined);
        } else {
          i++;
        }
      }
      if (i < content.size()) i++;
      lastString = joined;
      hasString = true;
      size_t k = i;
      while (k < content.size() && (content[k] == ' ' || content[k] == '\n' || content[k] == '\r' || content[k] == '\t'))
        k++;
      if (content.compare(k, 2, "TJ") == 0) {
        if (inText) out += lastString;
        hasString = false;
        i = k + 2;
      }
      continue;
    }
    i++;
  }
  return flush();
}

// Chunked pass counting /Type /Page (not /Pages) and checking the header
// and /Encrypt marker. Peak RAM: one chunk + 16-byte overlap.
bool scanPass(const PdfText::Reader& reader, size_t& pages, bool& isPdf, bool& encrypted) {
  pages = 0;
  isPdf = false;
  encrypted = false;
  if (reader.seek && !reader.seek(reader.ctx, 0)) return false;
  std::string tail;
  std::string chunk(kChunkSize, '\0');
  bool first = true;
  int n = 0;
  while ((n = reader.read(reader.ctx, reinterpret_cast<uint8_t*>(chunk.data()), chunk.size())) > 0) {
    std::string window = tail + chunk.substr(0, static_cast<size_t>(n));
    if (first) {
      isPdf = window.compare(0, 5, "%PDF-") == 0;
      first = false;
    }
    if (window.find("/Encrypt") != std::string::npos) encrypted = true;
    size_t pos = 0;
    while ((pos = window.find("/Type", pos)) != std::string::npos) {
      // Only count occurrences that end before the overlap tail, so a
      // pattern split across chunks is counted exactly once (next pass).
      if (pos + 5 > window.size() - tail.size() && tail.size() > 0) break;
      size_t p = pos + 5;
      while (p < window.size() && (window[p] == ' ' || window[p] == '\n' || window[p] == '\r' || window[p] == '\t'))
        p++;
      if (window.compare(p, 5, "/Page") == 0 && window.compare(p, 6, "/Pages") != 0) pages++;
      pos = p;
    }
    tail = window.size() > 16 ? window.substr(window.size() - 16) : window;
  }
  return n == 0;
}

}  // namespace

bool PdfText::extractText(const Reader& reader, SinkFn sink, void* sinkCtx, InflateFn inflate, size_t* pageCountOut) {
  if (!reader.read || !sink) return false;
  size_t pages = 0;
  bool isPdf = false;
  bool encrypted = false;
  if (!scanPass(reader, pages, isPdf, encrypted)) return false;
  if (!isPdf || encrypted) return false;
  if (pageCountOut) *pageCountOut = pages;

  if (reader.seek && !reader.seek(reader.ctx, 0)) return false;

  // Streaming pass: `pending` holds unconsumed bytes, `history` the last
  // 1 KiB before a stream marker (its dictionary), `streamBuf` the single
  // stream currently being collected. Nothing else grows with file size.
  std::string pending;
  std::string history;
  std::string streamBuf;
  std::string chunk(kChunkSize, '\0');
  bool inStream = false;
  bool streamFlate = false;
  bool anyStream = false;
  bool eof = false;

  auto processStream = [&](const std::string& raw) -> bool {
    std::string content;
    if (streamFlate) {
      if (inflate == nullptr) return true;
      if (raw.size() > kMaxFlateStreamBytes) return true;  // declared cap
      if (!inflate(reinterpret_cast<const uint8_t*>(raw.data()), raw.size(), content)) return true;
    } else {
      content = raw;
    }
    if (content.find("BT") == std::string::npos) return true;  // image/font stream
    anyStream = true;
    if (!emitContentText(content, sink, sinkCtx)) return false;
    return sink(sinkCtx, "\f", 1);
  };

  while (!eof || !pending.empty() || inStream) {
    if (!eof && (inStream || pending.size() < kChunkSize)) {
      const int n = reader.read(reader.ctx, reinterpret_cast<uint8_t*>(chunk.data()), chunk.size());
      if (n < 0) return false;
      if (n == 0) {
        eof = true;
      } else if (inStream) {
        streamBuf.append(chunk.data(), static_cast<size_t>(n));
      } else {
        pending.append(chunk.data(), static_cast<size_t>(n));
      }
    }

    if (inStream) {
      const size_t end = streamBuf.find("endstream");
      if (end == std::string::npos) {
        if (streamBuf.size() > kMaxRawStreamBytes) return false;  // declared cap
        if (eof) break;
        continue;
      }
      size_t realEnd = end;
      while (realEnd > 0 && (streamBuf[realEnd - 1] == '\n' || streamBuf[realEnd - 1] == '\r')) realEnd--;
      const std::string raw = streamBuf.substr(0, realEnd);
      pending = streamBuf.substr(end + 9);
      streamBuf.clear();
      inStream = false;
      if (!processStream(raw)) return false;
      continue;
    }

    // Find the next genuine "stream" keyword (not the tail of "endstream").
    size_t searchFrom = 0;
    size_t marker = std::string::npos;
    while (true) {
      const size_t cand = pending.find("stream", searchFrom);
      if (cand == std::string::npos) break;
      const bool isEnd = (cand >= 3 && pending.compare(cand - 3, 3, "end") == 0) ||
                         (cand < 3 && (history + pending.substr(0, cand)).size() >= 3 &&
                          (history + pending.substr(0, cand)).compare((history + pending.substr(0, cand)).size() - 3, 3,
                                                                    "end") == 0);
      if (isEnd) {
        searchFrom = cand + 6;
        continue;
      }
      marker = cand;
      break;
    }
    if (marker == std::string::npos) {
      // Keep a tail that can still complete a marker or its "end" prefix.
      if (pending.size() > 16) {
        history += pending.substr(0, pending.size() - 16);
        if (history.size() > 1024) history.erase(0, history.size() - 1024);
        pending.erase(0, pending.size() - 16);
      }
      if (eof) break;
      continue;
    }
    // Need the EOL after the keyword before consuming.
    if (pending.size() < marker + 8 && !eof) continue;
    history += pending.substr(0, marker);
    if (history.size() > 1024) history.erase(0, history.size() - 1024);
    streamFlate = history.find("FlateDecode") != std::string::npos || history.find("/Fl") != std::string::npos;
    size_t start = marker + 6;
    if (start < pending.size() && pending[start] == '\r') start++;
    if (start < pending.size() && pending[start] == '\n') start++;
    streamBuf = pending.substr(start);
    pending.clear();
    history.clear();
    inStream = true;
  }
  return anyStream;
}
