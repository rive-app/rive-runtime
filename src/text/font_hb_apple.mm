#include "rive/text_engine.hpp"

#ifdef WITH_RIVE_TEXT
#include "rive/text/font_hb.hpp"
#import <CoreText/CoreText.h>

#include "hb-coretext.h"

rive::rcp<rive::Font> HBFont::FromSystem(void* systemFont)
{
    auto font = hb_coretext_font_create((CTFontRef)systemFont);
    if (font)
    {
        return rive::rcp<rive::Font>(new HBFont(font));
    }
    return nullptr;
}
#endif
