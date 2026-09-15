/*
 * Copyright 2026 Rive
 */

#ifndef _RIVE_LINE_BREAK_HPP_
#define _RIVE_LINE_BREAK_HPP_

#include "rive/span.hpp"
#include <cstdint>

namespace rive
{

// UAX #14 line break classes after LB1 resolution (AI, SG, XX and SA never
// appear). Order must match CLASSES in dev/unicode/generate_line_break.py.
enum class LineBreakClass : uint8_t
{
    AL,
    AK,
    AP,
    AS,
    B2,
    BA,
    BB,
    BK,
    CB,
    CJ,
    CL,
    CM,
    CP,
    CR,
    EB,
    EM,
    EX,
    GL,
    H2,
    H3,
    HH,
    HL,
    HY,
    ID,
    IN,
    IS,
    JL,
    JT,
    JV,
    LF,
    NL,
    NS,
    NU,
    OP,
    PO,
    PR,
    QU,
    RI,
    SP,
    SY,
    VF,
    VI,
    WJ,
    ZW,
    ZWJ,
};

// Extra properties some rules need beyond the class. Values must match the
// FLAG_ constants in the generator.
enum LineBreakFlags : uint8_t
{
    lineBreakEastAsian = 1,      // East_Asian_Width is F, W or H
    lineBreakPi = 2,             // General_Category Pi
    lineBreakPf = 4,             // General_Category Pf
    lineBreakDottedCircle = 8,   // U+25CC
    lineBreakPictographicCn = 16 // Extended_Pictographic and unassigned
};

struct LineBreakProps
{
    LineBreakClass cls;
    uint8_t flags;
};

LineBreakProps lineBreakProps(uint32_t codepoint);

enum class LineBreak : uint8_t
{
    none,      // no break allowed at this boundary
    allowed,   // break opportunity
    mandatory, // hard break
};

// Computes the break status of every boundary in text. out must hold
// text.size() + 1 entries: out[i] is the boundary before codepoint i and
// out[0] is always none. End of text is always a break (LB3), reported as
// mandatory only when the last codepoint is itself a hard break so callers
// can tell a trailing empty line apart.
void computeLineBreaks(Span<const uint32_t> text, Span<LineBreak> out);

} // namespace rive
#endif
