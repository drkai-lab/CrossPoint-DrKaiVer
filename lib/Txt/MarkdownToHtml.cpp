#include "MarkdownToHtml.h"

#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "TxtToHtml.h"

namespace {

struct Emitter {
  Print& out;
  std::string buf;
  bool ok = true;
  void write(std::string_view s) {
    buf.append(s);
    if (buf.size() >= 4096) flush();
  }
  void flush() {
    if (!buf.empty()) {
      ok = ok && (out.write(reinterpret_cast<const uint8_t*>(buf.data()), buf.size()) == buf.size());
      buf.clear();
    }
  }
};

void escapeInto(Emitter& e, std::string_view text) {
  TxtToHtml::streamEscapedHtml([&e](std::string_view f) { e.write(f); }, text);
}

// Escape a URL for an href attribute (the EPUB engine resolves relative
// links; absolute http(s)/mailto links pass through).
void escapeUrl(Emitter& e, std::string_view url) {
  for (const char c : url) {
    if (c == '"') {
      e.write("&quot;");
    } else if (c == '&') {
      e.write("&amp;");
    } else {
      e.write(std::string_view(&c, 1));
    }
  }
}

// Render inline markup within one line: **bold**, *italic*, _italic_,
// `code`, [text](url). Everything else is HTML-escaped plain text.
void renderInline(Emitter& e, std::string_view line) {
  size_t i = 0;
  while (i < line.size()) {
    if (line.substr(i, 2) == "**") {
      const size_t end = line.find("**", i + 2);
      if (end != std::string_view::npos && end > i + 2) {
        e.write("<b>");
        escapeInto(e, line.substr(i + 2, end - i - 2));
        e.write("</b>");
        i = end + 2;
        continue;
      }
    }
    if (line[i] == '*' || line[i] == '_') {
      const char marker = line[i];
      const size_t end = line.find(marker, i + 1);
      if (end != std::string_view::npos && end > i + 1) {
        e.write("<i>");
        escapeInto(e, line.substr(i + 1, end - i - 1));
        e.write("</i>");
        i = end + 1;
        continue;
      }
    }
    if (line[i] == '`') {
      const size_t end = line.find('`', i + 1);
      if (end != std::string_view::npos) {
        e.write("<code>");
        escapeInto(e, line.substr(i + 1, end - i - 1));
        e.write("</code>");
        i = end + 1;
        continue;
      }
    }
    if (line[i] == '[') {
      const size_t close = line.find("](", i);
      if (close != std::string_view::npos) {
        const size_t paren = line.find(')', close + 2);
        if (paren != std::string_view::npos) {
          e.write("<a href=\"");
          escapeUrl(e, line.substr(close + 2, paren - close - 2));
          e.write("\">");
          escapeInto(e, line.substr(i + 1, close - i - 1));
          e.write("</a>");
          i = paren + 1;
          continue;
        }
      }
    }
    escapeInto(e, line.substr(i, 1));
    i++;
  }
}

std::string_view trimLeft(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  return s;
}

struct Converter {
  Emitter e;
  bool inFence = false;
  bool inUl = false;
  bool inOl = false;
  bool inPara = false;

  void closeLists() {
    if (inUl) e.write("</ul>\n");
    if (inOl) e.write("</ol>\n");
    inUl = inOl = false;
  }
  void closePara() {
    if (inPara) e.write("</p>\n");
    inPara = false;
  }

  void line(std::string_view raw) {
    if (!raw.empty() && raw.back() == '\r') raw.remove_suffix(1);
    const std::string_view t = trimLeft(raw);

    if (t.substr(0, 3) == "```") {
      if (inFence) {
        e.write("</pre>\n");
        inFence = false;
      } else {
        closePara();
        closeLists();
        e.write("<pre>");
        inFence = true;
      }
      return;
    }
    if (inFence) {
      escapeInto(e, raw);
      e.write("\n");
      return;
    }
    if (t.empty()) {
      closePara();
      closeLists();
      return;
    }
    // ATX heading: 1-6 '#' followed by a space.
    size_t hashes = 0;
    while (hashes < t.size() && t[hashes] == '#') hashes++;
    if (hashes >= 1 && hashes <= 6 && hashes < t.size() && t[hashes] == ' ') {
      closePara();
      closeLists();
      const std::string tag = "h" + std::to_string(hashes);
      e.write("<" + tag + ">");
      renderInline(e, trimLeft(t.substr(hashes)));
      e.write("</" + tag + ">\n");
      return;
    }
    if (t == "---" || t == "***" || t == "___") {
      closePara();
      closeLists();
      e.write("<hr />\n");
      return;
    }
    if (t.front() == '>' && (t.size() == 1 || t[1] == ' ')) {
      closePara();
      closeLists();
      e.write("<blockquote>");
      renderInline(e, trimLeft(t.substr(1)));
      e.write("</blockquote>\n");
      return;
    }
    // Unordered list item.
    if ((t.substr(0, 2) == "- " || t.substr(0, 2) == "* ") && t.size() > 2) {
      closePara();
      if (inOl) {
        e.write("</ol>\n");
        inOl = false;
      }
      if (!inUl) {
        e.write("<ul>\n");
        inUl = true;
      }
      e.write("<li>");
      renderInline(e, t.substr(2));
      e.write("</li>\n");
      return;
    }
    // Ordered list item: digits + ". ".
    size_t digits = 0;
    while (digits < t.size() && t[digits] >= '0' && t[digits] <= '9') digits++;
    if (digits >= 1 && digits + 1 < t.size() && t[digits] == '.' && t[digits + 1] == ' ') {
      closePara();
      if (inUl) {
        e.write("</ul>\n");
        inUl = false;
      }
      if (!inOl) {
        e.write("<ol>\n");
        inOl = true;
      }
      e.write("<li>");
      renderInline(e, t.substr(digits + 2));
      e.write("</li>\n");
      return;
    }
    closeLists();
    if (!inPara) {
      e.write("<p>");
      inPara = true;
    } else {
      e.write(" ");
    }
    renderInline(e, t);
  }

  void finish() {
    if (inFence) e.write("</pre>\n");
    closePara();
    closeLists();
  }
};

}  // namespace

bool MarkdownToHtml::stream(void* readerCtx, int (*readFn)(void* ctx, uint8_t* buf, size_t size), Print& out) {
  constexpr size_t IN_BUF_SIZE = 8192;
  auto inBuf = makeUniqueNoThrow<uint8_t[]>(IN_BUF_SIZE);
  if (!inBuf) {
    LOG_ERR("MD", "OOM: Markdown streaming buffer");
    return false;
  }
  Converter conv{Emitter{out}};
  std::string line;
  line.reserve(512);
  bool first = true;
  int bytesRead = 0;
  while ((bytesRead = readFn(readerCtx, inBuf.get(), IN_BUF_SIZE)) > 0) {
    int startIdx = 0;
    if (first) {
      first = false;
      if (bytesRead >= 3 && inBuf[0] == 0xEF && inBuf[1] == 0xBB && inBuf[2] == 0xBF) startIdx = 3;
    }
    for (int i = startIdx; i < bytesRead; i++) {
      const char c = static_cast<char>(inBuf[i]);
      if (c == '\n') {
        conv.line(line);
        line.clear();
      } else {
        if (line.size() < kMaxLineBytes) line.push_back(c);
      }
    }
  }
  if (bytesRead < 0) {
    LOG_ERR("MD", "Read error while streaming Markdown");
    return false;
  }
  if (!line.empty()) conv.line(line);
  conv.finish();
  conv.e.flush();
  return conv.e.ok;
}

bool MarkdownToHtml::stream(std::string_view content, Print& out) {
  struct ViewReader {
    std::string_view s;
    size_t pos = 0;
  } reader{content, 0};
  return stream(
      &reader,
      [](void* ctx, uint8_t* buf, size_t size) -> int {
        auto* r = static_cast<ViewReader*>(ctx);
        if (r->pos >= r->s.size()) return 0;
        const size_t n = std::min(size, r->s.size() - r->pos);
        memcpy(buf, r->s.data() + r->pos, n);
        r->pos += n;
        return static_cast<int>(n);
      },
      out);
}
