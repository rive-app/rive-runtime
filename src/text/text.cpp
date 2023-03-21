#include "rive/text/text.hpp"
using namespace rive;
#ifdef WITH_RIVE_TEXT
#include "rive/text_engine.hpp"
#include "rive/component_dirt.hpp"
#include "rive/text/utf.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/shapes/paint/shape_paint.hpp"

GlyphItr& GlyphItr::operator++()
{
    m_glyphIndex += m_run->dir == TextDirection::ltr ? 1 : -1;

    // Did we reach the end of the run?
    if (m_glyphIndex == m_line->endGlyphIndex(m_run) && m_run != m_line->lastRun())
    {
        m_run++;
        m_glyphIndex = m_line->startGlyphIndex(m_run);
    }
    return *this;
}

OrderedLine::OrderedLine(const Paragraph& paragraph, const GlyphLine& line) : m_line(&line)
{
    std::vector<const GlyphRun*> logicalRuns;
    const SimpleArray<GlyphRun>& glyphRuns = paragraph.runs;

    for (uint32_t i = line.startRunIndex; i < line.endRunIndex + 1; i++)
    {
        logicalRuns.push_back(&glyphRuns[i]);
    }
    if (paragraph.baseDirection == TextDirection::ltr || logicalRuns.empty())
    {
        m_startLogical = logicalRuns.front();
        m_endLogical = logicalRuns.back();
        m_runs = std::move(logicalRuns);
    }
    else
    {
        std::vector<const GlyphRun*> visualRuns;
        visualRuns.reserve(logicalRuns.size());

        auto itr = logicalRuns.rbegin();
        auto end = logicalRuns.rend();
        const GlyphRun* first = *itr;
        visualRuns.push_back(first);
        size_t ltrIndex = 0;
        TextDirection previousDirection = first->dir;
        while (++itr != end)
        {
            const GlyphRun* run = *itr;
            if (run->dir == TextDirection::ltr && previousDirection == run->dir)
            {
                visualRuns.insert(visualRuns.begin() + ltrIndex, run);
            }
            else
            {
                if (run->dir == TextDirection::ltr)
                {
                    ltrIndex = visualRuns.size();
                }
                visualRuns.push_back(run);
            }
            previousDirection = run->dir;
        }
        m_runs = std::move(visualRuns);
    }
}

void Text::buildRenderStyles()
{
    for (TextStyle* style : m_renderStyles)
    {
        style->rewindPath();
    }
    m_renderStyles.clear();

    // Build up ordered runs as we go.
    int paragraphIndex = 0;
    int lineIndex = 0;
    for (const SimpleArray<GlyphLine>& paragraphLines : m_lines)
    {
        const Paragraph& paragraph = m_shape[paragraphIndex++];
        float y = 0.0f;
        for (const GlyphLine& line : paragraphLines)
        {
            if (lineIndex >= m_orderedLines.size())
            {
                // We need to still compute this line's ordered runs.
                m_orderedLines.emplace_back(OrderedLine(paragraph, line));
            }
            const OrderedLine& orderedLine = m_orderedLines[lineIndex];
            float x = 0.0f;
            float renderY = y + line.baseline;
            for (auto glyphItr : orderedLine)
            {
                const GlyphRun* run = std::get<0>(glyphItr);
                size_t glyphIndex = std::get<1>(glyphItr);
                const Font* font = run->font.get();
                GlyphID glyphId = run->glyphs[glyphIndex];

                // TODO: profile if this should be cached.
                RawPath path = font->getPath(glyphId);
                // If we do end up caching these, we'll want to not
                // transformInPlace and just transform instead.
                path.transformInPlace(Mat2D(run->size, 0.0f, 0.0f, run->size, x, renderY));
                x += run->advances[glyphIndex];

                assert(run->styleId < m_runs.size());
                TextStyle* style = m_runs[run->styleId]->style();
                // TextValueRun::onAddedDirty botches loading if it cannot
                // resolve a style, so we're confident we have a style here.
                assert(style != nullptr);
                if (style->addPath(path))
                {
                    // This was the first path added to the style, so let's mark
                    // it in our draw list.
                    m_renderStyles.push_back(style);
                    style->propagateOpacity(renderOpacity());
                }
            }
            lineIndex++;
        }
    }
}
void Text::draw(Renderer* renderer)
{
    if (!clip(renderer))
    {
        // We didn't clip, so make sure to save as we'll be doing some
        // transformations.
        renderer->save();
    }
    renderer->transform(worldTransform());
    for (auto style : m_renderStyles)
    {
        style->draw(renderer);
    }
    renderer->restore();
}

void Text::addRun(TextValueRun* run) { m_runs.push_back(run); }

void Text::markShapeDirty() { addDirt(ComponentDirt::Path); }

void Text::alignValueChanged() { markShapeDirty(); }

void Text::sizingValueChanged() { markShapeDirty(); }

void Text::overflowValueChanged()
{
    if (sizing() != TextSizing::autoWidth)
    {
        markShapeDirty();
    }
}

void Text::widthChanged()
{
    if (sizing() != TextSizing::autoWidth)
    {
        markShapeDirty();
    }
}

void Text::heightChanged()
{
    if (sizing() == TextSizing::fixed)
    {
        markShapeDirty();
    }
}

static rive::TextRun append(std::vector<Unichar>& unichars,
                            rcp<Font> font,
                            float size,
                            const std::string& text,
                            uint16_t styleId)
{
    const uint8_t* ptr = (const uint8_t*)text.c_str();
    uint32_t n = 0;
    while (*ptr)
    {
        unichars.push_back(UTF::NextUTF8(&ptr));
        n += 1;
    }
    return {std::move(font), size, n, 0, styleId};
}

static SimpleArray<SimpleArray<GlyphLine>> breakLines(const SimpleArray<Paragraph>& paragraphs,
                                                      float width,
                                                      TextAlign align)
{
    bool autoWidth = width == -1.0f;
    float paragraphWidth = width;

    SimpleArray<SimpleArray<GlyphLine>> lines(paragraphs.size());

    size_t paragraphIndex = 0;
    for (auto& para : paragraphs)
    {
        lines[paragraphIndex] = GlyphLine::BreakLines(para.runs, autoWidth ? -1.0f : width);
        if (autoWidth)
        {
            paragraphWidth = std::max(paragraphWidth,
                                      GlyphLine::ComputeMaxWidth(lines[paragraphIndex], para.runs));
        }
        paragraphIndex++;
    }
    paragraphIndex = 0;
    for (auto& para : paragraphs)
    {
        GlyphLine::ComputeLineSpacing(lines[paragraphIndex++], para.runs, paragraphWidth, align);
    }
    return lines;
}

void Text::update(ComponentDirt value)
{
    Super::update(value);

    if (hasDirt(value, ComponentDirt::Path))
    {
        std::vector<Unichar> unichars;
        std::vector<TextRun> runs;
        uint16_t runIndex = 0;
        for (auto valueRun : m_runs)
        {
            auto style = valueRun->style();
            const std::string& text = valueRun->text();
            if (style == nullptr || style->font() == nullptr || text.empty())
            {
                runIndex++;
                continue;
            }
            runs.emplace_back(append(unichars, style->font(), style->fontSize(), text, runIndex++));
        }
        if (!runs.empty())
        {
            m_shape = runs[0].font->shapeText(unichars, runs);
            m_lines = breakLines(m_shape,
                                 sizing() == TextSizing::autoWidth ? -1.0f : width(),
                                 (TextAlign)alignValue());

            m_orderedLines.clear();
        }
        // This could later be flagged as dirty and called only when actually
        // rendered the first time, for now we do it on update cycle in tandem
        // with shaping.
        buildRenderStyles();
    }
    else if (hasDirt(value, ComponentDirt::RenderOpacity))
    {
        // Note that buildRenderStyles does this too, which is why we can get
        // away doing this in the else.
        for (TextStyle* style : m_renderStyles)
        {
            style->propagateOpacity(renderOpacity());
        }
    }
}

Core* Text::hitTest(HitInfo*, const Mat2D&)
{
    if (renderOpacity() == 0.0f)
    {
        return nullptr;
    }

    return nullptr;
}

#else
// Text disabled.
void Text::draw(Renderer* renderer) {}
Core* Text::hitTest(HitInfo*, const Mat2D&) { return nullptr; }
void Text::addRun(TextValueRun* run) {}
void Text::markShapeDirty() {}
void Text::update(ComponentDirt value) {}
void Text::alignValueChanged() {}
void Text::sizingValueChanged() {}
void Text::overflowValueChanged() {}
void Text::widthChanged() {}
void Text::heightChanged() {}
#endif