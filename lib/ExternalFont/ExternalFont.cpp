#include "ExternalFont.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "FontFilenameParser.h"

namespace {

// EPDFont magic: 'EPDF' written little-endian.
constexpr uint8_t EPDFONT_MAGIC[4] = {'E', 'P', 'D', 'F'};
constexpr size_t EPDFONT_HEADER_SIZE = 32;
constexpr size_t EPDFONT_INTERVAL_ENTRY_SIZE = 12;
constexpr size_t EPDFONT_GLYPH_ENTRY_SIZE = 16;
constexpr uint16_t EPDFONT_VERSION_SUPPORTED = 1;

int8_t readInt8(const uint8_t* bytes) { return static_cast<int8_t>(bytes[0]); }

int16_t readInt16LE(const uint8_t* bytes) {
  return static_cast<int16_t>(static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8));
}

uint16_t readUint16LE(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t readUint32LE(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

// Transcode an EPDFont 1-bit glyph bitmap from sequential MSB-first packing
// (bit `y*W + x`) into row-aligned MSB-first packing (`bytesPerRow * y +
// x/8`, bit `7 - x%8`). The renderer's drawing path assumes row-aligned
// packing because that's what the legacy ".bin" format and the built-in CJK
// fonts use.
void transcodeEpdBitmapToRowAligned(const uint8_t* src, uint8_t* dst, uint8_t width, uint8_t height) {
  const uint8_t bytesPerRow = (width + 7) / 8;
  std::memset(dst, 0, static_cast<size_t>(bytesPerRow) * height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int srcBitIndex = y * width + x;
      const uint8_t srcByte = src[srcBitIndex >> 3];
      const int srcBitPos = 7 - (srcBitIndex & 7);
      if ((srcByte >> srcBitPos) & 1) {
        const int dstByteIndex = y * bytesPerRow + (x >> 3);
        const int dstBitPos = 7 - (x & 7);
        dst[dstByteIndex] |= static_cast<uint8_t>(1 << dstBitPos);
      }
    }
  }
}

}  // namespace

ExternalFont::~ExternalFont() { unload(); }

void ExternalFont::unload() {
  if (_fontFile) {
    _fontFile.close();
  }
  _isLoaded = false;
  _fontName[0] = '\0';
  _fontSize = 0;
  _charWidth = 0;
  _charHeight = 0;
  _bytesPerRow = 0;
  _bytesPerChar = 0;
  _isRichMetricsFormat = false;
  _fontMetrics = {};
  _intervalCount = 0;
  _glyphCount = 0;
  _glyphsOffset = 0;
  _bitmapOffset = 0;
  _accessCounter = 0;
  _lastReadOffset = 0;
  _hasLastReadOffset = false;

  delete[] _intervals;
  _intervals = nullptr;
  delete[] _cache;
  _cache = nullptr;
  delete[] _hashTable;
  _hashTable = nullptr;
}

bool ExternalFont::parseFilename(const char* filepath) {
  ParsedFontFilename parsed;
  if (!parseFontFilename(filepath, parsed)) {
    LOG_ERR("EFT", "Invalid font filename: %s", filepath ? filepath : "(null)");
    return false;
  }

  _charWidth = parsed.width;
  _charHeight = parsed.height;
  _fontSize = parsed.size;
  strncpy(_fontName, parsed.name, sizeof(_fontName) - 1);
  _fontName[sizeof(_fontName) - 1] = '\0';

  _bytesPerRow = (_charWidth + 7) / 8;
  _bytesPerChar = _bytesPerRow * _charHeight;

  if (_bytesPerChar > MAX_GLYPH_BYTES) {
    LOG_ERR("EFT", "Glyph too large: %d bytes (max %d)", _bytesPerChar, MAX_GLYPH_BYTES);
    return false;
  }

  LOG_DBG("EFT", "Parsed: name=%s, size=%d, %dx%d, %d bytes/char", _fontName, _fontSize, _charWidth, _charHeight,
          _bytesPerChar);
  return true;
}

bool ExternalFont::loadEpdFontHeader() {
  if (!_fontFile.seek(0)) {
    return false;
  }
  uint8_t header[EPDFONT_HEADER_SIZE];
  if (_fontFile.read(header, sizeof(header)) != sizeof(header)) {
    LOG_ERR("EFT", "EPDFont header truncated");
    return false;
  }
  if (std::memcmp(header, EPDFONT_MAGIC, sizeof(EPDFONT_MAGIC)) != 0) {
    return false;  // Not an EPDFont file; caller falls back to legacy .bin.
  }

  const uint16_t version = readUint16LE(header + 4);
  if (version != EPDFONT_VERSION_SUPPORTED) {
    LOG_ERR("EFT", "Unsupported EPDFont version %u (expected %u)", version, EPDFONT_VERSION_SUPPORTED);
    return false;
  }

  const uint8_t is2Bit = header[6];
  if (is2Bit) {
    LOG_ERR("EFT",
            "EPDFont %s is 2-bit antialiased; the renderer only supports 1-bit external glyphs. "
            "Re-export with 1-bit mode.",
            _fontName);
    return false;
  }

  const uint8_t advanceY = header[8];
  const int8_t ascender = readInt8(header + 9);
  const int8_t descender = readInt8(header + 10);
  _fontMetrics.lineHeight = advanceY;
  _fontMetrics.ascender = ascender;
  _fontMetrics.descender = descender;

  _intervalCount = readUint32LE(header + 12);
  _glyphCount = readUint32LE(header + 16);
  const uint32_t intervalsOffset = readUint32LE(header + 20);
  _glyphsOffset = readUint32LE(header + 24);
  _bitmapOffset = readUint32LE(header + 28);

  if (intervalsOffset != EPDFONT_HEADER_SIZE || _intervalCount == 0 || _glyphCount == 0 ||
      _glyphsOffset != intervalsOffset + _intervalCount * EPDFONT_INTERVAL_ENTRY_SIZE ||
      _bitmapOffset != _glyphsOffset + _glyphCount * EPDFONT_GLYPH_ENTRY_SIZE) {
    LOG_ERR("EFT", "EPDFont layout invalid: intervals=%u (offset %u), glyphs=%u (offset %u), bitmap offset %u",
            _intervalCount, intervalsOffset, _glyphCount, _glyphsOffset, _bitmapOffset);
    return false;
  }

  if (!_fontFile.seek(intervalsOffset)) {
    return false;
  }
  return readEpdIntervals();
}

bool ExternalFont::readEpdIntervals() {
  _intervals = new (std::nothrow) EpdInterval[_intervalCount];
  if (!_intervals) {
    LOG_ERR("EFT", "Failed to allocate %u EPDFont intervals", _intervalCount);
    return false;
  }
  for (uint32_t i = 0; i < _intervalCount; ++i) {
    uint8_t entry[EPDFONT_INTERVAL_ENTRY_SIZE];
    if (_fontFile.read(entry, sizeof(entry)) != sizeof(entry)) {
      LOG_ERR("EFT", "EPDFont interval %u truncated", i);
      return false;
    }
    _intervals[i].start = readUint32LE(entry);
    _intervals[i].end = readUint32LE(entry + 4);
    _intervals[i].glyphOffset = readUint32LE(entry + 8);
    if (_intervals[i].start > _intervals[i].end) {
      LOG_ERR("EFT", "EPDFont interval %u invalid: start=%u > end=%u", i, _intervals[i].start, _intervals[i].end);
      return false;
    }
  }
  return true;
}

int ExternalFont::findEpdInterval(uint32_t codepoint) const {
  // Binary search: intervals are sorted by start (the converter writes them
  // sorted) and they don't overlap.
  if (!_intervals || _intervalCount == 0) return -1;
  int lo = 0;
  int hi = static_cast<int>(_intervalCount) - 1;
  while (lo <= hi) {
    const int mid = lo + (hi - lo) / 2;
    const EpdInterval& iv = _intervals[mid];
    if (codepoint < iv.start) {
      hi = mid - 1;
    } else if (codepoint > iv.end) {
      lo = mid + 1;
    } else {
      return mid;
    }
  }
  return -1;
}

bool ExternalFont::readEpdGlyphEntry(uint32_t glyphIndex, ExternalGlyphMetrics* out, uint32_t* outDataLength,
                                     uint32_t* outDataOffset) const {
  if (!_fontFile || glyphIndex >= _glyphCount) return false;
  const uint32_t offset = _glyphsOffset + glyphIndex * EPDFONT_GLYPH_ENTRY_SIZE;
  if (!_fontFile.seek(offset)) {
    _hasLastReadOffset = false;
    return false;
  }
  uint8_t entry[EPDFONT_GLYPH_ENTRY_SIZE];
  if (_fontFile.read(entry, sizeof(entry)) != sizeof(entry)) {
    _hasLastReadOffset = false;
    return false;
  }
  _hasLastReadOffset = false;  // we just seeked away from the bitmap stream

  const uint8_t width = entry[0];
  const uint8_t height = entry[1];
  const uint8_t advanceX = entry[2];
  const int16_t left = readInt16LE(entry + 4);
  const int16_t top = readInt16LE(entry + 6);
  const uint32_t dataLength = readUint32LE(entry + 8);
  const uint32_t dataOffset = readUint32LE(entry + 12);

  if (out) {
    out->width = width;
    out->height = height;
    out->advanceX = advanceX;
    out->left = left;
    out->top = top;
    // Flag bit 0x01 marks the entry as "non-empty cached metrics", matching
    // the convention used by the renderer's per-glyph adjustment paths.
    out->flags = (width > 0 && height > 0) ? 0x01 : 0;
  }
  if (outDataLength) *outDataLength = dataLength;
  if (outDataOffset) *outDataOffset = dataOffset;
  return true;
}

bool ExternalFont::readEpdGlyphBitmap(uint32_t dataOffset, uint32_t dataLength, uint8_t width, uint8_t height,
                                      uint8_t* dst) const {
  if (!dst) return false;
  const uint8_t bytesPerRow = (width + 7) / 8;
  const uint16_t rowAlignedSize = static_cast<uint16_t>(bytesPerRow) * height;
  if (rowAlignedSize > MAX_GLYPH_BYTES) {
    LOG_ERR("EFT", "EPDFont glyph %ux%u row-aligned size %u exceeds MAX_GLYPH_BYTES (%d)", width, height,
            rowAlignedSize, MAX_GLYPH_BYTES);
    return false;
  }
  if (dataLength == 0 || width == 0 || height == 0) {
    std::memset(dst, 0, rowAlignedSize);
    return true;
  }
  // Source is sequentially packed: ceil(width*height / 8) bytes.
  const uint32_t expectedSrcLen = (static_cast<uint32_t>(width) * height + 7) / 8;
  if (dataLength != expectedSrcLen) {
    LOG_ERR("EFT", "EPDFont glyph dataLength %u != expected %u for %ux%u", dataLength, expectedSrcLen, width, height);
    return false;
  }
  // Read the source bytes into a temporary on-stack buffer (≤ MAX_GLYPH_BYTES
  // so the renderer's byte budget already covers this).
  uint8_t src[MAX_GLYPH_BYTES];
  if (dataLength > sizeof(src)) {
    LOG_ERR("EFT", "EPDFont glyph source dataLength %u exceeds tmp buffer", dataLength);
    return false;
  }
  if (!_fontFile.seek(_bitmapOffset + dataOffset)) {
    _hasLastReadOffset = false;
    return false;
  }
  if (_fontFile.read(src, dataLength) != dataLength) {
    _hasLastReadOffset = false;
    return false;
  }
  _hasLastReadOffset = false;
  transcodeEpdBitmapToRowAligned(src, dst, width, height);
  return true;
}

bool ExternalFont::load(const char* filepath) {
  unload();

  if (!parseFilename(filepath)) {
    return false;
  }

  if (!Storage.openFileForRead("EXT_FONT", filepath, _fontFile)) {
    LOG_ERR("EFT", "Failed to open: %s", filepath);
    return false;
  }

  // Probe the magic to decide between EPDFont and legacy ".bin".
  uint8_t magic[4];
  const size_t magicRead = _fontFile.read(magic, sizeof(magic));
  const bool looksLikeEpdFont = (magicRead == sizeof(magic) && std::memcmp(magic, EPDFONT_MAGIC, sizeof(magic)) == 0);

  if (looksLikeEpdFont) {
    if (!loadEpdFontHeader()) {
      _fontFile.close();
      return false;
    }
    _isRichMetricsFormat = true;
  } else {
    // Legacy .bin: rewind so per-glyph reads start at offset 0.
    if (!_fontFile.seek(0)) {
      _fontFile.close();
      return false;
    }
    _isRichMetricsFormat = false;
  }

  _cache = new (std::nothrow) CacheEntry[CACHE_SIZE];
  _hashTable = new (std::nothrow) int16_t[CACHE_SIZE];
  if (!_cache || !_hashTable) {
    LOG_ERR("EFT", "Failed to allocate glyph cache (%d bytes)",
            static_cast<int>(CACHE_SIZE * (sizeof(CacheEntry) + sizeof(int16_t))));
    delete[] _cache;
    _cache = nullptr;
    delete[] _hashTable;
    _hashTable = nullptr;
    delete[] _intervals;
    _intervals = nullptr;
    _fontFile.close();
    return false;
  }
  std::memset(_hashTable, -1, CACHE_SIZE * sizeof(int16_t));

  _isLoaded = true;
  _lastReadOffset = 0;
  _hasLastReadOffset = false;
  LOG_DBG("EFT", "Loaded: %s (cache %dKB allocated, %s format)", filepath,
          static_cast<int>(CACHE_SIZE * (sizeof(CacheEntry) + sizeof(int16_t)) / 1024),
          _isRichMetricsFormat ? "EPDFont" : "legacy .bin");
  return true;
}

int ExternalFont::findInCache(uint32_t codepoint) const {
  // O(1) hash table lookup with linear probing for collisions
  int hash = hashCodepoint(codepoint);
  for (int i = 0; i < CACHE_SIZE; i++) {
    int idx = (hash + i) % CACHE_SIZE;
    int16_t cacheIdx = _hashTable[idx];
    if (cacheIdx == -1) {
      return -1;
    }
    if (_cache[cacheIdx].codepoint == codepoint) {
      return cacheIdx;
    }
  }
  return -1;
}

int ExternalFont::getLruSlot() {
  int lruIndex = 0;
  uint32_t minUsed = _cache[0].lastUsed;

  for (int i = 1; i < CACHE_SIZE; i++) {
    if (_cache[i].codepoint == 0xFFFFFFFF) {
      return i;
    }
    if (_cache[i].lastUsed < minUsed) {
      minUsed = _cache[i].lastUsed;
      lruIndex = i;
    }
  }
  return lruIndex;
}

bool ExternalFont::readLegacyGlyphFromSD(uint32_t codepoint, uint8_t* buffer) {
  if (!_fontFile) {
    return false;
  }

  // Legacy .bin: glyphs sit at offset = codepoint * bytesPerChar.
  const uint32_t offset = static_cast<uint32_t>(codepoint) * _bytesPerChar;

  bool needSeek = true;
  if (_hasLastReadOffset && _bytesPerChar > 0) {
    const uint32_t expectedNext = _lastReadOffset + _bytesPerChar;
    if (offset == expectedNext) {
      needSeek = false;
    }
  }

  if (needSeek) {
    if (!_fontFile.seek(offset)) {
      _hasLastReadOffset = false;
      return false;
    }
  }

  size_t bytesRead = _fontFile.read(buffer, _bytesPerChar);
  _lastReadOffset = offset;
  _hasLastReadOffset = true;

  if (bytesRead != _bytesPerChar) {
    std::memset(buffer, 0, _bytesPerChar);
  }

  return true;
}

const uint8_t* ExternalFont::getGlyph(uint32_t codepoint) {
  if (!_isLoaded) {
    return nullptr;
  }

  // Cache hit (O(1) via hash table).
  int cacheIndex = findInCache(codepoint);
  if (cacheIndex >= 0) {
    _cache[cacheIndex].lastUsed = ++_accessCounter;
    if (_cache[cacheIndex].notFound) {
      return nullptr;
    }
    return _cache[cacheIndex].bitmap;
  }

  // Cache miss: pick an LRU slot and remove its old hash table entry.
  int slot = getLruSlot();
  if (_cache[slot].codepoint != 0xFFFFFFFF) {
    int oldHash = hashCodepoint(_cache[slot].codepoint);
    for (int i = 0; i < CACHE_SIZE; i++) {
      int idx = (oldHash + i) % CACHE_SIZE;
      if (_hashTable[idx] == slot) {
        _hashTable[idx] = -1;
        break;
      }
    }
  }

  uint32_t actualCodepoint = codepoint;
  bool readSuccess = false;
  ExternalGlyphMetrics readMetrics{};
  uint8_t glyphWidth = 0;
  uint8_t glyphHeight = 0;
  bool isEmpty = true;
  uint8_t minX = _charWidth;
  uint8_t maxX = 0;

  if (_isRichMetricsFormat) {
    auto fetch = [&](uint32_t cp) -> bool {
      const int intervalIdx = findEpdInterval(cp);
      if (intervalIdx < 0) return false;
      const EpdInterval& iv = _intervals[intervalIdx];
      const uint32_t glyphIndex = iv.glyphOffset + (cp - iv.start);
      uint32_t dataLength = 0;
      uint32_t dataOffset = 0;
      ExternalGlyphMetrics entry{};
      if (!readEpdGlyphEntry(glyphIndex, &entry, &dataLength, &dataOffset)) {
        return false;
      }
      if (entry.width == 0 || entry.height == 0 || dataLength == 0) {
        // Empty glyph (e.g. space). Keep metrics for advanceX / spacing logic.
        std::memset(_cache[slot].bitmap, 0, MAX_GLYPH_BYTES);
        readMetrics = entry;
        glyphWidth = entry.width;
        glyphHeight = entry.height;
        return true;
      }
      if (!readEpdGlyphBitmap(dataOffset, dataLength, entry.width, entry.height, _cache[slot].bitmap)) {
        return false;
      }
      readMetrics = entry;
      glyphWidth = entry.width;
      glyphHeight = entry.height;
      return true;
    };

    readSuccess = fetch(codepoint);
    // Fullwidth->halfwidth fallback (FF01..FF5E -> 0021..007E) for fonts that
    // only ship halfwidth ASCII glyphs.
    if (!readSuccess && codepoint >= 0xFF01 && codepoint <= 0xFF5E) {
      const uint32_t halfwidth = codepoint - 0xFEE0;
      if (fetch(halfwidth)) {
        readSuccess = true;
        actualCodepoint = halfwidth;
      }
    }
  } else {
    readSuccess = readLegacyGlyphFromSD(codepoint, _cache[slot].bitmap);
    if (!readSuccess && codepoint >= 0xFF01 && codepoint <= 0xFF5E) {
      const uint32_t halfwidth = codepoint - 0xFEE0;
      readSuccess = readLegacyGlyphFromSD(halfwidth, _cache[slot].bitmap);
      if (readSuccess) actualCodepoint = halfwidth;
    }
    glyphWidth = _charWidth;
    glyphHeight = _charHeight;
  }

  // Scan bitmap for content / minX / maxX (used by legacy metrics fallback).
  if (readSuccess && glyphWidth > 0 && glyphHeight > 0) {
    const uint8_t bytesPerRow = (glyphWidth + 7) / 8;
    minX = glyphWidth;
    for (int y = 0; y < glyphHeight; y++) {
      for (int x = 0; x < glyphWidth; x++) {
        const int byteIndex = y * bytesPerRow + (x >> 3);
        const int bitIndex = 7 - (x & 7);
        if ((_cache[slot].bitmap[byteIndex] >> bitIndex) & 1) {
          isEmpty = false;
          if (x < minX) minX = x;
          if (x > maxX) maxX = x;
        }
      }
    }
  }

  _cache[slot].codepoint = codepoint;
  _cache[slot].lastUsed = ++_accessCounter;

  // Whitespace characters (U+2000-U+200F) are expected to be empty; render
  // them with explicit widths instead of marking notFound.
  const bool isWhitespace = (codepoint >= 0x2000 && codepoint <= 0x200F);
  _cache[slot].notFound = !readSuccess || (isEmpty && !isWhitespace && codepoint > 0x7F && !_isRichMetricsFormat);

  _cache[slot].metrics = {};

  if (_isRichMetricsFormat) {
    _cache[slot].metrics = readMetrics;
    // EPDFont guarantees per-glyph metrics, so we do NOT mark empty glyphs
    // as notFound — they may legitimately be advance-only spaces.
    if (readMetrics.advanceX == 0 && isWhitespace) {
      // Defensive fallback for badly-converted fonts.
      _cache[slot].metrics.advanceX = _charWidth / 3;
    }
    if (!readSuccess) {
      _cache[slot].notFound = true;
    }
  } else {
    _cache[slot].metrics.width = _charWidth;
    _cache[slot].metrics.height = _charHeight;
    if (!isEmpty) {
      _cache[slot].metrics.left = minX;
      _cache[slot].metrics.top = _charHeight;
      const bool isFullwidth = (actualCodepoint >= 0x2E80 && actualCodepoint <= 0x9FFF) ||
                               (actualCodepoint >= 0x3000 && actualCodepoint <= 0x30FF) ||
                               (actualCodepoint >= 0xF900 && actualCodepoint <= 0xFAFF) ||
                               (actualCodepoint >= 0xFF00 && actualCodepoint <= 0xFF60);
      if (isFullwidth) {
        _cache[slot].metrics.advanceX = _charWidth;
      } else {
        const uint8_t contentAdvance = (maxX - minX + 1) + 2;
        _cache[slot].metrics.advanceX = (contentAdvance > _charWidth) ? _charWidth : contentAdvance;
      }
      _cache[slot].metrics.flags = 0x01;
    } else {
      _cache[slot].metrics.left = 0;
      _cache[slot].metrics.top = _charHeight;
      if (isWhitespace) {
        if (codepoint == 0x2003) {
          _cache[slot].metrics.advanceX = _charWidth;
        } else if (codepoint == 0x2002) {
          _cache[slot].metrics.advanceX = _charWidth / 2;
        } else if (codepoint == 0x3000) {
          _cache[slot].metrics.advanceX = _charWidth;
        } else {
          _cache[slot].metrics.advanceX = _charWidth / 3;
        }
      } else {
        _cache[slot].metrics.advanceX = _charWidth / 3;
      }
    }
  }

  // Insert into hash table.
  int hash = hashCodepoint(codepoint);
  for (int i = 0; i < CACHE_SIZE; i++) {
    int idx = (hash + i) % CACHE_SIZE;
    if (_hashTable[idx] == -1) {
      _hashTable[idx] = slot;
      break;
    }
  }

  if (_cache[slot].notFound) {
    return nullptr;
  }
  return _cache[slot].bitmap;
}

bool ExternalFont::getGlyphMetrics(uint32_t codepoint, uint8_t* outMinX, uint8_t* outAdvanceX) {
  ExternalGlyphMetrics metrics{};
  if (!getGlyphMetrics(codepoint, &metrics)) {
    return false;
  }
  if (outMinX) *outMinX = static_cast<uint8_t>(metrics.left);
  if (outAdvanceX) *outAdvanceX = static_cast<uint8_t>(std::min<uint16_t>(metrics.advanceX, 255));
  return true;
}

bool ExternalFont::getGlyphMetrics(uint32_t codepoint, ExternalGlyphMetrics* out) const {
  if (!out || !_isLoaded) return false;

  if (_cache) {
    const int idx = findInCache(codepoint);
    if (idx >= 0 && !_cache[idx].notFound) {
      *out = _cache[idx].metrics;
      return true;
    }
  }

  if (_isRichMetricsFormat) {
    const int intervalIdx = findEpdInterval(codepoint);
    if (intervalIdx < 0) return false;
    const EpdInterval& iv = _intervals[intervalIdx];
    const uint32_t glyphIndex = iv.glyphOffset + (codepoint - iv.start);
    return readEpdGlyphEntry(glyphIndex, out, nullptr, nullptr);
  }

  return false;
}

void ExternalFont::preloadGlyphs(const uint32_t* codepoints, size_t count) {
  if (!_isLoaded || !codepoints || count == 0) {
    return;
  }

  const size_t maxLoad = std::min(count, static_cast<size_t>(CACHE_SIZE));

  // Sort + dedupe so SD reads stay roughly sequential (especially for legacy
  // .bin where neighbouring codepoints are neighbouring file offsets).
  std::vector<uint32_t> sorted(codepoints, codepoints + maxLoad);
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

  LOG_DBG("EFT", "Preloading %zu unique glyphs", sorted.size());
  const unsigned long startTime = millis();

  size_t loaded = 0;
  size_t skipped = 0;
  for (uint32_t cp : sorted) {
    if (findInCache(cp) >= 0) {
      skipped++;
      continue;
    }
    getGlyph(cp);
    loaded++;
  }

  LOG_DBG("EFT", "Preload done: %zu loaded, %zu already cached, took %lums", loaded, skipped, millis() - startTime);
}
