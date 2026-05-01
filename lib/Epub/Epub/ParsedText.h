#pragma once

#include <EpdFontFamily.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "blocks/BlockStyle.h"
#include "blocks/TextBlock.h"

class GfxRenderer;

class ParsedText {
  // Word storage: a single contiguous byte arena plus per-word offset/length
  // arrays, replacing the previous std::vector<std::string> words. This
  // eliminates the per-word heap allocation that the HTML parser was issuing
  // for every flushed token, which was the root cause of fragmentation-driven
  // OOM aborts on long CJK chapters (operator new -> bad_alloc -> abort).
  //
  // The arena grows in amortised 2x doublings (one heap allocation per growth
  // round), so accumulating N words costs O(log N) allocations instead of N.
  // Modified words (hyphenation split, paragraph indent prepend) are re-emitted
  // at the end of the arena; the old bytes become inert until the ParsedText
  // is destroyed at the end of the paragraph.
  std::vector<char> wordArena;
  std::vector<uint32_t> wordOffsets;  // start offset into wordArena
  std::vector<uint16_t> wordLengths;  // byte length (≤ MAX_WORD_SIZE + a few)
  std::vector<EpdFontFamily::Style> wordStyles;
  std::vector<bool> wordContinues;  // true = word attaches to previous (no space before it)
  BlockStyle blockStyle;
  bool firstLineIndent;
  bool hyphenationEnabled;
  bool extraParagraphSpacing;

  // View into the arena for word `i`. Valid until the next mutation of
  // wordArena (since vector growth may move the buffer).
  std::string_view wordView(size_t i) const { return {wordArena.data() + wordOffsets[i], wordLengths[i]}; }
  // Append `word` to the arena and add a new entry to all parallel vectors.
  void pushWord(std::string_view word, EpdFontFamily::Style style, bool continues);
  // Drop the first `count` words from offset/length/style/continues vectors.
  // Arena bytes are not reclaimed mid-life; they are released when the
  // ParsedText is destroyed at the end of the paragraph.
  void eraseFront(size_t count);

  void applyParagraphIndent();
  std::vector<size_t> computeLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth,
                                        std::vector<uint16_t>& wordWidths, std::vector<bool>& continuesVec);
  std::vector<size_t> computeHyphenatedLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth,
                                                  std::vector<uint16_t>& wordWidths, std::vector<bool>& continuesVec);
  bool hyphenateWordAtIndex(size_t wordIndex, int availableWidth, const GfxRenderer& renderer, int fontId,
                            std::vector<uint16_t>& wordWidths, bool allowFallbackBreaks);
  void extractLine(size_t breakIndex, int pageWidth, const std::vector<uint16_t>& wordWidths,
                   const std::vector<bool>& continuesVec, const std::vector<size_t>& lineBreakIndices,
                   const std::function<void(std::shared_ptr<TextBlock>)>& processLine, const GfxRenderer& renderer,
                   int fontId);
  std::vector<uint16_t> calculateWordWidths(const GfxRenderer& renderer, int fontId);

 public:
  explicit ParsedText(const bool hyphenationEnabled = false, const BlockStyle& blockStyle = BlockStyle(),
                      const bool firstLineIndent = false, const bool extraParagraphSpacing = false)
      : blockStyle(blockStyle),
        firstLineIndent(firstLineIndent),
        hyphenationEnabled(hyphenationEnabled),
        extraParagraphSpacing(extraParagraphSpacing) {}
  ~ParsedText() = default;

  // string_view, not std::string by value: avoids the per-call heap allocation
  // that previously fired from every parser flushPartWordBuffer() call.
  void addWord(std::string_view word, EpdFontFamily::Style fontStyle, bool underline = false,
               bool attachToPrevious = false);
  void setBlockStyle(const BlockStyle& blockStyle) { this->blockStyle = blockStyle; }
  BlockStyle& getBlockStyle() { return blockStyle; }
  size_t size() const { return wordOffsets.size(); }
  bool isEmpty() const { return wordOffsets.empty(); }
  void layoutAndExtractLines(const GfxRenderer& renderer, int fontId, uint16_t viewportWidth,
                             const std::function<void(std::shared_ptr<TextBlock>)>& processLine,
                             bool includeLastLine = true);
};
