#pragma once

#include <Print.h>

#include <string_view>

// Lightweight streaming Markdown -> HTML converter for .md books.
// Deliberate subset (docs/SHIFT_LEFT.md R3b): ATX headings, **bold**,
// *italic* / _italic_, `code`, fenced code blocks, - / * and 1. lists,
// [text](url) links, > blockquotes, --- rules. Raw HTML is escaped.
// Input arrives through a reader callback in chunks and is processed one
// line at a time; lines longer than kMaxLineBytes are split, so RAM use is
// bounded by the line buffer, never by the file size.
class MarkdownToHtml {
 public:
  static constexpr size_t kMaxLineBytes = 4096;

  static bool stream(void* readerCtx, int (*readFn)(void* ctx, uint8_t* buf, size_t size), Print& out);
  static bool stream(std::string_view content, Print& out);
};
