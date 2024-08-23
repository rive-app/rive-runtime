#ifndef RiveFont_DEFINED
#define RiveFont_DEFINED

#include "include/core/SkData.h"
#include "include/core/SkPath.h"
#include "include/core/SkTypeface.h"
#include <vector>

class RiveFont
{
    struct Pair
    {
        uint16_t fChar;
        uint16_t fGlyph;
    };
    std::vector<Pair> fCMap;

    struct Glyph
    {
        SkPath fPath;
        float fAdvance;
    };
    std::vector<Glyph> fGlyphs;

public:
    uint16_t charToGlyph(SkUnichar) const;
    float advance(uint16_t glyph) const;
    const SkPath* path(uint16_t glyph) const;

    void clear()
    {
        fCMap.clear();
        fGlyphs.clear();
    }

    void load(sk_sp<SkTypeface>, const char text[], size_t length);

    sk_sp<SkData> encode() const;
    bool decode(const void*, size_t);
};

#endif
