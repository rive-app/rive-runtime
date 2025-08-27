#include "rive/text/utf.hpp"
#include "rive/core/type_conversions.hpp"

using namespace rive;

int UTF::CountUTF8Length(const uint8_t utf8[])
{
    unsigned lead = *utf8;
    assert(lead != 0xFF);
    assert((lead & 0xC0) != 0x80); // 10xxxxxx is not a legal lead byte
    if ((lead & 0x80) == 0)
    {
        return 1;
    }
    int n = 1;
    lead <<= 1;
    while (lead & 0x80)
    {
        n += 1;
        lead <<= 1;
    }
    assert(n >= 1 && n <= 4);
    return n;
}

// Return the unichar pointed to by the utf8 pointer, and then
// update the pointer to point to the next sequence.
Unichar UTF::NextUTF8(const uint8_t** utf8Ptr)
{
    const uint8_t* text = *utf8Ptr;

    uint32_t c = 0;
    int n = CountUTF8Length(text);
    assert(n >= 1 && n <= 4);

    unsigned first = *text++;
    if (n == 1)
    {
        c = first;
    }
    else
    {
        c = first & ((unsigned)0xFF >> n);
        --n;
        do
        {
            c = (c << 6) | (*text++ & 0x3F);
        } while (--n);
    }
    *utf8Ptr = text; // update the pointer
    return c;
}

int UTF::ToUTF16(Unichar uni, uint16_t utf16[])
{
    if (uni > 0xFFFF)
    {
        utf16[0] = castTo<uint16_t>((0xD800 - 64) | (uni >> 10));
        utf16[1] = castTo<uint16_t>(0xDC00 | (uni & 0x3FF));
        return 2;
    }
    utf16[0] = castTo<uint16_t>(uni);
    return 1;
}

uint32_t UTF::CountCodePointLength(Span<const Unichar> codepoints)
{
    uint32_t length = 0;
    for (auto codepoint : codepoints)
    {
        if (codepoint <= 0x7F)
        {
            length += 1;
        }
        else if (codepoint <= 0x07FF)
        {
            length += 2;
        }
        else if (codepoint <= 0xFFFF)
        {
            length += 3;
        }
        else if (codepoint <= 0x10FFFF)
        {
            length += 4;
        }
    }
    return length;
}

uint32_t UTF::Encode(uint8_t* output, Unichar codepoint)
{
    if (codepoint <= 0x7F)
    {
        output[0] = (uint8_t)codepoint;
        return 1;
    }
    else if (codepoint <= 0x07FF)
    {
        output[0] = (uint8_t)(((codepoint >> 6) & 0x1F) | 0xC0);
        output[1] = (uint8_t)(((codepoint >> 0) & 0x3F) | 0x80);
        return 2;
    }
    else if (codepoint <= 0xFFFF)
    {
        output[0] = (uint8_t)(((codepoint >> 12) & 0x0F) | 0xE0);
        output[1] = (uint8_t)(((codepoint >> 6) & 0x3F) | 0x80);
        output[2] = (uint8_t)(((codepoint >> 0) & 0x3F) | 0x80);
        return 3;
    }
    else if (codepoint <= 0x10FFFF)
    {
        output[0] = (uint8_t)(((codepoint >> 18) & 0x07) | 0xF0);
        output[1] = (uint8_t)(((codepoint >> 12) & 0x3F) | 0x80);
        output[2] = (uint8_t)(((codepoint >> 6) & 0x3F) | 0x80);
        output[3] = (uint8_t)(((codepoint >> 0) & 0x3F) | 0x80);
        return 4;
    }
    else
    {
        // error
        return 0;
    }
}