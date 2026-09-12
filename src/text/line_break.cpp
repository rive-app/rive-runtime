/*
 * Copyright 2026 Rive
 */

#include "rive/text/line_break.hpp"
#include <cassert>
#include <vector>

using namespace rive;

namespace rive
{
extern const uint32_t kLineBreakHighStart;
extern const uint32_t kLineBreakTopShift;
extern const uint32_t kLineBreakBlockShift;
extern const uint8_t kLineBreakDefault;
extern const LineBreakProps kLineBreakExtClasses[];
extern const uint8_t kLineBreakTop[];
extern const uint16_t kLineBreakMid[];
extern const uint8_t kLineBreakData[];
} // namespace rive

LineBreakProps rive::lineBreakProps(uint32_t cp)
{
    if (cp >= kLineBreakHighStart)
    {
        return kLineBreakExtClasses[kLineBreakDefault];
    }
    uint32_t midEntries = 1u << (kLineBreakTopShift - kLineBreakBlockShift);
    uint32_t mid = kLineBreakTop[cp >> kLineBreakTopShift] * midEntries +
                   ((cp >> kLineBreakBlockShift) & (midEntries - 1));
    uint32_t blockSize = 1u << kLineBreakBlockShift;
    uint32_t data = kLineBreakMid[mid] * blockSize + (cp & (blockSize - 1));
    return kLineBreakExtClasses[kLineBreakData[data]];
}

namespace
{
using C = LineBreakClass;

// One entry per codepoint that survives LB9 (combining sequences collapse
// onto their base), so the rules can index neighbours directly.
struct Item
{
    C cls;
    uint8_t flags;
    bool afterZwj;  // the codepoint just before this item is a ZWJ
    uint32_t index; // first codepoint of the item
};

// Items live on the stack for typical UI strings.
constexpr size_t kInlineItems = 256;

bool isHardBreak(C c)
{
    return c == C::BK || c == C::CR || c == C::LF || c == C::NL;
}

bool isAlphabetic(C c) { return c == C::AL || c == C::HL; }

bool isBrahmicBase(const Item& i)
{
    return i.cls == C::AK || i.cls == C::AS ||
           (i.flags & lineBreakDottedCircle) != 0;
}

bool isAkOrDottedCircle(const Item& i)
{
    return i.cls == C::AK || (i.flags & lineBreakDottedCircle) != 0;
}

bool isEastAsian(const Item& i) { return (i.flags & lineBreakEastAsian) != 0; }

bool isKorean(C c)
{
    return c == C::JL || c == C::JV || c == C::JT || c == C::H2 || c == C::H3;
}

// Decides the boundary before items[k] (1 <= k < items.size()). Rules run in
// UAX #14 order and the first match wins.
LineBreak breakBefore(const Item* items,
                      size_t count,
                      size_t k,
                      size_t beforeSpaces,
                      size_t regionalIndicators)
{
    const Item& prev = items[k - 1];
    const Item& next = items[k];
    C p = prev.cls;
    C n = next.cls;
    // Class of the last item before any run of spaces ending at k - 1. SP
    // stands in when there is none so no rule matches it.
    C ps = beforeSpaces < count ? items[beforeSpaces].cls : C::SP;
    bool atStart = k == 1;

    // LB4, LB5
    if (p == C::BK)
    {
        return LineBreak::mandatory;
    }
    if (p == C::CR && n == C::LF)
    {
        return LineBreak::none;
    }
    if (p == C::CR || p == C::LF || p == C::NL)
    {
        return LineBreak::mandatory;
    }
    // LB6, LB7
    if (isHardBreak(n) || n == C::SP || n == C::ZW)
    {
        return LineBreak::none;
    }
    // LB8
    if (p == C::ZW || (p == C::SP && ps == C::ZW))
    {
        return LineBreak::allowed;
    }
    // LB8a
    if (next.afterZwj)
    {
        return LineBreak::none;
    }
    // The two dominant pairs skip the rule chain: no rule between here and
    // LB28 touches letter-letter, and none before LB31 touches ideograph
    // pairs.
    if (isAlphabetic(p) && isAlphabetic(n))
    {
        return LineBreak::none;
    }
    if (p == C::ID && n == C::ID)
    {
        return LineBreak::allowed;
    }
    // LB11, LB12
    if (n == C::WJ || p == C::WJ || p == C::GL)
    {
        return LineBreak::none;
    }
    // LB12a
    if (n == C::GL && p != C::SP && p != C::BA && p != C::HY && p != C::HH)
    {
        return LineBreak::none;
    }
    // LB13
    if (n == C::CL || n == C::CP || n == C::EX || n == C::SY)
    {
        return LineBreak::none;
    }
    // LB14
    if (p == C::OP || (p == C::SP && ps == C::OP))
    {
        return LineBreak::none;
    }
    // LB15a
    {
        size_t j = p == C::SP ? beforeSpaces : k - 1;
        if (j < count && items[j].cls == C::QU &&
            (items[j].flags & lineBreakPi) != 0)
        {
            bool ok = j == 0;
            if (!ok)
            {
                C b = items[j - 1].cls;
                ok = isHardBreak(b) || b == C::OP || b == C::QU || b == C::GL ||
                     b == C::SP || b == C::ZW;
            }
            if (ok)
            {
                return LineBreak::none;
            }
        }
    }
    // LB15b
    if (n == C::QU && (next.flags & lineBreakPf) != 0)
    {
        bool ok = k + 1 == count;
        if (!ok)
        {
            C a = items[k + 1].cls;
            ok = a == C::SP || a == C::GL || a == C::WJ || a == C::CL ||
                 a == C::QU || a == C::CP || a == C::EX || a == C::IS ||
                 a == C::SY || isHardBreak(a) || a == C::ZW;
        }
        if (ok)
        {
            return LineBreak::none;
        }
    }
    // LB15c
    if (p == C::SP && n == C::IS && k + 1 < count && items[k + 1].cls == C::NU)
    {
        return LineBreak::allowed;
    }
    // LB15d
    if (n == C::IS)
    {
        return LineBreak::none;
    }
    // LB16
    if (n == C::NS && (p == C::CL || p == C::CP ||
                       (p == C::SP && (ps == C::CL || ps == C::CP))))
    {
        return LineBreak::none;
    }
    // LB17
    if (n == C::B2 && (p == C::B2 || (p == C::SP && ps == C::B2)))
    {
        return LineBreak::none;
    }
    // LB18
    if (p == C::SP)
    {
        return LineBreak::allowed;
    }
    // LB19
    if (n == C::QU && (next.flags & lineBreakPi) == 0)
    {
        return LineBreak::none;
    }
    if (p == C::QU && (prev.flags & lineBreakPf) == 0)
    {
        return LineBreak::none;
    }
    // LB19a
    if (n == C::QU)
    {
        if (!isEastAsian(prev) || k + 1 == count || !isEastAsian(items[k + 1]))
        {
            return LineBreak::none;
        }
    }
    if (p == C::QU)
    {
        if (!isEastAsian(next) || atStart || !isEastAsian(items[k - 2]))
        {
            return LineBreak::none;
        }
    }
    // LB20
    if (n == C::CB || p == C::CB)
    {
        return LineBreak::allowed;
    }
    // LB20a
    if ((p == C::HY || p == C::HH) && isAlphabetic(n))
    {
        bool ok = atStart;
        if (!ok)
        {
            C b = items[k - 2].cls;
            ok = isHardBreak(b) || b == C::SP || b == C::ZW || b == C::CB ||
                 b == C::GL;
        }
        if (ok)
        {
            return LineBreak::none;
        }
    }
    // LB21
    if (n == C::BA || n == C::HH || n == C::HY || n == C::NS || p == C::BB)
    {
        return LineBreak::none;
    }
    // LB21a
    if ((p == C::HY || p == C::HH) && n != C::HL && !atStart &&
        items[k - 2].cls == C::HL)
    {
        return LineBreak::none;
    }
    // LB21b
    if (p == C::SY && n == C::HL)
    {
        return LineBreak::none;
    }
    // LB22
    if (n == C::IN)
    {
        return LineBreak::none;
    }
    // LB23
    if ((isAlphabetic(p) && n == C::NU) || (p == C::NU && isAlphabetic(n)))
    {
        return LineBreak::none;
    }
    // LB23a
    if (p == C::PR && (n == C::ID || n == C::EB || n == C::EM))
    {
        return LineBreak::none;
    }
    if ((p == C::ID || p == C::EB || p == C::EM) && n == C::PO)
    {
        return LineBreak::none;
    }
    // LB24
    if (((p == C::PR || p == C::PO) && isAlphabetic(n)) ||
        (isAlphabetic(p) && (n == C::PR || n == C::PO)))
    {
        return LineBreak::none;
    }
    // LB25
    if (n == C::PO || n == C::PR || n == C::NU)
    {
        // NU (SY | IS)* (CL | CP)? × (PO | PR) and NU (SY | IS)* × NU
        size_t j = k - 1;
        bool searching = true;
        if (n != C::NU && (items[j].cls == C::CL || items[j].cls == C::CP))
        {
            searching = j > 0;
            j--;
        }
        while (searching && (items[j].cls == C::SY || items[j].cls == C::IS))
        {
            searching = j > 0;
            j--;
        }
        if (searching && items[j].cls == C::NU)
        {
            return LineBreak::none;
        }
    }
    if (p == C::PO || p == C::PR)
    {
        if (n == C::NU)
        {
            return LineBreak::none;
        }
        if (n == C::OP && k + 1 < count)
        {
            if (items[k + 1].cls == C::NU)
            {
                return LineBreak::none;
            }
            if (items[k + 1].cls == C::IS && k + 2 < count &&
                items[k + 2].cls == C::NU)
            {
                return LineBreak::none;
            }
        }
    }
    if ((p == C::HY || p == C::IS) && n == C::NU)
    {
        return LineBreak::none;
    }
    // LB26
    if (p == C::JL && (n == C::JL || n == C::JV || n == C::H2 || n == C::H3))
    {
        return LineBreak::none;
    }
    if ((p == C::JV || p == C::H2) && (n == C::JV || n == C::JT))
    {
        return LineBreak::none;
    }
    if ((p == C::JT || p == C::H3) && n == C::JT)
    {
        return LineBreak::none;
    }
    // LB27
    if ((isKorean(p) && n == C::PO) || (p == C::PR && isKorean(n)))
    {
        return LineBreak::none;
    }
    // LB28
    if (isAlphabetic(p) && isAlphabetic(n))
    {
        return LineBreak::none;
    }
    // LB28a
    if (p == C::AP && isBrahmicBase(next))
    {
        return LineBreak::none;
    }
    if (isBrahmicBase(prev) && (n == C::VF || n == C::VI))
    {
        return LineBreak::none;
    }
    if (p == C::VI && !atStart && isBrahmicBase(items[k - 2]) &&
        isAkOrDottedCircle(next))
    {
        return LineBreak::none;
    }
    if (isBrahmicBase(prev) && isBrahmicBase(next) && k + 1 < count &&
        items[k + 1].cls == C::VF)
    {
        return LineBreak::none;
    }
    // LB29
    if (p == C::IS && isAlphabetic(n))
    {
        return LineBreak::none;
    }
    // LB30
    if ((isAlphabetic(p) || p == C::NU) && n == C::OP && !isEastAsian(next))
    {
        return LineBreak::none;
    }
    if (p == C::CP && !isEastAsian(prev) && (isAlphabetic(n) || n == C::NU))
    {
        return LineBreak::none;
    }
    // LB30a
    if (p == C::RI && n == C::RI && (regionalIndicators & 1))
    {
        return LineBreak::none;
    }
    // LB30b
    if (n == C::EM &&
        (p == C::EB || (prev.flags & lineBreakPictographicCn) != 0))
    {
        return LineBreak::none;
    }
    // LB31
    return LineBreak::allowed;
}
} // namespace

void rive::computeLineBreaks(Span<const uint32_t> text, Span<LineBreak> out)
{
    size_t n = text.size();
    assert(out.size() == n + 1);
    for (size_t i = 0; i <= n; i++)
    {
        out[i] = LineBreak::none;
    }
    if (n == 0)
    {
        return;
    }
    out[n] = LineBreak::allowed;

    // LB1 is baked into the table; CJ resolves to NS (strict) here. LB9 and
    // LB10: combining marks and ZWJ attach to any base except hard breaks,
    // spaces and ZW, otherwise they act as AL.
    Item inlineItems[kInlineItems];
    std::vector<Item> heapItems;
    Item* items = inlineItems;
    if (n > kInlineItems)
    {
        heapItems.resize(n);
        items = heapItems.data();
    }
    size_t count = 0;
    bool afterZwj = false;
    for (uint32_t i = 0; i < n; i++)
    {
        LineBreakProps props = lineBreakProps(text[i]);
        C cls = props.cls;
        bool isZwj = cls == C::ZWJ;
        if (cls == C::CJ)
        {
            cls = C::NS;
        }
        if (cls == C::CM || cls == C::ZWJ)
        {
            if (count > 0)
            {
                C base = items[count - 1].cls;
                if (!isHardBreak(base) && base != C::SP && base != C::ZW)
                {
                    afterZwj = isZwj;
                    continue;
                }
            }
            cls = C::AL;
            props.flags = 0;
        }
        items[count++] = {cls, props.flags, afterZwj, i};
        afterZwj = isZwj;
    }

    size_t beforeSpaces = count;
    // Count of consecutive regional indicators ending at k - 1.
    size_t regionalIndicators = 0;
    for (size_t k = 1; k < count; k++)
    {
        if (items[k - 1].cls != C::SP)
        {
            beforeSpaces = k - 1;
        }
        regionalIndicators =
            items[k - 1].cls == C::RI ? regionalIndicators + 1 : 0;
        out[items[k].index] =
            breakBefore(items, count, k, beforeSpaces, regionalIndicators);
    }
    if (isHardBreak(items[count - 1].cls))
    {
        out[n] = LineBreak::mandatory;
    }
}
