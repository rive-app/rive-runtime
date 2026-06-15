#include "rive/text/text.hpp"
using namespace rive;
#ifdef WITH_RIVE_TEXT
#include "rive/text_engine.hpp"
#include "rive/component_dirt.hpp"
#include "rive/math/rectangles_to_contour.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text/text_modifier_group.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/clip_result.hpp"
#include "rive/generated/core_registry.hpp"
#include <limits>

TextValueRunProperty::TextValueRunProperty(
    Core* textValueRun,
    TextValueRunListener* textValueRunListener,
    ViewModelInstanceValue* instanceValue,
    uint16_t propertyKey,
    SymbolType symbolType) :
    PropertySymbolDependentSingle(textValueRun,
                                  textValueRunListener,
                                  instanceValue,
                                  propertyKey),
    m_symbolType(symbolType)
{}

void TextValueRunProperty::writeValue()
{
    switch (m_symbolType)
    {
        case SymbolType::textContent:
            CoreRegistry::setString(
                m_coreObject,
                m_propertyKey,
                m_instanceValue->as<ViewModelInstanceString>()
                    ->propertyValue());
            break;
        case SymbolType::textStyle:
        {
            auto stylePaints =
                static_cast<TextValueRunListener*>(m_coreObjectListener)
                    ->text()
                    ->textStylePaints();
            auto styleValue =
                m_instanceValue->as<ViewModelInstanceString>()->propertyValue();
            for (size_t i = 0; i < stylePaints.size(); i++)
            {
                auto stylePaint = stylePaints[i];
                if (stylePaint->name() == styleValue)
                {
                    m_coreObject->as<TextValueRun>()->style(stylePaint);
                    break;
                }
                else if (i == 0)
                {
                    m_coreObject->as<TextValueRun>()->style(stylePaint);
                }
            }
            break;
        }
        default:
            break;
    }
}

TextValueRunListener::TextValueRunListener(TextValueRun* textValueRun,
                                           rcp<ViewModelInstance> instance,
                                           Text* text) :
    CoreObjectListener(textValueRun, instance), m_text(text)
{
    createProperties();
}

void TextValueRunListener::markDirty() { m_text->markShapeDirty(); }

void TextValueRunListener::createProperties()
{
    createPropertyListener(SymbolType::textStyle);
    createPropertyListener(SymbolType::textContent);
}

TextValueRunProperty* TextValueRunListener::createSinglePropertyListener(
    SymbolType symbolType)
{
    uint16_t propertyKey = 0;
    switch (symbolType)
    {
        case SymbolType::textStyle:
            propertyKey = TextValueRunBase::styleIdPropertyKey;
            break;
        case SymbolType::textContent:
            propertyKey = TextValueRunBase::textPropertyKey;
            break;
        default:
            break;
    }
    auto prop = m_instance->propertyValue(symbolType);
    if (prop != nullptr && prop->is<ViewModelInstanceValue>())
    {
        auto vpl = new TextValueRunProperty(m_core,
                                            this,
                                            prop,
                                            propertyKey,
                                            symbolType);
        return vpl;
    }
    return nullptr;
}

void TextValueRunListener::createPropertyListener(SymbolType symbolType)
{
    TextValueRunProperty* listener = nullptr;
    switch (symbolType)
    {
        case SymbolType::textStyle:
        case SymbolType::textContent:
            listener = createSinglePropertyListener(symbolType);
            break;
        default:
            break;
    }
    if (listener != nullptr)
    {
        listener->writeValue();
        m_properties.push_back(listener);
    }
}

Text::~Text()
{
    for (auto& textValueRun : m_valueRunListeners)
    {
        delete textValueRun;
    }
}

Vec2D Text::measureLayout(float width,
                          LayoutMeasureMode widthMode,
                          float height,
                          LayoutMeasureMode heightMode)
{
    return measure(Vec2D(widthMode == LayoutMeasureMode::undefined
                             ? std::numeric_limits<float>::max()
                             : width,
                         heightMode == LayoutMeasureMode::undefined
                             ? std::numeric_limits<float>::max()
                             : height));
}

void Text::controlSize(Vec2D size,
                       LayoutScaleType widthScaleType,
                       LayoutScaleType heightScaleType,
                       LayoutDirection direction)
{
    if (m_layoutWidth != size.x || m_layoutHeight != size.y ||
        m_layoutWidthScaleType != (uint8_t)widthScaleType ||
        m_layoutHeightScaleType != (uint8_t)heightScaleType ||
        m_layoutDirection != direction)
    {
        m_layoutWidth = size.x;
        m_layoutHeight = size.y;
        m_layoutWidthScaleType = (uint8_t)widthScaleType;
        m_layoutHeightScaleType = (uint8_t)heightScaleType;
        m_layoutDirection = direction;
        markShapeDirty(false);
    }
}

TextSizing Text::effectiveSizing() const
{
    if (m_layoutWidthScaleType == std::numeric_limits<uint8_t>::max() ||
        m_layoutWidthScaleType == (uint8_t)LayoutScaleType::hug ||
        m_layoutHeightScaleType == (uint8_t)LayoutScaleType::hug)
    {
        return sizing();
    }
    return TextSizing::fixed;
}

void Text::clearRenderStyles()
{
    for (TextStylePaint* style : m_renderStyles)
    {
        style->rewindPath();
    }
    m_renderStyles.clear();
    m_drawCommands.clear();

    for (TextValueRun* textValueRun : m_allRuns)
    {
        textValueRun->resetHitTest();
    }
}

// Vertical trim: the ascent above the first line's cap- or x-height and the
// descent below the last line's baseline (or natural descent) that should be
// removed from the box. Glyph layout is unchanged. Shared by computeBoundsInfo
// (rendering) and measure (layout), so it operates on supplied lines/shape.
static void computeVerticalTrim(
    const SimpleArray<SimpleArray<GlyphLine>>& lines,
    const SimpleArray<Paragraph>& shape,
    TextTrimTop trimTop,
    TextTrimBottom trimBottom,
    float& topTrim,
    float& bottomTrim)
{
    topTrim = 0.0f;
    bottomTrim = 0.0f;
    if (lines.empty() ||
        (trimTop == TextTrimTop::none && trimBottom == TextTrimBottom::none))
    {
        return;
    }

    if (trimTop != TextTrimTop::none && !lines.front().empty())
    {
        const GlyphLine& firstLine = lines.front().front();
        const Paragraph& firstParagraph = shape[0];
        // Largest top edge across the runs on the first line so that no glyph
        // is clipped. capHeight/xHeight are stored negative (up is -Y).
        float edgePx = 0.0f;
        for (uint32_t i = firstLine.startRunIndex; i <= firstLine.endRunIndex;
             ++i)
        {
            const Font::LineMetrics& metrics =
                firstParagraph.runs[i].font->lineMetrics();
            float edge = trimTop == TextTrimTop::cap ? metrics.capHeight
                                                     : metrics.xHeight;
            edgePx = std::max(edgePx, -edge * firstParagraph.runs[i].size);
        }
        topTrim = std::max(0.0f, (firstLine.baseline - edgePx) - firstLine.top);
    }

    // Bottom trim uses the last non-empty paragraph's last line.
    if (trimBottom != TextTrimBottom::none)
    {
        for (size_t p = lines.size(); p-- > 0;)
        {
            const SimpleArray<GlyphLine>& paragraphLines = lines[p];
            if (paragraphLines.empty())
            {
                continue;
            }
            const GlyphLine& lastLine = paragraphLines.back();
            float descentBand = lastLine.bottom - lastLine.baseline;
            if (trimBottom == TextTrimBottom::alphabetic)
            {
                // Box bottom at the baseline: remove the whole descent.
                bottomTrim = std::max(0.0f, descentBand);
            }
            else
            {
                // Box bottom at the natural descent: keep the descenders,
                // remove only any extra leading below them.
                const Paragraph& lastParagraph = shape[p];
                float descentPx = 0.0f;
                for (uint32_t i = lastLine.startRunIndex;
                     i <= lastLine.endRunIndex;
                     ++i)
                {
                    const GlyphRun& run = lastParagraph.runs[i];
                    descentPx =
                        std::max(descentPx,
                                 run.font->lineMetrics().descent * run.size);
                }
                bottomTrim = std::max(0.0f, descentBand - descentPx);
            }
            break;
        }
    }
}

TextBoundsInfo Text::computeBoundsInfo()
{
    const float paragraphSpace = paragraphSpacing();

    // Build up ordered runs as we go.
    int paragraphIndex = 0;
    float y = 0.0f;
    float minY = 0.0f;
    float maxWidth = 0.0f;
    float ellipsedHeight = 0;
    if (textOrigin() == TextOrigin::baseline && !m_lines.empty() &&
        !m_lines[0].empty())
    {
        y -= m_lines[0][0].baseline;
        minY = y;
    }

    int ellipsisLine = -1;
    bool isEllipsisLineLast = false;
    // Find the line to put the ellipsis on (line before the one that
    // overflows).
    bool wantEllipsis = overflow() == TextOverflow::ellipsis &&
                        effectiveSizing() == TextSizing::fixed;

    int lastLineIndex = -1;
    for (const SimpleArray<GlyphLine>& paragraphLines : m_lines)
    {
        const Paragraph& paragraph = m_shape[paragraphIndex++];
        for (const GlyphLine& line : paragraphLines)
        {
            const GlyphRun& endRun = paragraph.runs[line.endRunIndex];
            const GlyphRun& startRun = paragraph.runs[line.startRunIndex];
            float width = endRun.xpos[line.endGlyphIndex] -
                          startRun.xpos[line.startGlyphIndex];
            if (width > maxWidth)
            {
                maxWidth = width;
            }
            lastLineIndex++;
            if (wantEllipsis && y + line.bottom <= effectiveHeight())
            {
                ellipsedHeight = y + line.bottom;
                ellipsisLine++;
            }
        }

        if (!paragraphLines.empty())
        {
            y += paragraphLines.back().bottom;
        }
        y += paragraphSpace;
    }
    if (wantEllipsis && ellipsisLine == -1)
    {
        // Nothing fits, just show the first line and ellipse it.
        ellipsisLine = 0;
    }
    auto totalHeight = ellipsisLine > 0 ? ellipsedHeight : y;
    isEllipsisLineLast = lastLineIndex == ellipsisLine;

    // Vertical trim: shrink the box to the chosen cap/x-height + baseline band.
    // Glyph layout is unchanged; fixed sizing keeps its authored box.
    float topTrim = 0.0f;
    float bottomTrim = 0.0f;
    if (effectiveSizing() != TextSizing::fixed)
    {
        computeVerticalTrim(m_lines,
                            m_shape,
                            verticalTrimTop(),
                            verticalTrimBottom(),
                            topTrim,
                            bottomTrim);
    }

    return {
        minY,
        maxWidth,
        totalHeight,
        ellipsisLine,
        isEllipsisLineLast,
        topTrim,
        bottomTrim,
    };
}

float Text::fitFontScale()
{
    // Largest authored font size across runs is our maximum; we search integer
    // sizes in [1, maxSize]. Scaling all runs by a single multiplier preserves
    // their relative proportions while stepping the largest run by integers.
    float maxSize = 0.0f;
    for (TextValueRun* valueRun : m_allRuns)
    {
        TextStylePaint* style = valueRun->style();
        if (style != nullptr && style->font() != nullptr &&
            !valueRun->text().empty())
        {
            maxSize = std::max(maxSize, style->fontSize());
        }
    }

    const TextSizing sizing = effectiveSizing();
    // Without a fixed dimension to fit into there is nothing to search:
    // autoWidth grows in both directions, so we keep the authored size.
    if (maxSize <= 1.0f || sizing == TextSizing::autoWidth)
    {
        return 1.0f;
    }

    const float boxWidth = effectiveWidth();
    const float boxHeight = effectiveHeight();
    const float paragraphSpace = paragraphSpacing();

    StyledText styledText;

    // Shapes and lays out the text at the given integer top size and reports
    // whether it fits the bounds. Text dimensions grow monotonically with size,
    // so this predicate is monotonic and binary-searchable.
    auto fits = [&](int topSize) -> bool {
        float scale = (float)topSize / maxSize;
        if (!makeStyled(styledText, true, scale))
        {
            // Nothing to lay out: trivially fits.
            return true;
        }
        auto runs = styledText.runs();
        auto shape = runs[0].font->shapeText(styledText.unichars(), runs);
        auto lines = BreakLines(shape, boxWidth, align(), wrap());

        float maxWidth = 0.0f;
        float y = 0.0f;
        size_t paragraphIndex = 0;
        for (const SimpleArray<GlyphLine>& paragraphLines : lines)
        {
            const Paragraph& paragraph = shape[paragraphIndex++];
            for (const GlyphLine& line : paragraphLines)
            {
                const GlyphRun& endRun = paragraph.runs[line.endRunIndex];
                const GlyphRun& startRun = paragraph.runs[line.startRunIndex];
                float width = endRun.xpos[line.endGlyphIndex] -
                              startRun.xpos[line.startGlyphIndex];
                maxWidth = std::max(maxWidth, width);
            }
            if (!paragraphLines.empty())
            {
                y += paragraphLines.back().bottom;
            }
            y += paragraphSpace;
        }

        bool widthFits = maxWidth <= boxWidth;
        bool heightFits = sizing == TextSizing::fixed ? (y <= boxHeight) : true;
        return widthFits && heightFits;
    };

    // Binary search for the largest integer top size in [1, floor(maxSize)]
    // that fits. If nothing fits we fall back to the minimum size (1).
    int lo = 1;
    int hi = std::max(1, (int)maxSize);
    int best = 1;
    while (lo <= hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (fits(mid))
        {
            best = mid;
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }
    return (float)best / maxSize;
}

LineIter Text::shouldDrawLine(float curY,
                              float totalHeight,
                              const GlyphLine& line)
{
    switch (overflow())
    {
        case TextOverflow::hidden:
            if (effectiveSizing() == TextSizing::fixed)
            {
                switch (verticalAlign())
                {
                    case VerticalTextAlign::top:
                        if (curY + line.bottom > effectiveHeight())
                        {
                            return LineIter::yOutOfBounds;
                        }
                        break;
                    case VerticalTextAlign::middle:
                        if (curY + line.top <
                            totalHeight / 2 - effectiveHeight() / 2)
                        {
                            return LineIter::skipThisLine;
                        }
                        if (curY + line.bottom >
                            totalHeight / 2 + effectiveHeight() / 2)
                        {
                            return LineIter::yOutOfBounds;
                        }
                        break;
                    case VerticalTextAlign::bottom:
                        if (curY + line.top < totalHeight - effectiveHeight())
                        {
                            return LineIter::skipThisLine;
                        }
                        break;
                }
            }
            break;
        case TextOverflow::clipped:
            if (effectiveSizing() == TextSizing::fixed)
            {
                switch (verticalAlign())
                {
                    case VerticalTextAlign::top:
                        if (curY + line.top > effectiveHeight())
                        {
                            return LineIter::yOutOfBounds;
                        }
                        break;
                    case VerticalTextAlign::middle:
                        if (curY + line.bottom <
                            totalHeight / 2 - effectiveHeight() / 2)
                        {
                            return LineIter::skipThisLine;
                        }
                        if (curY + line.top >
                            totalHeight / 2 + effectiveHeight() / 2)
                        {
                            return LineIter::yOutOfBounds;
                        }
                        break;
                    case VerticalTextAlign::bottom:
                        if (curY + line.bottom <
                            totalHeight - effectiveHeight())
                        {
                            return LineIter::skipThisLine;
                        }
                        break;
                }
            }
            break;
        default:
            break;
    }
    return LineIter::drawLine;
}

void Text::buildRenderStyles()
{
    // Step 1: reset stuff
    clearRenderStyles();

    if (m_shape.empty())
    {
        m_bounds = AABB(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    // Step 2: compute ellipsis information
    TextBoundsInfo info = computeBoundsInfo();
    float minY = info.minY;
    float maxWidth = info.maxWidth;
    float totalHeight = info.totalHeight;
    int ellipsisLine = info.ellipsisLine;
    bool isEllipsisLineLast = info.isEllipsisLineLast;
    float topTrim = info.topTrim;
    float bottomTrim = info.bottomTrim;

    // Step 3: update modifiers
    bool hasModifiers = haveModifiers();
    if (hasModifiers)
    {
        uint32_t textSize = (uint32_t)m_styledText.unichars().size();
        for (TextModifierGroup* modifierGroup : m_modifierGroups)
        {
            modifierGroup->computeCoverage(textSize);
            modifierGroup->resetTextFollowPath();
        }
    }

    // Step 4: update bounds
    const float paragraphSpace = paragraphSpacing();
    switch (effectiveSizing())
    {
        case TextSizing::autoWidth:
            m_bounds = AABB(
                0.0f,
                minY,
                maxWidth,
                std::max(minY,
                         totalHeight - paragraphSpace - topTrim - bottomTrim));
            break;
        case TextSizing::autoHeight:
            m_bounds = AABB(
                0.0f,
                minY,
                effectiveWidth(),
                std::max(minY,
                         totalHeight - paragraphSpace - topTrim - bottomTrim));
            break;
        case TextSizing::fixed:
            m_bounds =
                AABB(0.0f, minY, effectiveWidth(), minY + effectiveHeight());
            break;
    }

    auto verticalAlignOffset = 0.0f;
    switch (verticalAlign())
    {
        case VerticalTextAlign::middle:
            verticalAlignOffset = (totalHeight - m_bounds.height()) / 2;
            break;
        case VerticalTextAlign::bottom:
            verticalAlignOffset = totalHeight - m_bounds.height();
            break;
        default:
            break;
    }

    // Step 5: Update clip information (if we want it)
    if (overflow() == TextOverflow::clipped)
    {
        m_clipRect.rewind();

        AABB bounds = localBounds();

        float minX = bounds.minX + bounds.width() * originX();
        float minY =
            bounds.minY + bounds.height() * originY() + verticalAlignOffset;
        m_clipRect.addRect(
            AABB(minX, minY, minX + bounds.width(), minY + bounds.height()));
    }

    // Step 6: add the glyphs to render paths. Shift content up by topTrim so
    // the first line's cap-height line aligns with the (trimmed) box top.
    float curY = minY - topTrim;
    int lineIndex = 0;
    int paragraphIndex = 0;
    float minX = std::numeric_limits<float>::max();

    for (const SimpleArray<GlyphLine>& paragraphLines : m_lines)
    {
        const Paragraph& paragraph = m_shape[paragraphIndex++];
        int lineIndexInParagraph = 0;
        for (const GlyphLine& line : paragraphLines)
        {
            LineIter lineIter = shouldDrawLine(curY, totalHeight, line);
            if (lineIter == LineIter::yOutOfBounds)
            {
                goto skipLines;
            }
            else if (lineIter == LineIter::skipThisLine)
            {
                lineIndexInParagraph++;
                lineIndex++;
                continue;
            }

            float renderY = curY + line.baseline;
            // We need to compute this line's ordered runs.
            m_orderedLines.emplace_back(OrderedLine(paragraph,
                                                    line,
                                                    effectiveWidth(),
                                                    ellipsisLine == lineIndex,
                                                    isEllipsisLineLast,
                                                    &m_ellipsisRun,
                                                    renderY));

            float curX = line.startX;
            minX = std::min(curX, minX);
            for (auto glyphItr : m_orderedLines.back())
            {
                const GlyphRun* run = std::get<0>(glyphItr);
                size_t glyphIndex = std::get<1>(glyphItr);

                const Font* font = run->font.get();
                const Vec2D& offset = run->offsets[glyphIndex];

                GlyphID glyphId = run->glyphs[glyphIndex];
                float advance = run->advances[glyphIndex];

                // Step 6.1: translate to the glyph's origin and scale.
                Vec2D curPos(curX, renderY);
                float centerX = advance / 2.0f;
                TransformComponents tc;
                tc.scaleX(run->size);
                tc.scaleY(run->size);
                tc.x(-centerX);
                Mat2D pathTransform = Mat2D::compose(tc);

                // Step 6.2: apply modifiers on a font-sized glyph
                float opacity = 1.0f;
                if (hasModifiers)
                {
                    uint32_t textIndex = run->textIndices[glyphIndex];
                    uint32_t glyphCount = m_glyphLookup.count(textIndex);

                    for (TextModifierGroup* modifierGroup : m_modifierGroups)
                    {
                        float coverage =
                            modifierGroup->glyphCoverage(textIndex, glyphCount);
                        TransformGlyphArg arg = {
                            curPos,
                            centerX,
                            lineIndexInParagraph,
                            paragraphLines,
                        };
                        modifierGroup->transform(coverage, pathTransform, arg);
                        if (modifierGroup->modifiesOpacity())
                        {
                            opacity = modifierGroup->computeOpacity(opacity,
                                                                    coverage);
                        }
                    }
                }

                // Step 6.3: translate back to center with offset
                pathTransform =
                    Mat2D::fromTranslate(curPos.x + centerX + offset.x,
                                         curPos.y + offset.y) *
                    pathTransform;

                assert(run->styleId < m_allRuns.size());
                TextValueRun* textValueRun = m_allRuns[run->styleId];
                TextStylePaint* style = textValueRun->style();
                assert(style != nullptr);

                // Check for color glyph (emoji) -- draw individually
                // with per-layer colors instead of accumulating.
                if (font->isColorGlyph(glyphId))
                {
                    TextDrawCommand cmd;
                    cmd.type = TextDrawCommand::kColorGlyph;
                    cmd.colorGlyph = {run->font,
                                      glyphId,
                                      pathTransform,
                                      style->foregroundColor(),
                                      opacity};
                    m_drawCommands.push_back(std::move(cmd));
                }
                else
                {
                    RawPath path = font->getPath(glyphId);
                    path.transformInPlace(pathTransform);

                    if (style->addPath(path, opacity))
                    {
                        // This was the first path added to the style, so
                        // let's mark it in our draw list.
                        m_renderStyles.push_back(style);
                        style->propagateOpacity(renderOpacity());

                        TextDrawCommand cmd;
                        cmd.type = TextDrawCommand::kStylePath;
                        cmd.style = style;
                        m_drawCommands.push_back(std::move(cmd));
                    }
                }

                // Bounds of the glyph
                if (textValueRun->isHitTarget())
                {
                    Vec2D topLeft = Vec2D(curX, curY + line.top);
                    Vec2D bottomRight =
                        Vec2D(curX + advance, curY + line.bottom);
                    textValueRun->addHitRect(AABB(topLeft.x,
                                                  topLeft.y,
                                                  bottomRight.x,
                                                  bottomRight.y));
                }
                curX += advance;
            }
            if (lineIndex == ellipsisLine)
            {
                goto skipLines;
            }
            lineIndexInParagraph++;
            lineIndex++;
        }
        if (!paragraphLines.empty())
        {
            curY += paragraphLines.back().bottom;
        }
        curY += paragraphSpacing();
    }
skipLines:
    // Step 7: consider fit mode, and update local transform
    auto scale = 1.0f;
    auto xOffset = -m_bounds.width() * originX();
    auto yOffset = -m_bounds.height() * originY();
    if (overflow() == TextOverflow::fit)
    {
        auto xScale = (effectiveSizing() != TextSizing::autoWidth &&
                       maxWidth > m_bounds.width())
                          ? m_bounds.width() / maxWidth
                          : 1;
        auto baseline = fitFromBaseline() ? m_lines[0][0].baseline : 0;
        auto yScale =
            (effectiveSizing() == TextSizing::fixed &&
             totalHeight > m_bounds.height())
                ? (m_bounds.height() - baseline) / (totalHeight - baseline)
                : 1;
        if (xScale != 1 || yScale != 1)
        {
            scale = std::max(0.0f, xScale > yScale ? yScale : xScale);
            yOffset += baseline * (1 - scale);
            switch (align())
            {
                case TextAlign::center:
                    xOffset += (m_bounds.width() - maxWidth * scale) / 2 -
                               minX * scale;
                    break;
                case TextAlign::right:
                    xOffset +=
                        m_bounds.width() - maxWidth * scale - minX * scale;
                    break;
                default:
                    break;
            }
        }
    }
    if (verticalAlign() != VerticalTextAlign::top)
    {
        if (effectiveSizing() == TextSizing::fixed)
        {
            yOffset = -m_bounds.height() * originY();
            if (verticalAlign() == VerticalTextAlign::middle)
            {
                yOffset += (m_bounds.height() - totalHeight * scale) / 2;
            }
            else if (verticalAlign() == VerticalTextAlign::bottom)
            {
                yOffset += m_bounds.height() - totalHeight * scale;
            }
        }
    }
    m_transform =
        Mat2D::fromScaleAndTranslation(scale, scale, xOffset, yOffset);
#ifdef WITH_RIVE_LAYOUT
    markLayoutNodeDirty();
#endif

    // Step 8: cleanup
    for (TextValueRun* textValueRun : m_allRuns)
    {
        if (textValueRun->isHitTarget())
        {
            textValueRun->computeHitContours();
        }
    }
}

const TextStylePaint* Text::styleFromShaperId(uint16_t id) const
{
    assert(id < m_runs.size());
    return m_runs[id]->style();
}

void Text::draw(Renderer* renderer)
{
    if (m_needsSaveOperation)
    {
        renderer->save();
    }
    // For now we need to check both empty() and hasRenderPath() in
    // ShapePaintPath because the raw path gets cleared when the render path
    // is created.
    if (overflow() == TextOverflow::clipped &&
        (!m_clipPath.empty() || m_clipPath.hasRenderPath()))
    {
        renderer->clipPath(m_clipPath.renderPath(this));
    }
    auto worldTransform = shapeWorldTransform();
    for (auto& cmd : m_drawCommands)
    {
        if (cmd.type == TextDrawCommand::kStylePath)
        {
            cmd.style->draw(renderer, worldTransform);
        }
        else
        {
            drawColorGlyph(renderer, cmd.colorGlyph, worldTransform);
        }
    }
    if (m_needsSaveOperation)
    {
        renderer->restore();
    }
}

void Text::drawColorGlyph(Renderer* renderer,
                          const TextDrawCommand::ColorGlyphInfo& info,
                          const Mat2D& worldTransform)
{
    std::vector<Font::ColorGlyphLayer> layers;
    size_t count =
        info.font->getColorLayers(info.glyphId, layers, info.foregroundColor);
    if (count == 0)
    {
        return;
    }

    Factory* factory = artboard()->factory();
    renderer->save();
    renderer->transform(worldTransform * info.transform);

    for (auto& layer : layers)
    {
        if (layer.paintType == Font::ColorGlyphPaintType::image)
        {
            // Decode and draw bitmap emoji (SBIX/CBDT).
            ColorGlyphCacheKey cacheKey{info.font.get(), info.glyphId};
            auto it = m_emojiImageCache.find(cacheKey);
            if (it == m_emojiImageCache.end())
            {
                auto image = factory->decodeImage(
                    {layer.imageBytes.data(), layer.imageBytes.size()});
                it =
                    m_emojiImageCache.emplace(cacheKey, std::move(image)).first;
            }
            if (it->second != nullptr)
            {
                renderer->save();
                // Transform from glyph space: position at bearing and
                // scale from image pixels to glyph extent units.
                float scaleX = layer.imageExtentX / (float)layer.imageWidth;
                float scaleY = layer.imageExtentY / (float)layer.imageHeight;
                renderer->transform(Mat2D(scaleX,
                                          0,
                                          0,
                                          scaleY,
                                          layer.imageBearingX,
                                          layer.imageBearingY));
                renderer->drawImage(it->second.get(),
                                    ImageSampler::LinearClamp(),
                                    BlendMode::srcOver,
                                    info.opacity);
                renderer->restore();
            }
        }
        else
        {
            auto renderPath =
                factory->makeRenderPath(layer.path, FillRule::nonZero);
            auto renderPaint = factory->makeRenderPaint();
            renderPaint->style(RenderPaintStyle::fill);
            renderPaint->color(colorModulateOpacity(layer.color, info.opacity));
            renderer->drawPath(renderPath.get(), renderPaint.get());
        }
    }
    renderer->restore();
}

void Text::addRun(TextValueRun* run)
{
    m_runs.push_back(run);
    m_allRuns.push_back(run);
}

void Text::addModifierGroup(TextModifierGroup* group)
{
    m_modifierGroups.push_back(group);
}

void Text::markShapeDirty() { markShapeDirty(true); }

void Text::markShapeDirty(bool sendToLayout)
{
    addDirt(ComponentDirt::Path);
    for (TextModifierGroup* group : m_modifierGroups)
    {
        group->clearRangeMaps();
    }
    markWorldTransformDirty();
#ifdef WITH_RIVE_LAYOUT
    if (sendToLayout)
    {
        markLayoutNodeDirty();
    }
#endif
}

void Text::modifierShapeDirty() { addDirt(ComponentDirt::Path); }

void Text::markPaintDirty() { addDirt(ComponentDirt::Paint); }

void Text::alignValueChanged() { markShapeDirty(); }

void Text::sizingValueChanged() { markShapeDirty(); }

void Text::overflowValueChanged()
{
    if (effectiveSizing() != TextSizing::autoWidth)
    {
        markShapeDirty();
    }
}

void Text::widthChanged()
{
    if (effectiveSizing() != TextSizing::autoWidth)
    {
        markShapeDirty();
    }
}

void Text::paragraphSpacingChanged() { markPaintDirty(); }

void Text::heightChanged()
{
    if (effectiveSizing() == TextSizing::fixed)
    {
        markShapeDirty();
    }
}

void StyledText::clear()
{
    m_value.clear();
    m_runs.clear();
}

bool StyledText::empty() const { return m_runs.empty(); }

void StyledText::append(rcp<Font> font,
                        float size,
                        float lineHeight,
                        float letterSpacing,
                        const std::string& text,
                        uint16_t styleId)
{
    const uint8_t* ptr = (const uint8_t*)text.c_str();
    uint32_t n = 0;
    while (*ptr)
    {
        m_value.push_back(UTF::NextUTF8(&ptr));
        n += 1;
    }
    m_runs.push_back(
        {std::move(font), size, lineHeight, letterSpacing, n, 0, styleId});
}

bool Text::makeStyled(StyledText& styledText,
                      bool withModifiers,
                      float fontScale) const
{
    styledText.clear();
    uint16_t runIndex = 0;
    for (auto valueRun : m_allRuns)
    {
        auto style = valueRun->style();
        const std::string& text = valueRun->text();
        if (style == nullptr || style->font() == nullptr || text.empty())
        {
            runIndex++;
            continue;
        }
        styledText.append(style->font(),
                          style->fontSize() * fontScale,
                          style->lineHeight(),
                          style->letterSpacing(),
                          text,
                          runIndex++);
    }
    if (withModifiers)
    {
        for (TextModifierGroup* group : m_modifierGroups)
        {
            group->applyShapeModifiers(*this, styledText);
        }
    }
    return !styledText.empty();
}

SimpleArray<SimpleArray<GlyphLine>> Text::BreakLines(
    const SimpleArray<Paragraph>& paragraphs,
    float width,
    TextAlign align,
    TextWrap wrap)
{
    bool autoWidth = width == -1.0f;
    float paragraphWidth = width;

    SimpleArray<SimpleArray<GlyphLine>> lines(paragraphs.size());

    size_t paragraphIndex = 0;
    for (auto& para : paragraphs)
    {
        lines[paragraphIndex] = GlyphLine::BreakLines(
            para.runs,
            (autoWidth || wrap == TextWrap::noWrap) ? -1.0f : width);
        if (autoWidth)
        {
            paragraphWidth = std::max(
                paragraphWidth,
                GlyphLine::ComputeMaxWidth(lines[paragraphIndex], para.runs));
        }
        paragraphIndex++;
    }
    paragraphIndex = 0;
    for (auto& para : paragraphs)
    {
        GlyphLine::ComputeLineSpacing(paragraphIndex == 0,
                                      lines[paragraphIndex],
                                      para.runs,
                                      paragraphWidth,
                                      align);
        paragraphIndex++;
    }
    return lines;
}

bool Text::modifierRangesNeedShape() const
{
    for (const TextModifierGroup* modifierGroup : m_modifierGroups)
    {
        if (modifierGroup->needsShape())
        {
            return true;
        }
    }
    return false;
}

void Text::onDirty(ComponentDirt value)
{
    // Sometimes a WorldTransform dirt may also affect Path
    if (hasDirt(value, ComponentDirt::WorldTransform))
    {
        for (TextModifierGroup* modifierGroup : m_modifierGroups)
        {
            modifierGroup->onTextWorldTransformDirty();
        }
    }
    if (hasDirt(value, ComponentDirt::Path | ComponentDirt::Paint))
    {
        for (TextStylePaint* style : m_renderStyles)
        {
            style->invalidateStrokeEffects();
        }
    }
}

void Text::update(ComponentDirt value)
{
    Super::update(value);

    if (hasDirt(value, ComponentDirt::Path))
    {
        // We have modifiers that need shaping we'll need to compute the
        // coverage right before we build the actual shape.
        bool precomputeModifierCoverage = modifierRangesNeedShape();
        bool parentIsLayoutNotArtboard =
            parent()->is<LayoutComponent>() && !parent()->is<Artboard>();
        // For fitFontSize, find the largest integer font size that fits the
        // bounds, then shape/lay out the text at that size below.
        float fontScale =
            overflow() == TextOverflow::fitFontSize ? fitFontScale() : 1.0f;
        if (precomputeModifierCoverage &&
            makeStyled(m_modifierStyledText, false, fontScale))
        {
            auto runs = m_modifierStyledText.runs();
            m_modifierShape =
                runs[0].font->shapeText(m_modifierStyledText.unichars(), runs);
            m_modifierLines =
                BreakLines(m_modifierShape,
                           (effectiveSizing() == TextSizing::autoWidth &&
                            !parentIsLayoutNotArtboard)
                               ? -1.0f
                               : effectiveWidth(),
                           align(),
                           wrap());
            m_glyphLookup.compute(m_modifierStyledText.unichars(),
                                  m_modifierShape);
            uint32_t textSize =
                (uint32_t)m_modifierStyledText.unichars().size();
            for (TextModifierGroup* group : m_modifierGroups)
            {
                group->computeRangeMap(m_modifierStyledText.unichars(),
                                       m_modifierShape,
                                       m_modifierLines,
                                       m_glyphLookup);
                group->computeCoverage(textSize);
            }
        }
        if (makeStyled(m_styledText, true, fontScale))
        {
            auto runs = m_styledText.runs();
            m_shape = runs[0].font->shapeText(m_styledText.unichars(), runs);

            m_lines = BreakLines(m_shape,
                                 (effectiveSizing() == TextSizing::autoWidth &&
                                  !parentIsLayoutNotArtboard)
                                     ? -1.0f
                                     : effectiveWidth(),
                                 align(),
                                 wrap());
            if (!precomputeModifierCoverage && haveModifiers())
            {
                m_glyphLookup.compute(m_styledText.unichars(), m_shape);
                uint32_t textSize = (uint32_t)m_styledText.unichars().size();
                for (TextModifierGroup* group : m_modifierGroups)
                {
                    group->computeRangeMap(m_styledText.unichars(),
                                           m_shape,
                                           m_lines,
                                           m_glyphLookup);
                    group->computeCoverage(textSize);
                }
            }
        }
        else
        {
            m_shape = SimpleArray<Paragraph>();
            m_lines = SimpleArray<SimpleArray<GlyphLine>>();
            m_glyphLookup.clear();
        }
        m_orderedLines.clear();
        m_ellipsisRun = {};
        m_emojiImageCache.clear();

        // Immediately build render styles so dimensions get computed.
        buildRenderStyles();
    }
    else if (hasDirt(value, ComponentDirt::Paint))
    {
        buildRenderStyles();
    }
    else if (hasDirt(value, ComponentDirt::RenderOpacity))
    {
        // Note that buildRenderStyles does this too, which is why we can get
        // away doing this in the else.
        for (TextStylePaint* style : m_renderStyles)
        {
            style->propagateOpacity(renderOpacity());
        }
    }

    if (hasDirt(value,
                ComponentDirt::WorldTransform | ComponentDirt::Path |
                    ComponentDirt::Paint))
    {
        m_clipPath.rewind();
        m_shapeWorldTransform = m_WorldTransform * m_transform;
        m_clipPath.addPath(m_clipRect, &m_shapeWorldTransform);
    }
}

Vec2D Text::measure(Vec2D maxSize)
{
    if (makeStyled(m_styledText))
    {
        const float paragraphSpace = paragraphSpacing();
        auto runs = m_styledText.runs();
        auto shape = runs[0].font->shapeText(m_styledText.unichars(), runs);
        auto measuringWidth = 0.0f;
        switch (effectiveSizing())
        {
            case TextSizing::autoHeight:
            case TextSizing::fixed:
                measuringWidth = width();
                break;
            default:
                measuringWidth = std::numeric_limits<float>::max();
                break;
        }
        auto measuringWrap = maxSize.x == std::numeric_limits<float>::max() &&
                                     effectiveSizing() != TextSizing::autoHeight
                                 ? TextWrap::noWrap
                                 : wrap();
        auto lines = BreakLines(shape,
                                std::min(maxSize.x, measuringWidth),
                                align(),
                                measuringWrap);
        float y = 0;
        float computedHeight = 0.0f;
        float minY = 0;
        int paragraphIndex = 0;
        float maxWidth = 0;

        if (textOrigin() == TextOrigin::baseline && !lines.empty() &&
            !lines[0].empty())
        {
            y -= lines[0][0].baseline;
            minY = y;
        }
        int ellipsisLine = -1;
        bool wantEllipsis = overflow() == TextOverflow::ellipsis &&
                            sizing() == TextSizing::fixed;

        for (const SimpleArray<GlyphLine>& paragraphLines : lines)
        {
            const Paragraph& paragraph = shape[paragraphIndex++];
            for (const GlyphLine& line : paragraphLines)
            {
                const GlyphRun& endRun = paragraph.runs[line.endRunIndex];
                const GlyphRun& startRun = paragraph.runs[line.startRunIndex];
                float width = endRun.xpos[line.endGlyphIndex] -
                              startRun.xpos[line.startGlyphIndex];
                if (width > maxWidth)
                {
                    maxWidth = width;
                }
                if (wantEllipsis && y + line.bottom > maxSize.y)
                {
                    if (ellipsisLine == -1)
                    {
                        // Nothing fits, just show the first line and ellipse
                        // it.
                        computedHeight = y + line.bottom;
                    }
                    goto doneMeasuring;
                }
                ellipsisLine++;
                computedHeight = y + line.bottom;
            }
            if (!paragraphLines.empty())
            {
                y += paragraphLines.back().bottom;
            }
            y += paragraphSpace;
        }
    doneMeasuring:
        // Match the rendered box: trim the ascent/descent band for auto sizing.
        float topTrim = 0.0f;
        float bottomTrim = 0.0f;
        computeVerticalTrim(lines,
                            shape,
                            verticalTrimTop(),
                            verticalTrimBottom(),
                            topTrim,
                            bottomTrim);
        Vec2D bounds;
        switch (sizing())
        {
            case TextSizing::autoWidth:
                bounds = Vec2D(
                    maxWidth,
                    std::max(minY, computedHeight - topTrim - bottomTrim));
                break;
            case TextSizing::autoHeight:
                bounds = Vec2D(
                    width(),
                    std::max(minY, computedHeight - topTrim - bottomTrim));
                break;
            case TextSizing::fixed:
                bounds = Vec2D(width(), minY + height());
                break;
        }
        return Vec2D(std::min(maxSize.x, bounds.x),
                     std::min(maxSize.y, bounds.y));
    }
    return Vec2D();
}

AABB Text::localBounds() const
{
    float width = m_bounds.width();
    float height = m_bounds.height();
    return AABB::fromLTWH(m_bounds.minX - width * originX(),
                          m_bounds.minY - height * originY(),
                          width,
                          height);
}

Core* Text::hitTest(HitInfo*, const Mat2D&)
{
    if (renderOpacity() == 0.0f)
    {
        return nullptr;
    }

    return nullptr;
}

void Text::originValueChanged()
{
    markPaintDirty();
    markWorldTransformDirty();
}

void Text::originXChanged()
{
    markPaintDirty();
    markWorldTransformDirty();
}
void Text::originYChanged()
{
    markPaintDirty();
    markWorldTransformDirty();
}

void Text::verticalTrimValueChanged()
{
    // Trim affects both the rendered box and the layout-measured size, so go
    // through the shape/layout path like sizing does.
    markShapeDirty();
}

#else
// Text disabled.
Text::~Text() {}
void Text::draw(Renderer* renderer) {}
Core* Text::hitTest(HitInfo*, const Mat2D&) { return nullptr; }
void Text::addRun(TextValueRun* run) {}
void Text::addModifierGroup(TextModifierGroup* group) {}
void Text::markShapeDirty(bool sendToLayout) {}
void Text::markShapeDirty() {}
void Text::update(ComponentDirt value) {}
void Text::onDirty(ComponentDirt value) {}
void Text::alignValueChanged() {}
void Text::sizingValueChanged() {}
void Text::overflowValueChanged() {}
void Text::widthChanged() {}
void Text::heightChanged() {}
void Text::markPaintDirty() {}
void Text::modifierShapeDirty() {}
bool Text::modifierRangesNeedShape() const { return false; }
const TextStylePaint* Text::styleFromShaperId(uint16_t id) const
{
    return nullptr;
}
void Text::paragraphSpacingChanged() {}
AABB Text::localBounds() const { return AABB(); }
void Text::originValueChanged() {}
void Text::originXChanged() {}
void Text::originYChanged() {}
void Text::verticalTrimValueChanged() {}
Vec2D Text::measureLayout(float width,
                          LayoutMeasureMode widthMode,
                          float height,
                          LayoutMeasureMode heightMode)
{
    return Vec2D();
}
void Text::controlSize(Vec2D size,
                       LayoutScaleType widthScaleType,
                       LayoutScaleType heightScaleType,
                       LayoutDirection direction)
{}
#endif

TextAlign Text::align() const
{
    auto val = (TextAlign)alignValue();
    if (m_layoutDirection == LayoutDirection::inherit ||
        val == TextAlign::center)
    {
        return val;
    }
    return m_layoutDirection == LayoutDirection::ltr ? TextAlign::left
                                                     : TextAlign::right;
}

void Text::updateList(std::vector<rcp<ViewModelInstanceListItem>>* list)
{
#ifdef WITH_RIVE_TEXT
    buildTextStylePaints();
    m_allRuns.clear();
    m_allRuns.assign(m_runs.begin(), m_runs.end());
    auto currentSize = m_valueRunListeners.size();
    size_t index = 0;
    for (auto& listItem : *list)
    {
        auto instance = listItem->viewModelInstance();
        if (instance)
        {
            TextValueRun* textRun;
            if (index < currentSize)
            {
                auto textRunListener = m_valueRunListeners[index];
                textRunListener->remap(instance);
                textRun = textRunListener->textValueRun();
            }
            else
            {
                textRun = new TextValueRun();
                textRun->textComponent(this);
                auto textRunListener =
                    new TextValueRunListener(textRun, instance, this);
                m_valueRunListeners.push_back(textRunListener);
            }
            if (textRun)
            {
                m_allRuns.push_back(textRun);
            }
            index++;
        }
    }
    while (m_valueRunListeners.size() > index)
    {
        auto valueRun = m_valueRunListeners.back();
        m_valueRunListeners.pop_back();
        delete valueRun;
    }
    markShapeDirty();
#endif
}

#ifdef WITH_RIVE_TEXT
void Text::buildTextStylePaints()
{
    if (m_textStylePaints.size() == 0)
    {
        for (auto& child : children())
        {
            if (child->coreType() == TextStylePaint::typeKey)
            {
                m_textStylePaints.push_back(child->as<TextStylePaint>());
            }
        }
    }
}
#endif