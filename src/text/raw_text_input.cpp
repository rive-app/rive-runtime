#ifdef WITH_RIVE_TEXT
#include "rive/text/raw_text_input.hpp"
#include "rive/text_engine.hpp"
#include "rive/factory.hpp"
#include "rive/span.hpp"
#include <algorithm>

using namespace rive;

static const Unichar zeroWidthSpace = 8203;
static const Unichar obscuringBullet = 8226;

RawTextInput::RawTextInput()
    : m_cursor(Cursor::atStart())
    , m_textRun({nullptr, 16.0f, -1.0f, 0.0f, 0, 0, 0})
    , m_cursorVisualPosition(CursorVisualPosition::missing())
{
  m_text.push_back(zeroWidthSpace);
  // The empty text is where undo stops, so the first edit can be undone.
  m_journal.push_back({m_cursor, m_cursor, {}});
}

void RawTextInput::draw(
    Factory* factory,
    Renderer* renderer,
    const Mat2D& worldTransform,
    RenderPaint* textPaint,
    RenderPaint* selectionPaint,
    RenderPaint* cursorPaint)
{
  if (m_overflow == TextOverflow::clipped && m_clipRenderPath)
  {
    renderer->save();
    renderer->clipPath(m_clipRenderPath.get());
  }

  if (m_cursor.hasSelection())
  {
    auto renderPath = m_selectionPath.renderPath(factory);
    renderer->drawPath(renderPath, selectionPaint);
  }

  auto renderPath = m_textPath.renderPath(factory);
  renderer->drawPath(renderPath, textPaint);

  auto cursorRenderPath = m_cursorPath.renderPath(factory);
  renderer->drawPath(cursorRenderPath, cursorPaint);

  if (m_overflow == TextOverflow::clipped && m_clipRenderPath)
  {
    renderer->restore();
  }
}

void RawTextInput::backspace(int32_t direction)
{
  Cursor startingCursor = m_cursor;
  if (!m_cursor.isCollapsed())
  {
    erase();
    captureJournalEntry(startingCursor);
    return;
  }
  m_idealCursorX = -1.0f;

  ensureShape();
  if (direction > 0)
  {
    // Delete forward: erase the full glyph cluster at cursor position.
    auto index = m_cursor.first().codePointIndex();
    if (index >= m_text.size() - 1)
    {
      return;
    }
    replaceText(index, clusterLength(index), {});
    auto position = CursorPosition(index);
    m_cursor = Cursor::collapsed(position);
  }
  else
  {
    // Backspace: erase the full glyph cluster before cursor position.
    auto index = textIndex(m_cursor.first());
    if (index == 0)
    {
      return;
    }
    uint32_t clusterStart = m_textRun.font == nullptr
                                ? index - 1
                                : m_shape.glyphLookup().glyphStart(index - 1);
    replaceText(clusterStart, clusterLength(clusterStart), {});
    auto position = CursorPosition(clusterStart);
    m_cursor = Cursor::collapsed(position);
  }
  captureJournalEntry(startingCursor);
}

// How many code points the cluster at index holds. One without a font to
// shape with, and never the sentinel, which a font with no glyph for it joins.
uint32_t RawTextInput::clusterLength(uint32_t index) const
{
  uint32_t count = m_textRun.font == nullptr
                       ? 1
                       : m_shape.glyphLookup().count(index);
  return std::min(count, (uint32_t)length() - index);
}

// A cursor set from outside can sit past the text, on the sentinel that ends
// it or beyond.
uint32_t RawTextInput::textIndex(CursorPosition position) const
{
  return std::min(position.codePointIndex(), (uint32_t)length());
}

void RawTextInput::erase()
{
  m_idealCursorX = -1.0f;
  if (m_cursor.isCollapsed())
  {
    return;
  }

  uint32_t first = textIndex(m_cursor.first());
  uint32_t last = textIndex(m_cursor.last());
  replaceText(first, last - first, {});
  auto position = CursorPosition(first);
  m_cursor = Cursor::collapsed(position);
}

bool RawTextInput::insert(Unichar codePoint)
{
  return insertCodePoints(Span<const Unichar>(&codePoint, 1));
}

bool RawTextInput::insert(const std::string& text)
{
  std::vector<Unichar> codePoints;
  const uint8_t* ptr = (const uint8_t*)text.data();
  const uint8_t* end = ptr + text.size();
  while (ptr < end && *ptr)
  {
    codePoints.push_back(UTF::NextUTF8(&ptr, end));
  }
  return insertCodePoints(codePoints);
}

bool RawTextInput::insertCodePoints(Span<const Unichar> codePoints)
{
  size_t count = fittingCount(codePoints);
  if (count == 0 && !codePoints.empty())
  {
    // Nothing fits, so the text, selection and undo history stay as they
    // are.
    return false;
  }

  Cursor startingCursor = m_cursor;
  erase();

  assert(m_cursor.isCollapsed());

  uint32_t codePointIndex = textIndex(m_cursor.start());
  replaceText(codePointIndex, 0, Span<const Unichar>(codePoints.data(), count));
  auto position = CursorPosition(codePointIndex + (uint32_t)count);
  m_cursor = Cursor::collapsed(position);
  captureJournalEntry(startingCursor);
  return true;
}

// How many of codePoints, from the front, can replace the selection without
// going over maxCharacters or maxLines.
size_t RawTextInput::fittingCount(Span<const Unichar> codePoints)
{
  size_t count = codePoints.size();
  if (count != 0 && m_maxCharacters != 0)
  {
    size_t first = std::min(
        (size_t)m_cursor.first().codePointIndex(),
        length());
    size_t last = std::min((size_t)m_cursor.last().codePointIndex(), length());
    size_t kept = length() - (last - first);
    size_t room = m_maxCharacters > kept ? m_maxCharacters - kept : 0;
    count = std::min(count, room);
  }
  if (count != 0 && m_maxLines != 0 && m_textRun.font != nullptr &&
      lineCountWith(codePoints, count, true) > m_maxLines)
  {
    // Already over at the insertion point: nothing goes in, as with
    // maxCharacters.
    if (lineCountWith(codePoints, 0, true) > m_maxLines)
    {
      return 0;
    }
    // Wrapping isn't monotonic in the prefix: one ending mid-word joins the
    // word after the selection and can wrap onto more lines than a longer one
    // ending in a space. Without the rest of the selection's paragraph it is,
    // as appending only ever adds lines, and putting that rest back can't
    // take any away. The paragraphs after it don't join, so they stay in and
    // keep the bound tight. So the longest prefix that fits without the rest
    // bounds the answer; count down from there to the first that fits with
    // it.
    size_t low = 0;
    size_t high = count - 1;
    while (low < high)
    {
      size_t middle = (low + high + 1) / 2;
      if (lineCountWith(codePoints, middle, false) <= m_maxLines)
      {
        low = middle;
      }
      else
      {
        high = middle - 1;
      }
    }
    // The count down is about as long as the rest of the paragraph is wide,
    // and every step shapes the whole text. Past a budget, settle for a
    // binary search on the real count: what it finds fits, it just isn't
    // always the longest prefix that does.
    const size_t countdownBudget = 64;
    size_t steps = 0;
    while (low > 0 && lineCountWith(codePoints, low, true) > m_maxLines)
    {
      low--;
      if (++steps == countdownBudget)
      {
        size_t fits = 0;
        while (fits < low)
        {
          size_t middle = (fits + low + 1) / 2;
          if (lineCountWith(codePoints, middle, true) <= m_maxLines)
          {
            fits = middle;
          }
          else
          {
            low = middle - 1;
          }
        }
        low = fits;
        break;
      }
    }
    count = low;
  }
  return count;
}

// The number of displayed lines the text would wrap to with the selection
// replaced by the first count code points, and with the rest of the
// selection's paragraph (the text after it, up to its line break) dropped
// unless withRestOfParagraph.
uint32_t RawTextInput::lineCountWith(
    Span<const Unichar> codePoints,
    size_t count,
    bool withRestOfParagraph)
{
  size_t first = std::min((size_t)m_cursor.first().codePointIndex(), length());
  size_t last = std::min((size_t)m_cursor.last().codePointIndex(), length());

  std::vector<Unichar> candidate;
  candidate.reserve(m_text.size() - (last - first) + count);
  candidate.insert(candidate.end(), m_text.begin(), m_text.begin() + first);
  candidate.insert(
      candidate.end(),
      codePoints.begin(),
      codePoints.begin() + count);
  size_t after = last;
  if (!withRestOfParagraph)
  {
    while (after < length() && m_text[after] != '\n' && m_text[after] != '\r' &&
           m_text[after] != 0x2029)
    {
      after++;
    }
  }
  // Ends with the sentinel either way.
  candidate.insert(candidate.end(), m_text.begin() + after, m_text.end());
  if (flagged(Flags::obscured))
  {
    // Count what would be displayed, keeping the trailing sentinel.
    std::fill(candidate.begin(), candidate.end() - 1, obscuringBullet);
  }

  if (!m_measuringShape)
  {
    m_measuringShape = std::make_unique<FullyShapedText>();
  }
#ifdef TESTING
  limitShapeCount++;
#endif

  // Wrap where the text will display. A hugging layout narrows maxWidth to
  // the text's current width, so wrapping there would push any line that
  // grows wider onto a new one: there the width measure() was last offered
  // is the room the text can grow into.
  float wrapWidth = flagged(Flags::hugsWidth) && flagged(Flags::measured)
                        ? m_lastMeasureMaxWidth
                        : m_maxWidth;

  TextRun run = m_textRun;
  run.unicharCount = (uint32_t)candidate.size();
  m_measuringShape->shape(
      candidate,
      Span<TextRun>(&run, 1),
      m_sizing,
      wrapWidth,
      m_maxHeight,
      m_align,
      m_wrap,
      m_origin,
      m_overflow,
      m_paragraphSpacing,
      m_alignWidth,
      m_verticalAlign,
      m_alignHeight);

  // Every line the breaker produced, which lineCount() can fall short of
  // when overflow stops it early.
  uint32_t lines = 0;
  for (const auto& paragraphLines : m_measuringShape->paragraphLines())
  {
    lines += (uint32_t)paragraphLines.size();
  }
  return lines;
}

void RawTextInput::selectionCornerRadius(float value)
{
  if (m_selectionCornerRadius == value)
  {
    return;
  }
  m_selectionCornerRadius = value;
  flag(Flags::selectionDirty);
}

void RawTextInput::fontSize(float value)
{
  if (m_textRun.size == value)
  {
    return;
  }
  m_textRun.size = value;
  reshapeAll();
}

void RawTextInput::maxWidth(float value)
{
  if (m_maxWidth == value)
  {
    return;
  }
  m_maxWidth = value;
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}

void RawTextInput::maxHeight(float value)
{
  if (m_maxHeight == value)
  {
    return;
  }
  m_maxHeight = value;
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}

void RawTextInput::sizing(TextSizing value)
{
  if (m_sizing == value)
  {
    return;
  }
  m_sizing = value;
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}

void RawTextInput::overflow(TextOverflow value)
{
  if (m_overflow == value)
  {
    return;
  }
  m_overflow = value;
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}

void RawTextInput::align(TextAlign value)
{
  if (m_align == value)
  {
    return;
  }
  m_align = value;
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}

void RawTextInput::alignWidth(float value)
{
  if (m_alignWidth == value)
  {
    return;
  }
  m_alignWidth = value;
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}

void RawTextInput::verticalAlign(VerticalTextAlign value)
{
  if (m_verticalAlign == value)
  {
    return;
  }
  m_verticalAlign = value;
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}

void RawTextInput::alignHeight(float value)
{
  if (m_alignHeight == value)
  {
    return;
  }
  m_alignHeight = value;
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}

void RawTextInput::font(rcp<Font> value)
{
  if (m_textRun.font == value)
  {
    return;
  }
  m_textRun.font = value;
  reshapeAll();
}

void RawTextInput::language(TextLanguageId value)
{
  if (m_textRun.language == value)
  {
    return;
  }
  m_textRun.language = value;
  markShapeDirty();
}

void RawTextInput::markShapeDirty() { reshapeAll(); }

void RawTextInput::computeVisualPositionFromCursor()
{
  m_cursorVisualPosition = cursorVisualPosition(m_cursor.end());
}

// The text handed to the shaper: the real text, or one bullet per code point
// when obscured. The trailing zero width space sentinel stays so cursor
// positions keep a one to one index mapping with m_text.
std::vector<Unichar>& RawTextInput::shapeableText()
{
  if (!flagged(Flags::obscured))
  {
    return m_text;
  }
  m_obscuredText.assign(m_text.size(), obscuringBullet);
  m_obscuredText.back() = zeroWidthSpace;
  return m_obscuredText;
}

void RawTextInput::textEdited(uint32_t start, uint32_t removed, uint32_t added)
{
  flag(
      Flags::paragraphsDirty | Flags::shapeDirty | Flags::measureDirty |
      Flags::selectionDirty);
  if (m_reshapeAll)
  {
    return;
  }
  uint32_t end = start + removed;
  if (!m_editPending)
  {
    m_editPending = true;
    m_editStart = start;
    m_editOldEnd = end;
    m_editNewEnd = start + added;
    return;
  }
  // Past the pending range, text still maps one to one onto the shaped text.
  uint32_t unionEnd = std::max(m_editNewEnd, end);
  m_editOldEnd += unionEnd - m_editNewEnd;
  m_editNewEnd = unionEnd - removed + added;
  m_editStart = std::min(m_editStart, start);
}

void RawTextInput::reshapeAll()
{
  flag(
      Flags::paragraphsDirty | Flags::shapeDirty | Flags::measureDirty |
      Flags::selectionDirty);
  m_reshapeAll = true;
}

void RawTextInput::ensureParagraphs()
{
  if (unflag(Flags::paragraphsDirty))
  {
    m_textRun.unicharCount = (uint32_t)m_text.size();
    bool reshaped = !m_reshapeAll && m_editPending &&
                    !flagged(Flags::obscured) &&
                    m_shape.reshapeParagraphs(
                        m_text,
                        Span<TextRun>(&m_textRun, 1),
                        m_editStart,
                        m_editOldEnd,
                        m_editNewEnd);
    if (!reshaped)
    {
      m_shape.shapeParagraphs(shapeableText(), Span<TextRun>(&m_textRun, 1));
    }
#ifdef TESTING
    (reshaped ? reshapeCount : fullShapeCount)++;
#endif
    m_reshapeAll = false;
    m_editPending = false;
    flag(Flags::shapeDirty);
  }
}

void RawTextInput::ensureShape()
{
  // There is no shaping without a font.
  if (m_textRun.font == nullptr)
  {
    return;
  }
  ensureParagraphs();
  if (unflag(Flags::shapeDirty))
  {
    m_shape.layout(
        m_sizing,
        m_maxWidth,
        m_maxHeight,
        m_align,
        m_wrap,
        m_origin,
        m_overflow,
        m_paragraphSpacing,
        m_alignWidth,
        m_verticalAlign,
        m_alignHeight);
    linesChanged(m_shape.lineChange());
    flag(Flags::shaped);
  }
}

RawTextInput::Flags RawTextInput::update(Factory* factory)
{
  Flags updated = Flags::none;
  if (m_textRun.font == nullptr)
  {
    return updated;
  }
  bool updateTextPath = false;
  ensureShape();
  // Whether shaped just now or by an edit since the last update -- a select
  // or a caret move shapes the text it needs at once.
  if (unflag(Flags::shaped))
  {
    updated |= Flags::shapeDirty;
    updateTextPath = true;
  }
  if (unflag(Flags::selectionDirty))
  {
    updated |= Flags::selectionDirty;
    if (flagged(Flags::separateSelectionText))
    {
      updateTextPath = true;
    }

    m_cursor.resolveLinePositions(m_shape);

    computeVisualPositionFromCursor();

    // Update the selection contours.
    m_selectionRects.clear();
    m_cursor.selectionRects(m_selectionRects, m_shape);
    m_selectionPath.update(m_selectionRects, m_selectionCornerRadius);

    m_cursorPath.rewind();
    float caretWidth = 1.0f;
    RawPath rect;
    rect.moveTo(m_cursorVisualPosition.x(), m_cursorVisualPosition.top());
    rect.lineTo(
        m_cursorVisualPosition.x() + caretWidth,
        m_cursorVisualPosition.top());
    rect.lineTo(
        m_cursorVisualPosition.x() + caretWidth,
        m_cursorVisualPosition.bottom());
    rect.lineTo(m_cursorVisualPosition.x(), m_cursorVisualPosition.bottom());
    rect.close();

    m_cursorPath.addPathClockwise(rect);
  }

  if (updateTextPath)
  {
    buildTextPaths(factory);
  }
  return updated;
}

bool RawTextInput::visibleRange(float minY, float maxY, Factory* factory)
{
  m_visibleMinY = minY;
  m_visibleMaxY = maxY;
  bool wantAll = minY > maxY;
  bool builtAll = m_builtMinY > m_builtMaxY;
  bool covered = wantAll
                     ? builtAll
                     : !builtAll && minY >= m_builtMinY && maxY <= m_builtMaxY;
  // A pending shape rebuilds the paths in update() with the new range.
  if (covered || m_textRun.font == nullptr || flagged(Flags::shapeDirty))
  {
    return false;
  }
  buildTextPaths(factory);
  return true;
}

// Keeps the outlines of the lines the change left in place.
void RawTextInput::linesChanged(const FullyShapedText::LineChange& change)
{
  size_t keptEnd = m_linePathsFirst + m_linePaths.size();
  if (change.all)
  {
    forgetLinePaths(0, m_linePaths.size());
  }
  else if (change.first + change.removed <= m_linePathsFirst)
  {
    m_linePathsFirst += change.added;
    m_linePathsFirst -= change.removed;
  }
  else if (change.first < keptEnd)
  {
    size_t from = std::max<size_t>(change.first, m_linePathsFirst);
    size_t to = std::min<size_t>(change.first + change.removed, keptEnd);
    auto at = m_linePaths.begin() + (from - m_linePathsFirst);
    m_linePaths.insert(
        m_linePaths.erase(at, at + (to - from)),
        change.added,
        LinePath());
    m_linePathsFirst = std::min<size_t>(m_linePathsFirst, change.first);
  }
}

// Has kept outlines [first, end) built again when their lines show.
void RawTextInput::forgetLinePaths(size_t first, size_t end)
{
  for (size_t i = first; i < end; i++)
  {
    m_linePaths[i].y = std::numeric_limits<float>::quiet_NaN();
  }
}

// Has m_linePaths hold ordered lines [first, end), keeping what it has of
// them while it slides. The lines let go lend their storage to those coming in.
void RawTextInput::keepLinePaths(size_t first, size_t end)
{
  auto kept = m_linePaths.begin();
  size_t size = m_linePaths.size();
  if (first >= m_linePathsFirst + size || m_linePathsFirst >= first + size)
  {
    forgetLinePaths(0, size);
  }
  else if (first > m_linePathsFirst)
  {
    size_t gone = first - m_linePathsFirst;
    std::rotate(kept, kept + gone, kept + size);
    forgetLinePaths(size - gone, size);
  }
  else
  {
    size_t added = m_linePathsFirst - first;
    std::rotate(kept, kept + (size - added), kept + size);
    forgetLinePaths(0, added);
  }
  m_linePaths.resize(end - first);
  m_linePathsFirst = first;
}

static bool endsAbove(const OrderedLine& line, float y)
{
  return line.bottom() < y;
}

static bool startsBelow(float y, const OrderedLine& line)
{
  return y < line.top();
}

void RawTextInput::addLine(
    const OrderedLine& line,
    ShapePaintPath& textPath,
    ShapePaintPath* selectedTextPath)
{
#ifdef TESTING
  lineBuildCount++;
#endif
  float x = line.startX();
  for (auto glyphItr : line)
  {
    const GlyphRun* run = std::get<0>(glyphItr);
    size_t glyphIndex = std::get<1>(glyphItr);
    const Vec2D& offset = run->offsets[glyphIndex];
    RawPath rawPath = run->font->getPath(run->glyphs[glyphIndex]);
    rawPath.transformInPlace(Mat2D(
        run->size,
        0.0f,
        0.0f,
        run->size,
        x + offset.x,
        line.y() + offset.y));
    x += line.advance(*run, (uint32_t)glyphIndex);

    // m_path contains everything, so inner feather bounds can work.
    if (selectedTextPath != nullptr &&
        m_cursor.contains(run->textIndices[glyphIndex]))
    {
      selectedTextPath->addPathClockwise(rawPath);
    }
    else
    {
      textPath.addPathClockwise(rawPath);
    }
  }
}

void RawTextInput::buildTextPaths(Factory* factory)
{
  bool wantSeparate = flagged(Flags::separateSelectionText);
  m_textPath.rewind();
  m_selectedTextPath.rewind();

  // One range's height beyond each edge lets small scrolls reuse the paths.
  bool cull = m_visibleMinY <= m_visibleMaxY;
  float cullMinY = 0.0f;
  float cullMaxY = 0.0f;
  if (cull)
  {
    float height = m_visibleMaxY - m_visibleMinY;
    cullMinY = m_visibleMinY - height;
    cullMaxY = m_visibleMaxY + height;
  }
  m_builtMinY = cull ? cullMinY : 1.0f;
  m_builtMaxY = cull ? cullMaxY : 0.0f;
  if (!m_shape.hasValidBounds())
  {
    m_clipRenderPath = nullptr;
    m_linePaths.clear();
    return;
  }

  // Build the clip path if we want it.
  if (m_overflow == TextOverflow::clipped)
  {
    if (m_clipRenderPath == nullptr)
    {
      m_clipRenderPath = factory->makeEmptyRenderPath();
    }
    else
    {
      m_clipRenderPath->rewind();
    }

    const AABB& bounds = m_shape.bounds();
    m_clipRenderPath
        ->addRect(bounds.minX, bounds.minY, bounds.width(), bounds.height());
  }
  else
  {
    m_clipRenderPath = nullptr;
  }

  const std::vector<OrderedLine>& lines = m_shape.orderedLines();
  size_t first = 0;
  size_t end = lines.size();
  // Lines only go down while paragraphs don't pull back over each other.
  bool sorted = m_paragraphSpacing >= 0.0f;
  if (cull && sorted)
  {
    first = std::lower_bound(lines.begin(), lines.end(), cullMinY, endsAbove) -
            lines.begin();
    end = std::upper_bound(
              lines.begin() + first,
              lines.end(),
              cullMaxY,
              startsBelow) -
          lines.begin();
  }

  // Unsorted lines would keep every outline a scroll has passed.
  bool keep = cull && sorted && !wantSeparate;
  if (keep)
  {
    keepLinePaths(first, end);
  }
  else
  {
    m_linePaths.clear();
  }
  for (size_t i = first; i < end; i++)
  {
    const OrderedLine& line = lines[i];
    if (!keep)
    {
      if (!cull || (line.top() <= cullMaxY && line.bottom() >= cullMinY))
      {
        addLine(line, m_textPath, wantSeparate ? &m_selectedTextPath : nullptr);
      }
      continue;
    }
    LinePath& kept = m_linePaths[i - first];
    if (kept.y != line.y())
    {
      kept.path.rewind();
      addLine(line, kept.path, nullptr);
      kept.y = line.y();
    }
    // The outline was pruned as it was built, so it skips the path's prune.
    m_textPath.mutableRawPath()->addPath(*kept.path.rawPath());
  }
}

AABB RawTextInput::bounds() const { return m_shape.bounds(); }

void RawTextInput::paragraphSpacing(float value)
{
  if (m_paragraphSpacing == value)
  {
    return;
  }
  m_paragraphSpacing = value;
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}

void RawTextInput::cursor(Cursor value)
{
  if (m_cursor == value)
  {
    return;
  }
  m_cursor = value;
  flag(Flags::selectionDirty);
}

void RawTextInput::selectWord()
{
  CursorPosition searchPosition = m_cursor.start();
  Delineator classification = classify(searchPosition);
  // If it's not a word, make sure point before it is not a word too. This
  // accomodates for when trying to select a word by clicking on the
  // right-most edge of the word.
  if ((classification & Delineator::word) == Delineator::unknown)
  {
    CursorPosition previousPosition = searchPosition - 1;
    Delineator previousClassification = classify(previousPosition);
    if ((previousClassification & Delineator::word) != Delineator::unknown)
    {
      searchPosition = previousPosition;
      classification = previousClassification;
    }
  }

  // Be more flexible on classification when search for a word (we don't want
  // an exact match on upper/lower/underscore, we want any of the three).
  if ((classification & Delineator::word) != Delineator::unknown)
  {
    classification = Delineator::word;
  }
  CursorPosition start = findPosition(
      ~(uint8_t)classification,
      searchPosition,
      -1);
  CursorPosition end = findPosition(
      ~(uint8_t)classification,
      searchPosition,
      1);

  // The sentinel that ends the text is no part of a word.
  if (end.codePointIndex() < length())
  {
    end += 1;
  }

  m_cursor = Cursor(start, end);
  flag(Flags::selectionDirty);
}

CursorPosition RawTextInput::positionAtIndex(uint32_t codePointIndex)
{
  // There is no shaping without a font.
  if (m_textRun.font == nullptr)
  {
    return CursorPosition(std::min(codePointIndex, (uint32_t)length()));
  }
  ensureShape();
  return CursorPosition::atIndex(codePointIndex, m_shape);
}

void RawTextInput::selectAll()
{
  m_idealCursorX = -1.0f;
  m_cursor = Cursor(positionAtIndex(0), positionAtIndex((uint32_t)length()));
  flag(Flags::selectionDirty);
}

void RawTextInput::moveCursorToEnd()
{
  m_idealCursorX = -1.0f;
  m_cursor = Cursor::collapsed(positionAtIndex((uint32_t)length()));
  flag(Flags::selectionDirty);
}

void RawTextInput::selectLine()
{
  ensureShape();
  m_idealCursorX = -1.0f;
  CursorPosition cursor = m_cursor.start();
  cursor.resolveLine(m_shape);
  const OrderedLine* line = orderedLine(cursor);
  if (line == nullptr)
  {
    return;
  }
  const auto& glyphLookup = m_shape.glyphLookup();
  CursorPosition start(
      cursor.lineIndex(),
      line->firstCodePointIndex(glyphLookup));
  CursorPosition end(cursor.lineIndex(), line->lastCodePointIndex(glyphLookup));
  m_cursor = Cursor(start, end);
  flag(Flags::selectionDirty);
}

void RawTextInput::clearSelection()
{
  cursor(Cursor::collapsed(m_cursor.end()));
}

const OrderedLine* RawTextInput::orderedLine(CursorPosition position) const
{
  const std::vector<OrderedLine>& orderedLines = m_shape.orderedLines();
  if (position.lineIndex() >= orderedLines.size())
  {
    return nullptr;
  }
  return &orderedLines[position.lineIndex()];
}

void RawTextInput::cursorHorizontal(
    int32_t offset,
    CursorBoundary boundary,
    bool select)
{
  ensureShape();
  m_idealCursorX = -1.0f;
  // Collapsing a selection with a leftward move works off its leading edge,
  // so the cursor ends up before the selection's first character instead of
  // before its last. Clamping keeps a selection that already starts at the
  // beginning there.
  CursorPosition basePosition = !select && offset < 0 && m_cursor.hasSelection()
                                    ? m_cursor.first()
                                    : m_cursor.end();

  CursorPosition position = basePosition;
  switch (boundary)
  {
    case CursorBoundary::character:
    {
      const auto& glyphLookup = m_shape.glyphLookup();
      uint32_t nextIndex = basePosition.codePointIndex(offset);
      // Skip over interior codepoints of multi-codepoint glyphs.
      if (offset > 0)
      {
        while (nextIndex < (uint32_t)(m_text.size() - 1) &&
               !glyphLookup.isGlyphBoundary(nextIndex))
        {
          nextIndex++;
        }
      }
      else
      {
        while (nextIndex > 0 && !glyphLookup.isGlyphBoundary(nextIndex))
        {
          nextIndex--;
        }
      }
      position = positionAtIndex(nextIndex);
      break;
    }
    case CursorBoundary::line:
    {
      auto line = orderedLine(basePosition);
      if (line != nullptr)
      {
        uint32_t codePointIndex = offset < 0 ? line->firstCodePointIndex(
                                                   m_shape.glyphLookup())
                                             : line->lastCodePointIndex(
                                                   m_shape.glyphLookup());
        position = CursorPosition(basePosition.lineIndex(), codePointIndex);
      }
      break;
    }
    case CursorBoundary::word:
    case CursorBoundary::subWord:
    {
      Delineator classification = classify(position - (offset < 0 ? 1 : 0));

      switch (classification)
      {
        case Delineator::whitespace:
        case Delineator::underscore:
          classification = find(~classification, position, offset);
          break;
        default:
          break;
      }

      switch (classification)
      {
        case Delineator::symbol:
          classification = find(~classification, position, offset);
          break;
        case Delineator::lowercase:
          if (boundary == CursorBoundary::subWord)
          {
            Delineator nonLowerCase = find(
                ~Delineator::lowercase,
                position,
                offset);
            if (offset == -1 && nonLowerCase == Delineator::uppercase)
            {
              position = position - 1;
            }
          }
          else
          {
            find(
                ~(Delineator::lowercase | Delineator::uppercase |
                  Delineator::underscore),
                position,
                offset);
          }
          break;
        case Delineator::uppercase:
          if (boundary == CursorBoundary::subWord)
          {
            CursorPosition startPosition = position;
            Delineator nonUpper = find(
                ~Delineator::uppercase,
                position,
                offset);
            if (offset == 1 && nonUpper == Delineator::lowercase)
            {
              position = position - 1;
              if (position.codePointIndex() == startPosition.codePointIndex())
              {
                find(~Delineator::lowercase, position, offset);
              }
            }
          }
          else
          {
            find(
                ~(Delineator::lowercase | Delineator::uppercase |
                  Delineator::underscore),
                position,
                offset);
          }
          break;
        default:
          find(~classification, position, offset);
          break;
      }
      break;
    }
    default:
      break;
  }

  if (select)
  {
    m_cursor = Cursor(m_cursor.start(), position);
  }
  else
  {
    m_cursor = Cursor::collapsed(position);
  }

  flag(Flags::selectionDirty);
}

RawTextInput::Delineator RawTextInput::classify(Unichar codepoint)
{
  if (isWhiteSpace(codepoint))
  {
    return Delineator::whitespace;
  }
  if (codepoint == 95)
  {
    return Delineator::underscore;
  }
  if (codepoint < 48 || (codepoint >= 58 && codepoint <= 64) ||
      (codepoint >= 91 && codepoint <= 96) ||
      (codepoint >= 123 && codepoint <= 127))
  {
    return Delineator::symbol;
  }
  if (codepoint >= 65 && codepoint <= 90)
  {
    return Delineator::uppercase;
  }
  return Delineator::lowercase;
}

RawTextInput::Delineator RawTextInput::classify(CursorPosition position) const
{
  if (empty() || position.codePointIndex() >= m_text.size() - 1)
  {
    return Delineator::whitespace;
  }
  // Word boundaries follow what is displayed, so obscured text moves as one
  // run of bullets and reveals nothing.
  return classify(
      flagged(Flags::obscured) ? obscuringBullet
                               : m_text[position.codePointIndex()]);
}

RawTextInput::Delineator RawTextInput::find(
    uint8_t delineatorMask,
    CursorPosition& position,
    int32_t direction) const
{
  Delineator lastClassification = Delineator::unknown;
  while (true)
  {
    CursorPosition nextPosition = position + direction;
    // Past the end of the text there is nothing left to find.
    if (nextPosition.codePointIndex() == position.codePointIndex() ||
        nextPosition.codePointIndex() > length())
    {
      break;
    }

    position = nextPosition;
    if (((lastClassification = classify(
              nextPosition - (direction < 0 ? 1 : 0))) &
         delineatorMask) != 0)
    {
      break;
    }
  }
  return lastClassification;
}

CursorPosition RawTextInput::findPosition(
    uint8_t delineatorMask,
    const CursorPosition& position,
    int32_t direction) const
{
  CursorPosition result = position;
  while (true)
  {
    CursorPosition nextPosition = result + direction;

    if (nextPosition.codePointIndex() == result.codePointIndex() ||
        (size_t)nextPosition.codePointIndex() >= length())
    {
      break;
    }

    if ((classify(nextPosition) & delineatorMask) != 0)
    {
      break;
    }
    result = nextPosition;
  }
  return result;
}

void RawTextInput::cursorLeft(CursorBoundary boundary, bool select)
{
  cursorHorizontal(-1, boundary, select);
}

void RawTextInput::cursorRight(CursorBoundary boundary, bool select)
{
  cursorHorizontal(1, boundary, select);
}

void RawTextInput::cursorUp(bool select)
{
  ensureShape();
  if (m_idealCursorX == -1.0f)
  {
    m_idealCursorX = m_cursorVisualPosition.x();
  }
  uint32_t lineIndex = m_cursor.end().lineIndex();

  auto position = lineIndex == 0 ? CursorPosition::zero()
                                 : CursorPosition::fromLineX(
                                       m_cursor.end().lineIndex(-1),
                                       m_idealCursorX,
                                       m_shape);
  if (select)
  {
    m_cursor = Cursor(m_cursor.start(), position);
  }
  else
  {
    m_cursor = Cursor::collapsed(position);
  }
  flag(Flags::selectionDirty);
}

void RawTextInput::cursorDown(bool select)
{
  ensureShape();
  if (m_idealCursorX == -1.0f)
  {
    m_idealCursorX = m_cursorVisualPosition.x();
  }
  uint32_t nextLineIndex = m_cursor.end().lineIndex(1);
  auto position = m_shape.lineCount() != 0 && m_text.size() > 1 &&
                          nextLineIndex >= m_shape.lineCount()
                      ? CursorPosition(
                            (uint32_t)(m_shape.lineCount() - 1),
                            (uint32_t)(m_text.size() - 1))
                      : CursorPosition::fromLineX(
                            nextLineIndex,
                            m_idealCursorX,
                            m_shape);

  if (select)
  {
    m_cursor = Cursor(m_cursor.start(), position);
  }
  else
  {
    m_cursor = Cursor::collapsed(position);
  }
  flag(Flags::selectionDirty);
}

void RawTextInput::moveCursorTo(Vec2D translation, bool select)
{
  ensureShape();
  m_idealCursorX = -1.0f;
  auto position = CursorPosition::fromTranslation(translation, m_shape);

  if (select)
  {
    m_cursor = Cursor(m_cursor.start(), position);
  }
  else
  {
    m_cursor = Cursor::collapsed(position);
  }
  flag(Flags::selectionDirty);
}

static std::string encodeCodePoints(Span<const Unichar> codePoints)
{
  Unichar bits = 0;
  for (Unichar codePoint : codePoints)
  {
    bits |= codePoint;
  }
  if (bits < 0x80)
  {
    // Every code point is its own byte.
    return std::string(codePoints.begin(), codePoints.end());
  }
  std::string str(UTF::CountCodePointLength(codePoints), '\0');
  uint8_t* encoded = (uint8_t*)str.data();
  for (auto codePoint : codePoints)
  {
    encoded += UTF::Encode(encoded, codePoint);
  }
  return str;
}

std::string RawTextInput::text() const
{
  size_t size = m_text.size();
  if (size == 0)
  {
    return std::string();
  }

  return encodeCodePoints(Span<const Unichar>(m_text.data(), size - 1));
}

std::string RawTextInput::selectedText() const
{
  if (m_text.empty())
  {
    return std::string();
  }
  // Exclude the trailing sentinel code point from the selectable range.
  size_t textEnd = m_text.size() - 1;
  size_t first = std::min((size_t)m_cursor.first().codePointIndex(), textEnd);
  size_t last = std::min((size_t)m_cursor.last().codePointIndex(), textEnd);
  if (first >= last)
  {
    return std::string();
  }
  return encodeCodePoints(
      Span<const Unichar>(m_text.data() + first, last - first));
}

void RawTextInput::replaceText(
    uint32_t start,
    uint32_t count,
    Span<const Unichar> added)
{
  // An edit that changes nothing still leaves the cursor to resolve.
  if (count != 0 || !added.empty())
  {
    auto first = m_text.begin() + start;
    m_pendingEdits.push_back(
        {start,
         std::vector<Unichar>(first, first + count),
         std::vector<Unichar>(added.begin(), added.end())});
  }
  spliceText(start, count, added);
}

void RawTextInput::spliceText(
    uint32_t start,
    uint32_t count,
    Span<const Unichar> added)
{
  auto first = m_text.begin() + start;
  m_text.insert(m_text.erase(first, first + count), added.begin(), added.end());
  textEdited(start, count, (uint32_t)added.size());
}

void RawTextInput::setTextPrivate(const std::string& value)
{
  const uint8_t* ptr = (const uint8_t*)value.data();
  const uint8_t* end = ptr + value.size();
  std::vector<Unichar> text;
  text.reserve(m_text.size());
  while (ptr < end && *ptr)
  {
    Unichar codePoint = UTF::NextUTF8(&ptr, end);
    text.push_back(codePoint);
  }
  text.push_back(zeroWidthSpace);

  // Only the part between the common prefix and suffix changes.
  size_t oldSize = m_text.size();
  size_t newSize = text.size();
  size_t prefix = 0;
  size_t maxPrefix = std::min(oldSize, newSize);
  while (prefix < maxPrefix && m_text[prefix] == text[prefix])
  {
    prefix++;
  }
  size_t suffix = 0;
  size_t maxSuffix = maxPrefix - prefix;
  while (suffix < maxSuffix &&
         m_text[oldSize - 1 - suffix] == text[newSize - 1 - suffix])
  {
    suffix++;
  }
  if (prefix == oldSize && prefix == newSize)
  {
    return;
  }
  replaceText(
      (uint32_t)prefix,
      (uint32_t)(oldSize - prefix - suffix),
      Span<const Unichar>(text.data() + prefix, newSize - prefix - suffix));
}

void RawTextInput::text(std::string value)
{
  setTextPrivate(value);
  auto position = CursorPosition::zero();
  m_cursor = Cursor::collapsed(position);
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
  // Text set from outside (the file's own, a data bind) starts a new undo
  // history with it as the baseline: undo takes back the edits after it,
  // never the text itself.
  m_journal.clear();
  m_journal.push_back({m_cursor, m_cursor, {}});
  m_pendingEdits.clear();
  m_journalIndex = 0;
}

void RawTextInput::textPreserveCursor(std::string value)
{
  Cursor startingCursor = m_cursor;
  m_idealCursorX = -1.0f;
  setTextPrivate(value);
  uint32_t maxIndex = static_cast<uint32_t>(length());
  CursorPosition start(
      std::min(startingCursor.start().codePointIndex(), maxIndex));
  CursorPosition end(std::min(startingCursor.end().codePointIndex(), maxIndex));
  m_cursor = Cursor(start, end);
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
  captureJournalEntry(startingCursor);
}

size_t RawTextInput::length() const
{
  if (empty())
  {
    return 0;
  }
  return m_text.size() - 1;
}

bool RawTextInput::empty() const
{
  // note that we're empty at length 1 as we have our zeroWidthSpace
  return m_text.size() <= 1;
}

void RawTextInput::captureJournalEntry(Cursor cursor)
{
  if (m_journalIndex + 1 < m_journal.size())
  {
    m_journal.erase(m_journal.begin() + m_journalIndex + 1, m_journal.end());
  }
  m_journal.push_back({cursor, m_cursor, std::move(m_pendingEdits)});
  m_pendingEdits.clear();
  m_journalIndex = (uint32_t)m_journal.size() - 1;
}

void RawTextInput::revertEdits(const std::vector<TextEdit>& edits)
{
  for (auto edit = edits.rbegin(); edit != edits.rend(); ++edit)
  {
    spliceText(edit->start, (uint32_t)edit->inserted.size(), edit->removed);
  }
}

// Edits made without an entry go with the entry before them.
void RawTextInput::revertPendingEdits()
{
  revertEdits(m_pendingEdits);
  m_pendingEdits.clear();
}

void RawTextInput::undo()
{
  if (m_journalIndex == 0)
  {
    return;
  }
  revertPendingEdits();
  const JournalEntry& entry = m_journal[m_journalIndex];
  revertEdits(entry.edits);
  m_cursor = entry.cursorFrom;

  m_journalIndex -= 1;
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}
void RawTextInput::redo()
{
  if (m_journal.empty() || m_journalIndex + 1 >= m_journal.size())
  {
    return;
  }
  revertPendingEdits();
  const JournalEntry& entry = m_journal[m_journalIndex + 1];
  for (const TextEdit& edit : entry.edits)
  {
    spliceText(edit.start, (uint32_t)edit.removed.size(), edit.inserted);
  }
  m_cursor = entry.cursorTo;
  m_journalIndex += 1;
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}

#ifdef TESTING
size_t RawTextInput::journalCodePoints() const
{
  size_t count = 0;
  for (const JournalEntry& entry : m_journal)
  {
    for (const TextEdit& edit : entry.edits)
    {
      count += edit.removed.size() + edit.inserted.size();
    }
  }
  return count;
}
#endif

bool RawTextInput::flagged(RawTextInput::Flags mask) const
{
  return enums::any_flag_set(m_flags, mask);
}

bool RawTextInput::unflag(RawTextInput::Flags mask)
{
  if (enums::any_flag_set(m_flags, mask))
  {
    m_flags &= ~mask;
    return true;
  }

  return false;
}

void RawTextInput::flag(RawTextInput::Flags mask) { m_flags |= mask; }

bool RawTextInput::separateSelectionText() const
{
  return flagged(Flags::separateSelectionText);
}

void RawTextInput::separateSelectionText(bool value)
{
  if (value)
  {
    flag(Flags::separateSelectionText);
  }
  else
  {
    unflag(Flags::separateSelectionText);
  }
  flag(Flags::shapeDirty | Flags::measureDirty | Flags::selectionDirty);
}

bool RawTextInput::obscured() const { return flagged(Flags::obscured); }

bool RawTextInput::hugsWidth() const { return flagged(Flags::hugsWidth); }

void RawTextInput::hugsWidth(bool value)
{
  if (value)
  {
    flag(Flags::hugsWidth);
  }
  else
  {
    unflag(Flags::hugsWidth);
  }
}

static Cursor withoutLineIndices(const Cursor& cursor)
{
  return Cursor(
      CursorPosition(cursor.start().codePointIndex()),
      CursorPosition(cursor.end().codePointIndex()));
}

void RawTextInput::obscured(bool value)
{
  if (obscured() == value)
  {
    return;
  }
  if (value)
  {
    flag(Flags::obscured);
  }
  else
  {
    unflag(Flags::obscured);
  }
  // Masking can reflow lines, so resolved line indices are stale; keep only
  // the code point indices. The journal holds cursor snapshots too, or undo
  // would restore one resolved against the old layout.
  m_cursor = withoutLineIndices(m_cursor);
  for (JournalEntry& entry : m_journal)
  {
    entry.cursorFrom = withoutLineIndices(entry.cursorFrom);
    entry.cursorTo = withoutLineIndices(entry.cursorTo);
  }
  m_idealCursorX = -1.0f;
  reshapeAll();
}

AABB RawTextInput::measure(float maxWidth, float maxHeight)
{
  if (m_textRun.font == nullptr)
  {
    return AABB();
  }
  bool force = !flagged(Flags::measured) || m_lastMeasureMaxWidth != maxWidth ||
               m_lastMeasureMaxHeight != maxHeight;
  if (unflag(Flags::measureDirty) || force)
  {
    // Whole, not just the paragraphs: shape() is public.
    ensureShape();
    m_measuredBounds = m_shape.measure(
        m_sizing,
        maxWidth,
        maxHeight,
        m_align,
        m_wrap,
        m_origin,
        m_overflow,
        m_paragraphSpacing,
        m_alignWidth,
        m_verticalAlign,
        m_alignHeight);
    m_lastMeasureMaxWidth = maxWidth;
    m_lastMeasureMaxHeight = maxHeight;
    flag(Flags::measured);
#ifdef TESTING
    measureCount++;
#endif
  }
  return m_measuredBounds;
}

#endif
