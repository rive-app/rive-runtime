/*
 * Copyright 2022 Rive
 */

#include "viewer/viewer_content.hpp"
#include "rive/text/utf.hpp"

#include "rive/math/raw_path.hpp"
#include "rive/factory.hpp"
#include "rive/refcnt.hpp"
#include "rive/text_engine.hpp"
#include <algorithm>

using FontTextRuns = std::vector<rive::TextRun>;
using FontGlyphRuns = rive::SimpleArray<rive::GlyphRun>;
using FontFactory = rive::rcp<rive::Font> (*)(const rive::Span<const uint8_t>);

static float drawrun(rive::Factory* factory,
                     rive::Renderer* renderer,
                     const rive::GlyphRun& run,
                     unsigned startIndex,
                     unsigned endIndex,
                     rive::Vec2D origin)
{
    auto font = run.font.get();
    const auto scale = rive::Mat2D::fromScale(run.size, run.size);
    auto paint = factory->makeRenderPaint();
    paint->color(0xFFFFFFFF);

    float x = origin.x;
    assert(startIndex >= 0 && endIndex <= run.glyphs.size());
    int i, end, inc;
    if (run.dir == rive::TextDirection::rtl)
    {
        i = endIndex - 1;
        end = startIndex - 1;
        inc = -1;
    }
    else
    {
        i = startIndex;
        end = endIndex;
        inc = 1;
    }
    while (i != end)
    {
        auto trans = rive::Mat2D::fromTranslate(x, origin.y);
        x += run.advances[i];
        auto rawpath = font->getPath(run.glyphs[i]);
        rawpath.transformInPlace(trans * scale);
        auto path = factory->makeRenderPath(rawpath, rive::FillRule::nonZero);
        renderer->drawPath(path.get(), paint.get());
        i += inc;
    }
    return x;
}

static float drawpara(rive::Factory* factory,
                      rive::Renderer* renderer,
                      const rive::Paragraph& paragraph,
                      const rive::SimpleArray<rive::GlyphLine>& lines,
                      rive::Vec2D origin)
{

    for (const auto& line : lines)
    {

        float x = line.startX + origin.x;
        int runIndex, endRun, runInc;
        if (paragraph.baseDirection == rive::TextDirection::rtl)
        {
            runIndex = line.endRunIndex;
            endRun = line.startRunIndex - 1;
            runInc = -1;
        }
        else
        {
            runIndex = line.startRunIndex;
            endRun = line.endRunIndex + 1;
            runInc = 1;
        }
        while (runIndex != endRun)
        {
            const auto& run = paragraph.runs[runIndex];
            int startGIndex = runIndex == line.startRunIndex ? line.startGlyphIndex : 0;
            int endGIndex = runIndex == line.endRunIndex ? line.endGlyphIndex : run.glyphs.size();

            x = drawrun(factory,
                        renderer,
                        run,
                        startGIndex,
                        endGIndex,
                        {x, origin.y + line.baseline});

            runIndex += runInc;
        }
    }
    return origin.y + lines.back().bottom;
}

////////////////////////////////////////////////////////////////////////////////////
std::vector<rive::rcp<rive::Font>> fallbackFonts;
static rive::rcp<rive::Font> pickFallbackFont(rive::Span<const rive::Unichar> missing)
{
    size_t length = fallbackFonts.size();
    for (size_t i = 0; i < length; i++)
    {
        auto font = fallbackFonts[i];
        if (font->hasGlyph(missing))
        {
            return font;
        }
    }
    return nullptr;
}

static rive::rcp<rive::RenderPath> make_line(rive::Factory* factory, rive::Vec2D a, rive::Vec2D b)
{
    rive::RawPath rawPath;
    rawPath.move(a);
    rawPath.line(b);
    return factory->makeRenderPath(rawPath, rive::FillRule::nonZero);
}

static void draw_line(rive::Factory* factory, rive::Renderer* renderer, float x)
{
    auto paint = factory->makeRenderPaint();
    paint->style(rive::RenderPaintStyle::stroke);
    paint->thickness(1);
    paint->color(0xFFFFFFFF);
    auto path = make_line(factory, {x, 0}, {x, 1000});
    renderer->drawPath(path.get(), paint.get());
}

static rive::TextRun append(std::vector<rive::Unichar>* unichars,
                            rive::rcp<rive::Font> font,
                            float size,
                            float lineHeight,
                            const char text[])
{
    const uint8_t* ptr = (const uint8_t*)text;
    uint32_t n = 0;
    while (*ptr)
    {
        unichars->push_back(rive::UTF::NextUTF8(&ptr));
        n += 1;
    }
    return {std::move(font), size, lineHeight, 0.0f, n};
}

class TextContent : public ViewerContent
{
    std::vector<rive::Unichar> m_unichars;

    rive::SimpleArray<rive::Paragraph> m_paragraphs;
    rive::Mat2D m_xform;
    float m_width = 300;
    bool m_autoWidth = false;
    int m_align = 0;

    FontTextRuns make_truns(FontFactory fact)
    {
        auto loader = [fact](const char filename[]) -> rive::rcp<rive::Font> {
            auto bytes = ViewerContent::LoadFile(filename);
            if (bytes.size() == 0)
            {
                assert(false);
                return nullptr;
            }
            return fact(bytes);
        };

        const char* fontFiles[] = {"../../../test/assets/RobotoFlex.ttf",
                                   "../../../test/assets/Montserrat.ttf",
                                   "../../../test/assets/IBMPlexSansArabic-Regular.ttf"};
        // "../../../test/assets/NotoSansArabic-VariableFont_wdth,wght.ttf"};

        auto font0 = loader(fontFiles[0]);
        auto font1 = loader(fontFiles[1]);
        auto font2 = loader(fontFiles[2]);
        assert(font0);
        assert(font1);
        assert(font2);

        fallbackFonts.push_back(font2);

        rive::Font::Coord c1 = {'wght', 100.f}, c2 = {'wght', 800.f};

        FontTextRuns truns;

        // truns.push_back(
        //     append(&m_unichars, font0->makeAtCoord(c2), 32, "No one ever left alive in "));
        // truns.push_back(append(&m_unichars, font0->makeAtCoord(c2), 54, "nineteen hundred"));
        // truns.push_back(append(&m_unichars, font0->makeAtCoord(c1), 30, "ne漢字asy"));

        // truns.push_back(append(&m_unichars, font2, 30, " its the  "));
        // truns.push_back(append(&m_unichars, font2, 40, "cRown"));
        // truns.push_back(append(&m_unichars, font2, 30, "a b c d"));

        // truns.push_back(append(&m_unichars, font1->makeAtCoord(c1), 30, " that often"));
        // truns.push_back(append(&m_unichars, font0, 30, " lies the head."));
        // truns.push_back(append(&m_unichars, font0->makeAtCoord(c2), 60, "hi one two"));

        // truns.push_back(append(&m_unichars, font2, 32.0f, "في 10-12 آذار 1997 بمدينة"));
        // truns.push_back(append(&m_unichars, font2, 32.0f, "في 10-12 آذار 1997 بمدينة"));
        truns.push_back(append(&m_unichars,
                               font1,
                               32.0f,
                               21.0f,
                               // clang-format off
                               "this is some text and here is "));
        truns.push_back(append(&m_unichars,
                               font2,
                               32.0f,
                               21.0f,
                               // clang-format off
                               "some"));
        truns.push_back(append(&m_unichars,
                               font1,
                               32.0f,
                               21.0f,
                               // clang-format off
                               " more"));
                            //    "لمفاتيح ABC"));

        // truns.push_back(append(&m_unichars,
        //                        font1,
        //                        28.0f,
        //                        // clang-format off
        //                        " DEF\n"));

        // truns.push_back(append(&m_unichars,
        //                        font0,
        //                        32.0f,
        //                        // clang-format off
        //                        " one ever ff ffi left alive in "));

        // truns.push_back(append(&m_unichars,
        //                        font1,
        //                        54.0f,
        //                        // clang-format off
        //                        "nineteen hundred"));

        // truns.push_back(append(&m_unichars,
        //                        font0,
        //                        32.0f,
        //                        // clang-format off
        //                        "and eighty five"));
        // TODO: test case from flutter causing assertion break

        //    "لمفاتيح ABC DEF في 10-12 آذار 1997 بمدينة\nabc def ghi jkl mnop\nلكن لا بد أن أوضح لك
        //    أن كل"));
        // "hello look\u2028here\nsecond paragraph"));

        // clang-format on

        // truns.push_back(append(&m_unichars, font2, 32.0f, "abc def\nghijkl"));

        // truns.push_back(append(&m_unichars, font2, 32.0f, "DEF دنة"));

        // truns.push_back(append(&m_unichars, font2, 32.0f, "AفيB"));

        // truns.push_back(append(&m_unichars, font0, 42.0f, "OT\nHER\n"));
        // truns.push_back(append(&m_unichars, font1, 62.0f, "VERY LARGE FONT HERE"));
        // truns.push_back(
        //     append(&m_unichars, font0, 52.0f, "one two three\n\n\nfour five six seven"));

        // truns.push_back(append(&m_unichars, font0, 32.0f, "ab"));
        // truns.push_back(append(&m_unichars, font0, 60.0f, "ee\n four"));

        return truns;
    }

public:
    TextContent()
    {
        fallbackFonts.clear();
        rive::Font::gFallbackProc = pickFallbackFont;
        auto truns = this->make_truns(ViewerContent::DecodeFont);
        m_paragraphs = truns[0].font->shapeText(m_unichars, truns);

        m_xform = rive::Mat2D::fromTranslate(10, 0) * rive::Mat2D::fromScale(3, 3);
    }

    void draw(rive::Renderer* renderer,
              float width,
              const rive::SimpleArray<rive::Paragraph>& paragraphs)
    {

        renderer->save();
        renderer->transform(m_xform);
        float y = 0.0f;
        float paragraphWidth = m_autoWidth ? -1.0f : width;

        rive::SimpleArray<rive::SimpleArray<rive::GlyphLine>>* lines =
            new rive::SimpleArray<rive::SimpleArray<rive::GlyphLine>>(paragraphs.size());
        rive::SimpleArray<rive::SimpleArray<rive::GlyphLine>>& linesRef = *lines;
        size_t paragraphIndex = 0;

        for (auto& para : paragraphs)
        {
            linesRef[paragraphIndex] =
                rive::GlyphLine::BreakLines(para.runs, m_autoWidth ? -1.0f : width);

            if (m_autoWidth)
            {
                paragraphWidth =
                    std::max(paragraphWidth,
                             rive::GlyphLine::ComputeMaxWidth(linesRef[paragraphIndex], para.runs));
            }
            paragraphIndex++;
        }
        paragraphIndex = 0;
        for (auto& para : paragraphs)
        {
            rive::SimpleArray<rive::GlyphLine>& lines = linesRef[paragraphIndex];
            rive::GlyphLine::ComputeLineSpacing(paragraphIndex == 0,
                                                lines,
                                                para.runs,
                                                paragraphWidth,
                                                (rive::TextAlign)m_align);
            y = drawpara(RiveFactory(), renderer, para, lines, {0, y}) + 20.0f;
            paragraphIndex++;
        }
        if (!m_autoWidth)
        {
            draw_line(RiveFactory(), renderer, width);
        }

        renderer->restore();
    }

    void handleDraw(rive::Renderer* renderer, double) override
    {
        this->draw(renderer, m_width, m_paragraphs);
    }

    void handleResize(int width, int height) override {}
#ifndef RIVE_SKIP_IMGUI
    void handleImgui() override
    {
        const char* alignOptions[] = {"left", "right", "center"};
        ImGui::Begin("text", nullptr);
        ImGui::SliderFloat("Width", &m_width, 1, 400);
        ImGui::Checkbox("Autowidth", &m_autoWidth);
        ImGui::Combo("combo", &m_align, alignOptions, IM_ARRAYSIZE(alignOptions));
        ImGui::End();
    }
#endif
};

static bool ends_width(const char str[], const char suffix[])
{
    size_t ln = strlen(str);
    size_t lx = strlen(suffix);
    if (lx > ln)
    {
        return false;
    }
    for (size_t i = 0; i < lx; ++i)
    {
        if (str[ln - lx + i] != suffix[i])
        {
            return false;
        }
    }
    return true;
}

std::unique_ptr<ViewerContent> ViewerContent::Text(const char filename[])
{
    if (ends_width(filename, ".svg"))
    {
        return rivestd::make_unique<TextContent>();
    }
    return nullptr;
}
