#ifdef WITH_RIVE_TEXT
#include "rive/text/raw_text_input.hpp"
#include "rive/text_engine.hpp"
#include "rive/factory.hpp"
#include "rive/span.hpp"

using namespace rive;

static const Unichar zeroWidthSpace = 8203;

RawTextInput::RawTextInput() :
    m_cursor(Cursor::atStart()),
    m_textRun({nullptr, 16.0f, -1.0f, 0.0f, 0, 0, 0}),
    m_cursorVisualPosition(CursorVisualPosition::missing())
{
    m_text.push_back(zeroWidthSpace);
}

void RawTextInput::draw(Factory* factory,
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
    int32_t offset = direction > 0 ? 0 : -1;
    if (!m_cursor.isCollapsed())
    {
        erase();
        captureJournalEntry(startingCursor);
        return;
    }
    m_idealCursorX = -1.0f;

    auto index = m_cursor.first().codePointIndex(offset);

    if (index >= m_text.size() - 1)
    {
        return;
    }
    m_text.erase(m_text.begin() + index);
    auto position = CursorPosition(index);
    m_cursor = Cursor::collapsed(position);
    flag(Flags::shapeDirty | Flags::selectionDirty);
    captureJournalEntry(startingCursor);
}

void RawTextInput::erase()
{
    m_idealCursorX = -1.0f;
    if (m_cursor.isCollapsed())
    {
        return;
    }

    assert(m_cursor.first().codePointIndex() < length());
    assert(m_cursor.last().codePointIndex() <= length());

    m_text.erase(m_text.begin() + m_cursor.first().codePointIndex(),
                 m_text.begin() + m_cursor.last().codePointIndex());
    auto position = CursorPosition(m_cursor.first().codePointIndex());
    m_cursor = Cursor::collapsed(position);
    flag(Flags::shapeDirty | Flags::selectionDirty);
}

void RawTextInput::insert(Unichar codePoint)
{
    Cursor startingCursor = m_cursor;
    erase();

    assert(m_cursor.isCollapsed());

    m_text.insert(m_text.begin() + m_cursor.start().codePointIndex(),
                  codePoint);

    auto position = CursorPosition(m_cursor.first().codePointIndex(1));
    m_cursor = Cursor::collapsed(position);
    captureJournalEntry(startingCursor);
    flag(Flags::shapeDirty | Flags::selectionDirty);
}

void RawTextInput::insert(const std::string& text)
{
    Cursor startingCursor = m_cursor;
    erase();

    uint32_t codePointIndex = m_cursor.start().codePointIndex();

    const uint8_t* ptr = (const uint8_t*)text.c_str();
    while (*ptr)
    {
        Unichar codePoint = UTF::NextUTF8(&ptr);
        m_text.insert(m_text.begin() + codePointIndex, codePoint);
        codePointIndex++;
    }
    auto position = CursorPosition(codePointIndex);
    m_cursor = Cursor::collapsed(position);
    flag(Flags::shapeDirty | Flags::selectionDirty);
    captureJournalEntry(startingCursor);
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
    flag(Flags::shapeDirty | Flags::selectionDirty);
}

void RawTextInput::maxWidth(float value)
{
    if (m_maxWidth == value)
    {
        return;
    }
    m_maxWidth = value;
    flag(Flags::shapeDirty | Flags::selectionDirty);
}

void RawTextInput::maxHeight(float value)
{
    if (m_maxHeight == value)
    {
        return;
    }
    m_maxHeight = value;
    flag(Flags::shapeDirty | Flags::selectionDirty);
}

void RawTextInput::sizing(TextSizing value)
{
    if (m_sizing == value)
    {
        return;
    }
    m_sizing = value;
    flag(Flags::shapeDirty | Flags::selectionDirty);
}

void RawTextInput::overflow(TextOverflow value)
{
    if (m_overflow == value)
    {
        return;
    }
    m_overflow = value;
    flag(Flags::shapeDirty | Flags::selectionDirty);
}

void RawTextInput::font(rcp<Font> value)
{
    if (m_textRun.font == value)
    {
        return;
    }
    m_textRun.font = value;
    flag(Flags::shapeDirty | Flags::selectionDirty);
}

void RawTextInput::computeVisualPositionFromCursor()
{
    m_cursorVisualPosition = cursorVisualPosition(m_cursor.end());
}

bool RawTextInput::update(Factory* factory)
{
    bool updated = false;
    bool updateTextPath = false;
    if (unflag(Flags::shapeDirty))
    {
        updated = true;
        m_textRun.unicharCount = (uint32_t)m_text.size();
        m_shape.shape(m_text,
                      Span<TextRun>(&m_textRun, 1),
                      m_sizing,
                      m_maxWidth,
                      m_maxHeight,
                      m_align,
                      m_wrap,
                      m_origin,
                      m_overflow,
                      m_paragraphSpacing);
        updateTextPath = true;
    }
    if (unflag(Flags::selectionDirty))
    {
        updated = true;
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
        rect.lineTo(m_cursorVisualPosition.x() + caretWidth,
                    m_cursorVisualPosition.top());
        rect.lineTo(m_cursorVisualPosition.x() + caretWidth,
                    m_cursorVisualPosition.bottom());
        rect.lineTo(m_cursorVisualPosition.x(),
                    m_cursorVisualPosition.bottom());
        rect.close();

        m_cursorPath.addPathClockwise(rect);
    }

    if (updateTextPath)
    {
        buildTextPaths(factory);
    }
    return updated;
}

void RawTextInput::buildTextPaths(Factory* factory)
{
    bool wantSeparate = flagged(Flags::separateSelectionText);
    m_textPath.rewind();
    m_selectedTextPath.rewind();
    if (!m_shape.hasValidBounds())
    {
        m_clipRenderPath = nullptr;
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
        m_clipRenderPath->addRect(bounds.minX,
                                  bounds.minY,
                                  bounds.width(),
                                  bounds.height());
    }
    else
    {
        m_clipRenderPath = nullptr;
    }

    float y = 0;
    const SimpleArray<SimpleArray<GlyphLine>>& paragraphLines =
        m_shape.paragraphLines();
    const std::vector<OrderedLine>& orderedLines = m_shape.orderedLines();

    if (m_origin == TextOrigin::baseline && !paragraphLines.empty() &&
        !paragraphLines[0].empty())
    {
        y -= paragraphLines[0][0].baseline;
    }
    int32_t lineIndex = 0;
    // Only reason we don't iterate lines here is so we don't need to recompute
    // shape if we just change paragraph spacing, otherwise we could store the
    // computed y in each OrderedLine and just iterate those.
    for (const SimpleArray<GlyphLine>& lines : paragraphLines)
    {
        for (const GlyphLine& line : lines)
        {
            if (lineIndex >= orderedLines.size())
            {
                // We previously decided to clip at this number of lines (see
                // fully_shaped_text.cpp).
                break;
            }

            const OrderedLine& orderedLine = orderedLines[lineIndex];
            float x = line.startX;
            float renderY = y + line.baseline;
            for (auto glyphItr : orderedLine)
            {
                const GlyphRun* run = std::get<0>(glyphItr);
                size_t glyphIndex = std::get<1>(glyphItr);

                const Font* font = run->font.get();
                const Vec2D& offset = run->offsets[glyphIndex];

                GlyphID glyphId = run->glyphs[glyphIndex];
                float advance = run->advances[glyphIndex];

                RawPath rawPath = font->getPath(glyphId);

                rawPath.transformInPlace(Mat2D(run->size,
                                               0.0f,
                                               0.0f,
                                               run->size,
                                               x + offset.x,
                                               renderY + offset.y));

                x += advance;

                // m_path contains everything, so inner feather bounds can work.
                if (wantSeparate &&
                    m_cursor.contains(run->textIndices[glyphIndex]))
                {
                    m_selectedTextPath.addPathClockwise(rawPath);
                }
                else
                {
                    m_textPath.addPathClockwise(rawPath);
                }

                // assert(run->styleId < m_styles.size());
                // RenderStyle* style = &m_styles[run->styleId];
                // assert(style != nullptr);
                // path.addTo(style->path.get());

                // if (style->isEmpty)
                // {
                //     // This was the first path added to the style, so let's
                //     mark
                //     // it in our draw list.
                //     style->isEmpty = false;

                //     m_renderStyles.push_back(style);
                // }
            }
            lineIndex++;
        }
        if (!lines.empty())
        {
            y += lines.back().bottom;
        }
        y += m_paragraphSpacing;
    }
}

AABB RawTextInput::bounds() { return m_shape.bounds(); }

void RawTextInput::paragraphSpacing(float value)
{
    if (m_paragraphSpacing == value)
    {
        return;
    }
    m_paragraphSpacing = value;
    flag(Flags::shapeDirty | Flags::selectionDirty);
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
    CursorPosition start =
        findPosition(~(uint8_t)classification, searchPosition, -1);
    CursorPosition end =
        findPosition(~(uint8_t)classification, searchPosition, 1);

    end += 1;

    m_cursor = Cursor(start, end);
    flag(Flags::selectionDirty);
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

void RawTextInput::cursorHorizontal(int32_t offset,
                                    CursorBoundary boundary,
                                    bool select)
{
    m_idealCursorX = -1.0f;
    CursorPosition end = m_cursor.end();

    CursorPosition position = end;
    switch (boundary)
    {
        case CursorBoundary::character:
            position =
                CursorPosition::atIndex(end.codePointIndex(offset), m_shape);
            break;
        case CursorBoundary::line:
        {
            auto line = orderedLine(end);
            if (line != nullptr)
            {
                uint32_t codePointIndex =
                    offset < 0
                        ? line->firstCodePointIndex(m_shape.glyphLookup())
                        : line->lastCodePointIndex(m_shape.glyphLookup());
                position = CursorPosition(end.lineIndex(), codePointIndex);
            }
            break;
        }
        case CursorBoundary::word:
        case CursorBoundary::subWord:
        {
            Delineator classification =
                classify(position - (offset < 0 ? 1 : 0));

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
                        Delineator nonLowerCase =
                            find(~Delineator::lowercase, position, offset);
                        if (offset == -1 &&
                            nonLowerCase == Delineator::uppercase)
                        {
                            position = position - 1;
                        }
                    }
                    else
                    {
                        find(~(Delineator::lowercase | Delineator::uppercase |
                               Delineator::underscore),
                             position,
                             offset);
                    }
                    break;
                case Delineator::uppercase:
                    if (boundary == CursorBoundary::subWord)
                    {
                        CursorPosition startPosition = position;
                        Delineator nonUpper =
                            find(~Delineator::uppercase, position, offset);
                        if (offset == 1 && nonUpper == Delineator::lowercase)
                        {
                            position = position - 1;
                            if (position.codePointIndex() ==
                                startPosition.codePointIndex())
                            {
                                find(~Delineator::lowercase, position, offset);
                            }
                        }
                    }
                    else
                    {
                        find(~(Delineator::lowercase | Delineator::uppercase |
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
    return classify(m_text[position.codePointIndex()]);
}

RawTextInput::Delineator RawTextInput::find(uint8_t delineatorMask,
                                            CursorPosition& position,
                                            int32_t direction) const
{
    Delineator lastClassification = Delineator::unknown;
    while (true)
    {
        CursorPosition nextPosition = position + direction;
        if (nextPosition.codePointIndex() == position.codePointIndex())
        {
            break;
        }

        position = nextPosition;
        if (((lastClassification =
                  classify(nextPosition - (direction < 0 ? 1 : 0))) &
             delineatorMask) != 0)
        {
            break;
        }
    }
    return lastClassification;
}

CursorPosition RawTextInput::findPosition(uint8_t delineatorMask,
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
    if (m_idealCursorX == -1.0f)
    {
        m_idealCursorX = m_cursorVisualPosition.x();
    }
    uint32_t lineIndex = m_cursor.end().lineIndex();

    auto position =
        lineIndex == 0 ? CursorPosition::zero()
                       : CursorPosition::fromLineX(m_cursor.end().lineIndex(-1),
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
    if (m_idealCursorX == -1.0f)
    {
        m_idealCursorX = m_cursorVisualPosition.x();
    }
    uint32_t nextLineIndex = m_cursor.end().lineIndex(1);
    auto position =
        m_shape.lineCount() != 0 && m_text.size() > 1 &&
                nextLineIndex >= m_shape.lineCount()
            ? CursorPosition((uint32_t)(m_shape.lineCount() - 1),
                             (uint32_t)(m_text.size() - 1))
            : CursorPosition::fromLineX(nextLineIndex, m_idealCursorX, m_shape);

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

std::string RawTextInput::text() const
{
    size_t size = m_text.size();
    if (size == 0)
    {
        return std::string();
    }

    auto codePoints = Span<const Unichar>(m_text.data(), size - 1);

    std::vector<uint8_t> buffer(UTF::CountCodePointLength(codePoints));
    uint8_t* encoded = buffer.data();
    for (auto codePoint : codePoints)
    {
        encoded += UTF::Encode(encoded, codePoint);
    }

    std::string str;
    std::move(buffer.begin(), buffer.end(), std::back_inserter(str));
    return str;
}

void RawTextInput::setTextPrivate(std::string value)
{
    const uint8_t* ptr = (const uint8_t*)value.c_str();
    m_text.clear();
    while (*ptr)
    {
        Unichar codePoint = UTF::NextUTF8(&ptr);
        m_text.push_back(codePoint);
    }
    m_text.push_back(zeroWidthSpace);
}

void RawTextInput::text(std::string value)
{
    Cursor startingCursor = m_cursor;
    setTextPrivate(value);
    auto position = CursorPosition::zero();
    m_cursor = Cursor::collapsed(position);
    flag(Flags::shapeDirty | Flags::selectionDirty);
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
        m_journal.erase(m_journal.begin() + m_journalIndex + 1,
                        m_journal.end());
    }
    m_journal.push_back({cursor, m_cursor, text()});
    m_journalIndex = (uint32_t)m_journal.size() - 1;
}

void RawTextInput::undo()
{
    if (m_journalIndex == 0)
    {
        return;
    }
    const JournalEntry& entryFrom = m_journal[m_journalIndex];
    const JournalEntry& entryTo = m_journal[m_journalIndex - 1];
    setTextPrivate(entryTo.text);
    m_cursor = entryFrom.cursorFrom;

    m_journalIndex -= 1;
    flag(Flags::shapeDirty | Flags::selectionDirty);
}
void RawTextInput::redo()
{
    if (m_journal.empty() || m_journalIndex + 1 >= m_journal.size())
    {
        return;
    }
    const JournalEntry& entryTo = m_journal[m_journalIndex + 1];
    setTextPrivate(entryTo.text);
    m_cursor = entryTo.cursorTo;
    m_journalIndex += 1;
    flag(Flags::shapeDirty | Flags::selectionDirty);
}

bool RawTextInput::flagged(RawTextInput::Flags mask) const
{
    return m_flags & mask;
}

bool RawTextInput::unflag(RawTextInput::Flags mask)
{
    if (m_flags & mask)
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
}

#endif
