#ifndef _RIVE_UTF_HPP_
#define _RIVE_UTF_HPP_

#include "rive/text_engine.hpp"

namespace rive
{

class UTF
{
public:
    // returns the number of bytes needed in this sequence
    // For ascii, this will return 1
    static int CountUTF8Length(const uint8_t utf8[]);

    // Return the unichar pointed to by the utf8 pointer, and then
    // update the pointer to point to the next sequence.
    static Unichar NextUTF8(const uint8_t** utf8Ptr);

    // Convert the unichar into (1 or 2) utf16 values, and return
    // the number of values.
    static int ToUTF16(Unichar uni, uint16_t utf16[]);

    // Count the number of bytes needed to encode the codepoints to utf8.
    static uint32_t CountCodePointLength(Span<const Unichar> codepoints);

    // Encode a utf8 codepoint into a byte buffer.
    static uint32_t Encode(uint8_t* output, Unichar codepoint);
};

} // namespace rive

#endif
