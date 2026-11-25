#pragma once

#include "FileCache.h"

// Builds the FileOps table backed by the device SD card (HalStorage).
// readFile caps the index file at 64 KiB; the LRU index is small by design
// (kMaxEntries entries), payloads are never read through this adapter.
FileCache::FileOps makeHalFileCacheOps();
