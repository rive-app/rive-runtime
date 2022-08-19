/*
 * Copyright 2022 Rive
 */

#include "rive/rive_types.hpp"
#include "utils/rive_utf.hpp"

#if defined(RIVE_BUILD_FOR_APPLE) && defined(RIVE_TEXT)
#include "renderfont_coretext.hpp"
#include "mac_utils.hpp"

#include "rive/factory.hpp"
#include "rive/render_text.hpp"
#include "rive/core/type_conversions.hpp"

#if defined(RIVE_BUILD_FOR_OSX)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(RIVE_BUILD_FOR_IOS)
#include <CoreText/CoreText.h>
#include <CoreText/CTFontManager.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

constexpr int kStdScale = 2048;
constexpr float gInvScale = 1.0f / kStdScale;

static std::vector<rive::RenderFont::Axis> compute_axes(CTFontRef font) {
    std::vector<rive::RenderFont::Axis> axes;

    AutoCF array = CTFontCopyVariationAxes(font);
    if (auto count = array.get() ? CFArrayGetCount(array.get()) : 0) {
        axes.reserve(count);

        for (auto i = 0; i < count; ++i) {
            auto axis = (CFDictionaryRef)CFArrayGetValueAtIndex(array, i);

            auto tag = find_u32(axis, kCTFontVariationAxisIdentifierKey);
            auto min = find_float(axis, kCTFontVariationAxisMinimumValueKey);
            auto def = find_float(axis, kCTFontVariationAxisDefaultValueKey);
            auto max = find_float(axis, kCTFontVariationAxisMaximumValueKey);
            //     printf("%08X %g %g %g\n", tag, min, def, max);

            axes.push_back({tag, min, def, max});
        }
    }
    return axes;
}

static std::vector<rive::RenderFont::Coord> compute_coords(CTFontRef font) {
    std::vector<rive::RenderFont::Coord> coords(0);
    AutoCF dict = CTFontCopyVariation(font);
    if (dict) {
        int count = CFDictionaryGetCount(dict);
        if (count > 0) {
            coords.resize(count);

            AutoSTArray<100, const void*> ptrs(count * 2);
            const void** keys = &ptrs[0];
            const void** values = &ptrs[count];
            CFDictionaryGetKeysAndValues(dict, keys, values);
            for (int i = 0; i < count; ++i) {
                uint32_t tag = number_as_u32((CFNumberRef)keys[i]);
                float value = number_as_float((CFNumberRef)values[i]);
                //                printf("[%d] %08X %s %g\n", i, tag, tag2str(tag).c_str(), value);
                coords[i] = {tag, value};
            }
        }
    }
    return coords;
}

static rive::RenderFont::LineMetrics make_lmx(CTFontRef font) {
    return {
        (float)-CTFontGetAscent(font) * gInvScale,
        (float)CTFontGetDescent(font) * gInvScale,
    };
}

CoreTextRenderFont::CoreTextRenderFont(CTFontRef font, std::vector<rive::RenderFont::Axis> axes) :
    rive::RenderFont(make_lmx(font)),
    m_font(font), // we take ownership of font
    m_axes(std::move(axes)),
    m_coords(compute_coords(font)) {}

CoreTextRenderFont::~CoreTextRenderFont() { CFRelease(m_font); }

rive::rcp<rive::RenderFont> CoreTextRenderFont::makeAtCoords(rive::Span<const Coord> coords) const {
    AutoCF vars = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                            coords.size(),
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
    for (const auto& c : coords) {
        AutoCF tagNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &c.axis);
        AutoCF valueNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloat32Type, &c.value);
        CFDictionaryAddValue(vars.get(), tagNum.get(), valueNum.get());
    }

    AutoCF attrs = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                             1,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attrs.get(), kCTFontVariationAttribute, vars.get());

    AutoCF desc = (CTFontDescriptorRef)CTFontDescriptorCreateWithAttributes(attrs.get());

    auto font = CTFontCreateCopyWithAttributes(m_font, 0, nullptr, desc.get());

    return rive::rcp<rive::RenderFont>(new CoreTextRenderFont(font, compute_axes(font)));
}

static CTFontRef font_from_run(CTRunRef run) {
    auto attr = CTRunGetAttributes(run);
    assert(attr);
    CTFontRef ct = (CTFontRef)CFDictionaryGetValue(attr, kCTFontAttributeName);
    assert(ct);
    return ct;
}

static rive::rcp<rive::RenderFont> convert_to_renderfont(CTFontRef ct,
                                                         rive::rcp<rive::RenderFont> rf) {
    auto ctrf = static_cast<CoreTextRenderFont*>(rf.get());
    if (ctrf->m_font == ct) {
        return rf;
    }
    CFRetain(ct);
    return rive::rcp<rive::RenderFont>(new CoreTextRenderFont(ct, compute_axes(ct)));
}

static void apply_element(void* ctx, const CGPathElement* element) {
    auto path = (rive::RawPath*)ctx;
    const CGPoint* points = element->points;

    switch (element->type) {
        case kCGPathElementMoveToPoint: path->moveTo(points[0].x, points[0].y); break;

        case kCGPathElementAddLineToPoint: path->lineTo(points[0].x, points[0].y); break;

        case kCGPathElementAddQuadCurveToPoint:
            path->quadTo(points[0].x, points[0].y, points[1].x, points[1].y);
            break;

        case kCGPathElementAddCurveToPoint:
            path->cubicTo(points[0].x,
                          points[0].y,
                          points[1].x,
                          points[1].y,
                          points[2].x,
                          points[2].y);
            break;

        case kCGPathElementCloseSubpath: path->close(); break;

        default: assert(false); break;
    }
}

rive::RawPath CoreTextRenderFont::getPath(rive::GlyphID glyph) const {
    rive::RawPath rpath;

    AutoCF cgPath = CTFontCreatePathForGlyph(m_font, glyph, nullptr);
    if (!cgPath) {
        return rpath;
    }

    CGPathApply(cgPath.get(), &rpath, apply_element);
    rpath.transformInPlace(rive::Mat2D::fromScale(gInvScale, -gInvScale));
    return rpath;
}

////////////////////////////////////////////////////////////////////////////////////

struct AutoUTF16 {
    std::vector<uint16_t> array;

    AutoUTF16(const rive::Unichar uni[], int count) {
        array.reserve(count);
        for (int i = 0; i < count; ++i) {
            uint16_t tmp[2];
            int n = rive::UTF::ToUTF16(uni[i], tmp);

            for (int i = 0; i < n; ++i) {
                array.push_back(tmp[i]);
            }
        }
    }
};

static float
add_run(rive::RenderGlyphRun* gr, CTRunRef run, uint32_t textStart, float textSize, float startX) {
    if (auto count = CTRunGetGlyphCount(run)) {
        const float scale = textSize * gInvScale;

        gr->glyphs.resize(count);
        gr->xpos.resize(count + 1);
        gr->textOffsets.resize(count);

        CTRunGetGlyphs(run, {0, count}, gr->glyphs.data());

        AutoSTArray<1024, CFIndex> indices(count);
        AutoSTArray<1024, CGSize> advances(count);

        CTRunGetAdvances(run, {0, count}, advances.data());
        CTRunGetStringIndices(run, {0, count}, indices.data());

        for (CFIndex i = 0; i < count; ++i) {
            gr->xpos[i] = startX;
            gr->textOffsets[i] = textStart + indices[i]; // utf16 offsets, will fix-up later
            startX += advances[i].width * scale;
        }
        gr->xpos[count] = startX;
    }
    return startX;
}

std::vector<rive::RenderGlyphRun>
CoreTextRenderFont::onShapeText(rive::Span<const rive::Unichar> text,
                                rive::Span<const rive::RenderTextRun> truns) const {
    std::vector<rive::RenderGlyphRun> gruns;
    gruns.reserve(truns.size());

    uint32_t textIndex = 0;
    float startX = 0;
    for (const auto& tr : truns) {
        CTFontRef font = ((CoreTextRenderFont*)tr.font.get())->m_font;

        AutoUTF16 utf16(&text[textIndex], tr.unicharCount);
        const bool hasSurrogates = utf16.array.size() != tr.unicharCount;
        assert(!hasSurrogates);

        AutoCF string = CFStringCreateWithCharactersNoCopy(nullptr,
                                                           utf16.array.data(),
                                                           utf16.array.size(),
                                                           kCFAllocatorNull);

        AutoCF attr = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(attr.get(), kCTFontAttributeName, font);

        AutoCF attrString = CFAttributedStringCreate(kCFAllocatorDefault, string.get(), attr.get());

        AutoCF typesetter = CTTypesetterCreateWithAttributedString(attrString.get());

        AutoCF line = CTTypesetterCreateLine(typesetter.get(), {0, tr.unicharCount});

        CFArrayRef run_array = CTLineGetGlyphRuns(line.get());
        CFIndex runCount = CFArrayGetCount(run_array);
        for (CFIndex i = 0; i < runCount; ++i) {
            CTRunRef runref = (CTRunRef)CFArrayGetValueAtIndex(run_array, i);
            rive::RenderGlyphRun grun;
            startX = add_run(&grun, runref, textIndex, tr.size, startX);
            if (grun.glyphs.size() > 0) {
                auto ct = font_from_run(runref);
                grun.font = convert_to_renderfont(ct, tr.font);
                grun.size = tr.size;
                gruns.push_back(std::move(grun));
            }
        }
        textIndex += tr.unicharCount;
    }

    return gruns;
}

////////////////////////////////////////////////////////////////////////////////////////////////

rive::rcp<rive::RenderFont> CoreTextRenderFont::FromCT(CTFontRef ctfont) {
    if (!ctfont) {
        return nullptr;
    }

    // We always want the ctfont at our magic size
    if (CTFontGetSize(ctfont) != kStdScale) {
        ctfont = CTFontCreateCopyWithAttributes(ctfont, kStdScale, nullptr, nullptr);
    } else {
        CFRetain(ctfont);
    }

    // Apple may have secretly set the opsz axis based on the size. We want to undo this
    // since our stdsize isn't really the size we'll show it at.
    auto axes = compute_axes(ctfont);
    if (axes.size() > 0) {
        constexpr uint32_t kOPSZ = make_tag('o', 'p', 's', 'z');
        for (const auto& ax : axes) {
            if (ax.tag == kOPSZ) {
                auto xform = CGAffineTransformMakeScale(kStdScale / ax.def, kStdScale / ax.def);
                // Recreate the font at this size, but with a balancing transform,
                // so we get the 'default' shapes w.r.t. the opsz axis
                auto newfont = CTFontCreateCopyWithAttributes(ctfont, ax.def, &xform, nullptr);
                CFRelease(ctfont);
                ctfont = newfont;
                break;
            }
        }
    }

    return rive::rcp<rive::RenderFont>(new CoreTextRenderFont(ctfont, std::move(axes)));
}

rive::rcp<rive::RenderFont> CoreTextRenderFont::Decode(rive::Span<const uint8_t> span) {
    AutoCF data = CFDataCreate(nullptr, span.data(), span.size()); // makes a copy
    if (!data) {
        assert(false);
        return nullptr;
    }

    AutoCF desc = CTFontManagerCreateFontDescriptorFromData(data.get());
    if (!desc) {
        assert(false);
        return nullptr;
    }

    CTFontOptions options = kCTFontOptionsPreventAutoActivation;

    AutoCF ctfont =
        CTFontCreateWithFontDescriptorAndOptions(desc.get(), kStdScale, nullptr, options);
    if (!ctfont) {
        assert(false);
        return nullptr;
    }
    return FromCT(ctfont.get());
}

#endif
