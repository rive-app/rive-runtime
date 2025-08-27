#ifndef _RIVE_CURSOR_HPP_
#define _RIVE_CURSOR_HPP_

#ifdef WITH_RIVE_TEXT
#include <cstdint>
#include "rive/text_engine.hpp"
#include "rive/text/glyph_lookup.hpp"
#include "rive/text/fully_shaped_text.hpp"

#include <vector>

namespace rive
{
class ShapePaintPath;

class CursorVisualPosition
{
public:
    static CursorVisualPosition missing() { return CursorVisualPosition(); }

    CursorVisualPosition(float x, float top, float bottom) :
        m_found(true), m_x(x), m_top(top), m_bottom(bottom)
    {}

    bool found() const { return m_found; }
    float x() const { return m_x; }
    float top() const { return m_top; }
    float bottom() const { return m_bottom; }

private:
    CursorVisualPosition() :
        m_found(false), m_x(0.0f), m_top(0.0f), m_bottom(0.0f)
    {}

    bool m_found;
    float m_x;
    float m_top;
    float m_bottom;
};

class CursorPosition
{
public:
    CursorPosition(uint32_t lineIndex, uint32_t codePointIndex) :
        m_lineIndex(lineIndex), m_codePointIndex(codePointIndex)
    {}

    CursorPosition(uint32_t codePointIndex) :
        m_lineIndex(-1), m_codePointIndex(codePointIndex)
    {}

    uint32_t lineIndex() const { return m_lineIndex; }
    uint32_t lineIndex(int32_t inc) const;
    uint32_t codePointIndex() const { return m_codePointIndex; }
    uint32_t codePointIndex(int32_t inc) const;

    static CursorPosition zero() { return CursorPosition(0, 0); }

    CursorPosition& operator+=(uint32_t v)
    {
        m_codePointIndex += v;
        return *this;
    }

    CursorPosition& operator-=(uint32_t v)
    {
        if (m_codePointIndex >= v)
        {
            m_codePointIndex -= v;
        }
        else
        {
            m_codePointIndex = 0;
        }
        return *this;
    }

    bool hasLineIndex() const { return m_lineIndex != -1; }

    // Find the closest lineIndex() for the codePointIndex().
    void resolveLine(const FullyShapedText& shape);

    CursorVisualPosition visualPosition(const FullyShapedText& shape) const;

    // Move this cursor to the given translation and return the visual position.
    static CursorPosition fromTranslation(const Vec2D translation,
                                          const FullyShapedText& shape);

    static CursorPosition fromLineX(uint32_t lineIndex,
                                    float x,
                                    const FullyShapedText& shape);

    static CursorPosition atIndex(uint32_t codePointIndex,
                                  const FullyShapedText& shape);

    CursorPosition clamped(const FullyShapedText& shape) const;

private:
    static CursorPosition fromOrderedLine(const OrderedLine& orderedLine,
                                          uint32_t lineIndex,
                                          float translationX,
                                          const FullyShapedText& shape);
    uint32_t m_lineIndex;
    uint32_t m_codePointIndex;
};

// Add an offset to the code point index of the cursor and return a cursor with
// undetermined line.
inline CursorPosition operator+(const CursorPosition& cursor,
                                const int32_t offset)
{
    return CursorPosition(cursor.codePointIndex(offset));
}

inline CursorPosition operator-(const CursorPosition& cursor,
                                const int32_t offset)
{
    return CursorPosition(cursor.codePointIndex(-offset));
}

inline bool operator==(const CursorPosition& a, const CursorPosition& b)
{
    return a.lineIndex() == b.lineIndex() &&
           a.codePointIndex() == b.codePointIndex();
}

inline bool operator!=(const CursorPosition& a, const CursorPosition& b)
{
    return a.lineIndex() != b.lineIndex() ||
           a.codePointIndex() != b.codePointIndex();
}

inline bool operator<(const CursorPosition& a, const CursorPosition& b)
{
    return a.codePointIndex() < b.codePointIndex();
}

inline bool operator>(const CursorPosition& a, const CursorPosition& b)
{
    return a.codePointIndex() > b.codePointIndex();
}

inline bool operator<=(const CursorPosition& a, const CursorPosition& b)
{
    return a.codePointIndex() <= b.codePointIndex();
}

inline bool operator>=(const CursorPosition& a, const CursorPosition& b)
{
    return a.codePointIndex() >= b.codePointIndex();
}

class Cursor
{
public:
    Cursor(CursorPosition start, CursorPosition end) :
        m_start(start), m_end(end)
    {}

    static Cursor collapsed(CursorPosition position)
    {
        return Cursor(position, position);
    }

    static Cursor zero() { return Cursor::collapsed(CursorPosition::zero()); }

    static Cursor atStart()
    {
        return Cursor(CursorPosition::zero(), CursorPosition::zero());
    }

    CursorPosition start() const { return m_start; }
    CursorPosition end() const { return m_end; }

    CursorPosition first() const { return m_start < m_end ? m_start : m_end; }
    CursorPosition last() const { return m_start < m_end ? m_end : m_start; }

    bool isCollapsed() const { return m_start == m_end; }
    bool hasSelection() const { return m_start != m_end; }

    void selectionRects(std::vector<AABB>& rects,
                        const FullyShapedText& shape) const;

    void updateSelectionPath(ShapePaintPath& path,
                             const std::vector<AABB>& rects,
                             const FullyShapedText& shape) const;

    bool resolveLinePositions(const FullyShapedText& shape);

    bool contains(uint32_t codePointIndex) const;

private:
    CursorPosition m_start;
    CursorPosition m_end;
};

inline bool operator==(const Cursor& a, const Cursor& b)
{
    return a.start() == b.start() && a.end() == b.end();
}

inline bool operator!=(const Cursor& a, const Cursor& b)
{
    return a.start() != b.start() || a.end() != b.end();
}
} // namespace rive

#endif
#endif