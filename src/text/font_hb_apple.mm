#if defined(WITH_RIVE_TEXT) && !defined(RIVE_NO_CORETEXT)

// #if defined(TARGET_OS_IPHONE) || defined(TARGET_OS_SIMULATOR)
// #else
#include <CoreText/CoreText.h>
#include <CoreText/CTFontManager.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
// #endif

#include "rive/math/math_types.hpp"
#include "rive/text_engine.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/utf.hpp"

#include "hb-coretext.h"
#include "hb-ot.h"

constexpr int kStdScale = 2048;
constexpr float gInvScale = 1.0f / kStdScale;

class CoreTextHBFont : public HBFont
{
private:
    bool m_useSystemShaper;
    uint16_t m_weight;
    uint8_t m_width;

public:
    CoreTextHBFont(hb_font_t* font,
                   bool useSystemShaper,
                   uint16_t weight,
                   uint8_t width);

    void shapeFallbackRun(rive::SimpleArrayBuilder<rive::GlyphRun>& gruns,
                          const rive::Unichar text[],
                          const unsigned textStart,
                          const rive::TextRun& textRun,
                          const rive::TextRun& originalTextRun,
                          const uint32_t fallbackIndex) override;

    rive::RawPath getPath(rive::GlyphID glyph) const override;
};

rive::rcp<rive::Font> HBFont::FromSystem(void* systemFont,
                                         bool useSystemShaper,
                                         uint16_t weight,
                                         uint8_t width)
{
    auto ctFont = (CTFontRef)systemFont;
    if (CTFontGetSize(ctFont) != kStdScale)
    {
        // Need the font sized at our magic scale so we can extract normalized
        // path data.
        ctFont =
            CTFontCreateCopyWithAttributes(ctFont, kStdScale, nullptr, nullptr);
    }
    auto font = hb_coretext_font_create(ctFont);
    if (font)
    {
        return rive::rcp<rive::Font>(
            new CoreTextHBFont(font, useSystemShaper, weight, width));
    }
    return nullptr;
}

template <size_t N, typename T> class AutoSTArray
{
    T m_storage[N];
    T* m_ptr;
    const size_t m_count;

public:
    AutoSTArray(size_t n) : m_count(n)
    {
        m_ptr = m_storage;
        if (n > N)
        {
            m_ptr = new T[n];
        }
    }
    ~AutoSTArray()
    {
        if (m_ptr != m_storage)
        {
            delete[] m_ptr;
        }
    }

    T* data() const { return m_ptr; }

    T& operator[](size_t index)
    {
        assert(index < m_count);
        return m_ptr[index];
    }
};
template <typename T> class AutoCF
{
    T m_obj;

public:
    AutoCF(T obj = nullptr) : m_obj(obj) {}
    AutoCF(const AutoCF& other)
    {
        if (other.m_obj)
        {
            CFRetain(other.m_obj);
        }
        m_obj = other.m_obj;
    }
    AutoCF(AutoCF&& other)
    {
        m_obj = other.m_obj;
        other.m_obj = nullptr;
    }
    ~AutoCF()
    {
        if (m_obj)
        {
            CFRelease(m_obj);
        }
    }

    AutoCF& operator=(const AutoCF& other)
    {
        if (m_obj != other.m_obj)
        {
            if (other.m_obj)
            {
                CFRetain(other.m_obj);
            }
            if (m_obj)
            {
                CFRelease(m_obj);
            }
            m_obj = other.m_obj;
        }
        return *this;
    }

    void reset(T obj)
    {
        if (obj != m_obj)
        {
            if (m_obj)
            {
                CFRelease(m_obj);
            }
            m_obj = obj;
        }
    }

    operator T() const { return m_obj; }
    operator bool() const { return m_obj != nullptr; }
    T get() const { return m_obj; }
};

struct AutoUTF16
{
    std::vector<uint16_t> array;

    AutoUTF16(const rive::Unichar uni[], int count)
    {
        array.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            uint16_t tmp[2];
            int n = rive::UTF::ToUTF16(uni[i], tmp);

            for (int i = 0; i < n; ++i)
            {
                array.push_back(tmp[i]);
            }
        }
    }
};

static void apply_element(void* ctx, const CGPathElement* element)
{
    auto path = (rive::RawPath*)ctx;
    const CGPoint* points = element->points;

    switch (element->type)
    {
        case kCGPathElementMoveToPoint:
            path->moveTo((float)points[0].x * gInvScale,
                         (float)-points[0].y * gInvScale);
            break;

        case kCGPathElementAddLineToPoint:
            path->lineTo((float)points[0].x * gInvScale,
                         (float)-points[0].y * gInvScale);
            break;

        case kCGPathElementAddQuadCurveToPoint:

            path->quadToCubic((float)points[0].x * gInvScale,
                              (float)-points[0].y * gInvScale,
                              (float)points[1].x * gInvScale,
                              (float)-points[1].y * gInvScale);
            break;

        case kCGPathElementAddCurveToPoint:

            path->cubicTo((float)points[0].x * gInvScale,
                          (float)-points[0].y * gInvScale,
                          (float)points[1].x * gInvScale,
                          (float)-points[1].y * gInvScale,
                          (float)points[2].x * gInvScale,
                          (float)-points[2].y * gInvScale);
            break;

        case kCGPathElementCloseSubpath:
            path->close();
            break;

        default:
            assert(false);
            break;
    }
}

CoreTextHBFont::CoreTextHBFont(hb_font_t* font,
                               bool useSystemShaper,
                               uint16_t weight,
                               uint8_t width) :
    m_useSystemShaper(useSystemShaper),
    m_weight(weight),
    m_width(width),
    HBFont(font)
{
    hb_variation_t variation_data[2];
    variation_data[0].tag = HB_OT_TAG_VAR_AXIS_WEIGHT;
    variation_data[0].value = weight;
    variation_data[1].tag = HB_OT_TAG_VAR_AXIS_WIDTH;
    variation_data[1].value = width;
    hb_font_set_variations(font, variation_data, 2);
}

void CoreTextHBFont::shapeFallbackRun(
    rive::SimpleArrayBuilder<rive::GlyphRun>& gruns,
    const rive::Unichar text[],
    const unsigned textStart,
    const rive::TextRun& textRun,
    const rive::TextRun& originalTextRun,
    const uint32_t fallbackIndex)
{
    if (!m_useSystemShaper)
    {
        HBFont::shapeFallbackRun(
            gruns, text, textStart, textRun, originalTextRun, fallbackIndex);
        return;
    }

    CTFontRef ctFont = hb_coretext_font_get_ct_font(m_font);

    AutoUTF16 utf16(&text[textStart], textRun.unicharCount);

    assert(utf16.array.size() == textRun.unicharCount);

    AutoCF<CFStringRef> string = CFStringCreateWithCharactersNoCopy(
        nullptr, utf16.array.data(), utf16.array.size(), kCFAllocatorNull);

    AutoCF<CFMutableDictionaryRef> attr =
        CFDictionaryCreateMutable(kCFAllocatorDefault,
                                  0,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(attr.get(), kCTFontAttributeName, ctFont);

    AutoCF<CFMutableAttributedStringRef> attrString =
        CFAttributedStringCreateMutable(kCFAllocatorDefault,
                                        textRun.unicharCount);
    CFAttributedStringReplaceString(
        attrString.get(), CFRangeMake(0, 0), string.get());

    AutoCF<CFNumberRef> level_number =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &textRun.level);
    AutoCF<CFDictionaryRef> options = CFDictionaryCreate(
        kCFAllocatorDefault,
        (const void**)&kCTTypesetterOptionForcedEmbeddingLevel,
        (const void**)&level_number,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    AutoCF<CTTypesetterRef> typesetter =
        CTTypesetterCreateWithAttributedStringAndOptions(attrString.get(),
                                                         options.get());

    AutoCF<CTLineRef> line = CTTypesetterCreateLine(typesetter.get(), {0, 0});

    CFArrayRef run_array = CTLineGetGlyphRuns(line.get());
    CFIndex runCount = CFArrayGetCount(run_array);
    bool isEvenLevel = textRun.level % 2 == 0;
    for (CFIndex runIndex = 0; runIndex < runCount; runIndex++)
    {
        CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(run_array, runIndex);

        if (CFIndex count = CTRunGetGlyphCount(run))
        {
            rive::GlyphRun gr(count);

            // Because CoreText will automatically do its own font fallbacks
            // we need to detect that it's trying to use a different font.
            CFDictionaryRef attributes = CTRunGetAttributes(run);
            CTFontRef runCtFont = static_cast<CTFontRef>(
                CFDictionaryGetValue(attributes, kCTFontAttributeName));
            const float scale = textRun.size / (float)CTFontGetSize(runCtFont);
            if (!CFEqual(runCtFont, ctFont))
            {
                // Get the original font's traits and create a fallback font
                // with the same traits. In some cases, CoreText will use a
                // different font for the fallback, but the fallback font will
                // not have the same traits (e.g weight) as the original font.
                CTFontSymbolicTraits originalTraits =
                    CTFontGetSymbolicTraits(ctFont);
                CTFontRef adjustedFallbackFont =
                    CTFontCreateCopyWithSymbolicTraits(runCtFont,
                                                       CTFontGetSize(runCtFont),
                                                       nullptr,
                                                       originalTraits,
                                                       originalTraits);

                // Use the adjusted font instead
                gr.font = HBFont::FromSystem(
                    (void*)adjustedFallbackFont, true, m_weight, m_width);
                CFRelease(adjustedFallbackFont);
            }
            else
            {
                gr.font = textRun.font;
            }
            gr.size = textRun.size;
            gr.lineHeight = textRun.lineHeight;
            gr.letterSpacing = textRun.letterSpacing;
            gr.styleId = textRun.styleId;
            gr.level = textRun.level;

            CTRunGetGlyphs(run, {0, count}, gr.glyphs.data());
            if (!isEvenLevel)
            {
                std::reverse(gr.glyphs.begin(), gr.glyphs.end());
            }

            AutoSTArray<1024, CFIndex> indices(count);
            AutoSTArray<1024, CGSize> advances(count);

            CTRunGetAdvances(run, {0, count}, advances.data());
            CTRunGetStringIndices(run, {0, count}, indices.data());

            CFIndex reverseIndex = count - 1;
            for (CFIndex i = 0; i < count; ++i)
            {
                CFIndex glyphIndex = isEvenLevel ? i : reverseIndex;
                float advance =
                    (float)(advances[i].width * scale) + textRun.letterSpacing;
                gr.xpos[glyphIndex] = gr.advances[glyphIndex] = advance;
                gr.textIndices[glyphIndex] =
                    rive::math::lossless_numeric_cast<uint32_t>(
                        textStart +
                        indices[i]); // utf16 offsets, will fix-up later

                gr.offsets[glyphIndex] = rive::Vec2D(0.0f, 0.0f);
                reverseIndex--;
            }
            gr.xpos[count] = 0;

            gruns.add(std::move(gr));
        }
    }
}

rive::RawPath CoreTextHBFont::getPath(rive::GlyphID glyph) const
{
    // When we use the system shaper (coretext) get the glyphs from there
    // too.
    if (m_useSystemShaper)
    {
        CTFontRef ctFont = hb_coretext_font_get_ct_font(m_font);
        if (ctFont)
        {
            AutoCF<CGPathRef> cgPath =
                CTFontCreatePathForGlyph(ctFont, glyph, nullptr);

            if (cgPath)
            {
                rive::RawPath rpath;
                CGPathApply(cgPath.get(), &rpath, apply_element);
                return rpath;
            }
        }
    }

    rive::RawPath rpath = HBFont::getPath(glyph);

    // We didn't use the system shaper, but harfbuzz failed to extract the
    // glyphs. Try getting them from the system.
    if (rpath.empty() && !m_useSystemShaper)
    {
        CTFontRef ctFont = hb_coretext_font_get_ct_font(m_font);
        if (ctFont)
        {
            AutoCF<CGPathRef> cgPath =
                CTFontCreatePathForGlyph(ctFont, glyph, nullptr);

            if (cgPath)
            {
                rive::RawPath rpath;
                CGPathApply(cgPath.get(), &rpath, apply_element);
                return rpath;
            }
        }
    }
    return rpath;
}
#endif
