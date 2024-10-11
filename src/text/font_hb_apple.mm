#include "rive/text_engine.hpp"

#ifdef WITH_RIVE_TEXT
#include "rive/text/font_hb.hpp"
#import <CoreText/CoreText.h>

#include "hb-coretext.h"
#include "hb-ot.h"

rive::rcp<rive::Font> HBFont::FromSystem(void* systemFont,
                                         uint16_t weight,
                                         uint8_t width)
{
    auto font = hb_coretext_font_create((CTFontRef)systemFont);
    hb_variation_t variation_data[2];
    variation_data[0].tag = HB_OT_TAG_VAR_AXIS_WEIGHT;
    variation_data[0].value = weight;
    variation_data[1].tag = HB_OT_TAG_VAR_AXIS_WIDTH;
    variation_data[1].value = width;
    hb_font_set_variations(font, variation_data, 2);
    if (font)
    {
        return rive::rcp<rive::Font>(new HBFont(font));
    }
    return nullptr;
}
#endif
