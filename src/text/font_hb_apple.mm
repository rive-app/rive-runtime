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
    CTFontRef m_ctFont;
    bool m_useSystemShaper;

public:
    CoreTextHBFont(hb_font_t* font,
                   CTFontRef ctFont,
                   bool useSystemShaper,
                   uint16_t weight,
                   uint8_t width);
    ~CoreTextHBFont() override;

    void shapeFallbackRun(rive::SimpleArrayBuilder<rive::GlyphRun>& gruns,
                          const rive::Unichar text[],
                          const unsigned textStart,
                          const rive::TextRun& textRun,
                          const rive::TextRun& originalTextRun,
                          const uint32_t fallbackIndex) override;

    rive::RawPath getPath(rive::GlyphID glyph) const override;
};

struct HBCTFaceData
{
    CTFontRef ctFont;
};

static void hb_ct_face_data_destroy(void* userData)
{
    auto data = static_cast<HBCTFaceData*>(userData);
    if (data != nullptr)
    {
        if (data->ctFont != nullptr)
        {
            CFRelease(data->ctFont);
        }
        delete data;
    }
}

static hb_blob_t* hb_ct_reference_table(hb_face_t*,
                                        hb_tag_t tag,
                                        void* userData)
{
    auto data = static_cast<HBCTFaceData*>(userData);
    if (data == nullptr || data->ctFont == nullptr)
    {
        return hb_blob_get_empty();
    }

    CFDataRef table = CTFontCopyTable(data->ctFont,
                                      static_cast<CTFontTableTag>(tag),
                                      kCTFontTableOptionNoOptions);
    if (table == nullptr)
    {
        return hb_blob_get_empty();
    }

    const char* bytes = reinterpret_cast<const char*>(CFDataGetBytePtr(table));
    const auto length = static_cast<unsigned int>(CFDataGetLength(table));

    return hb_blob_create(bytes,
                          length,
                          HB_MEMORY_MODE_READONLY,
                          (void*)table,
                          [](void* blobUserData) {
                              CFRelease(static_cast<CFDataRef>(blobUserData));
                          });
}

static hb_font_t* hb_font_create_from_ct_tables(CTFontRef ctFont)
{
    if (ctFont == nullptr)
    {
        return nullptr;
    }

    auto* faceData = new HBCTFaceData{(CTFontRef)CFRetain(ctFont)};
    hb_face_t* face = hb_face_create_for_tables(
        hb_ct_reference_table, faceData, hb_ct_face_data_destroy);
    if (face == nullptr)
    {
        hb_ct_face_data_destroy(faceData);
        return nullptr;
    }

    const unsigned int upem = (unsigned int)CTFontGetUnitsPerEm(ctFont);
    if (upem > 0)
    {
        hb_face_set_upem(face, upem);
    }

    hb_font_t* font = hb_font_create(face);
    hb_face_destroy(face);
    return font;
}

rive::rcp<rive::Font> HBFont::FromSystem(void* systemFont,
                                         bool useSystemShaper,
                                         uint16_t weight,
                                         uint8_t width)
{
    CTFontRef ctFont = (CTFontRef)systemFont;
    bool isCopy = false;
    if (CTFontGetSize(ctFont) != kStdScale)
    {
        // Need the font sized at our magic scale so we can extract normalized
        // path data.
        ctFont =
            CTFontCreateCopyWithAttributes(ctFont, kStdScale, nullptr, nullptr);
        isCopy = true;
    }

    hb_font_t* font = hb_font_create_from_ct_tables(ctFont);
    if (font == nullptr)
    {
        font = hb_coretext_font_create(ctFont);
    }
    if (font)
    {
        auto riveFont = rive::rcp<rive::Font>(
            new CoreTextHBFont(font, ctFont, useSystemShaper, weight, width));
        if (isCopy)
        {
            CFRelease(ctFont);
        }
        return riveFont;
    }
    if (isCopy)
    {
        CFRelease(ctFont);
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

static bool ct_extract_glyph_path(CTFontRef ctFont,
                                  rive::GlyphID glyph,
                                  rive::RawPath* outPath)
{
    if (ctFont == nullptr)
    {
        return false;
    }
    AutoCF<CGPathRef> cgPath = CTFontCreatePathForGlyph(ctFont, glyph, nullptr);
    if (!cgPath)
    {
        return false;
    }
    CGPathApply(cgPath.get(), outPath, apply_element);
    return true;
}

CoreTextHBFont::CoreTextHBFont(hb_font_t* font,
                               CTFontRef ctFont,
                               bool useSystemShaper,
                               uint16_t weight,
                               uint8_t width) :
    m_ctFont(ctFont ? (CTFontRef)CFRetain(ctFont) : nullptr),
    m_useSystemShaper(useSystemShaper),
    HBFont(font)
{
    hb_variation_t variation_data[2];
    variation_data[0].tag = HB_OT_TAG_VAR_AXIS_WEIGHT;
    variation_data[0].value = weight;
    variation_data[1].tag = HB_OT_TAG_VAR_AXIS_WIDTH;
    variation_data[1].value = width;
    hb_font_set_variations(font, variation_data, 2);
}

CoreTextHBFont::~CoreTextHBFont()
{
    if (m_ctFont)
    {
        CFRelease(m_ctFont);
    }
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

    CTFontRef ctFont = m_ctFont;

    AutoUTF16 utf16(&text[textStart], textRun.unicharCount);
    CFIndex utf16Length =
        rive::math::lossless_numeric_cast<CFIndex>(utf16.array.size());

    assert(utf16.array.size() >= textRun.unicharCount);

    AutoCF<CFStringRef> string = CFStringCreateWithCharactersNoCopy(
        nullptr, utf16.array.data(), utf16Length, kCFAllocatorNull);

    AutoCF<CFMutableDictionaryRef> attr =
        CFDictionaryCreateMutable(kCFAllocatorDefault,
                                  0,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(attr.get(), kCTFontAttributeName, ctFont);

    AutoCF<CFMutableAttributedStringRef> attrString =
        CFAttributedStringCreateMutable(kCFAllocatorDefault, utf16Length);
    CFAttributedStringReplaceString(
        attrString.get(), CFRangeMake(0, 0), string.get());
    CFAttributedStringSetAttributes(
        attrString.get(), CFRangeMake(0, utf16Length), attr.get(), false);

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
            const float scale = textRun.size / (float)CTFontGetSize(ctFont);
            gr.font = textRun.font;
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
        rive::RawPath ctPath;
        if (ct_extract_glyph_path(m_ctFont, glyph, &ctPath))
        {
            return ctPath;
        }
    }

    rive::RawPath rpath = HBFont::getPath(glyph);

    // We didn't use the system shaper, but harfbuzz failed to extract the
    // glyphs. Try getting them from the system.
    if (rpath.empty() && !m_useSystemShaper)
    {
        rive::RawPath ctPath;
        if (ct_extract_glyph_path(m_ctFont, glyph, &ctPath))
        {
            return ctPath;
        }
    }
    return rpath;
}
#endif
