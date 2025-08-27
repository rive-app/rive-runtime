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
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/clip_result.hpp"
#include <limits>

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

    for (TextValueRun* textValueRun : m_runs)
    {
        textValueRun->resetHitTest();
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
    return {
        minY,
        maxWidth,
        totalHeight,
        ellipsisLine,
        isEllipsisLineLast,
    };
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
            m_bounds = AABB(0.0f,
                            minY,
                            maxWidth,
                            std::max(minY, totalHeight - paragraphSpace));
            break;
        case TextSizing::autoHeight:
            m_bounds = AABB(0.0f,
                            minY,
                            effectiveWidth(),
                            std::max(minY, totalHeight - paragraphSpace));
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

    // Step 6: add the glyphs to render paths
    float curY = minY;
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

                RawPath path = font->getPath(glyphId);

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

                path.transformInPlace(pathTransform);

                assert(run->styleId < m_runs.size());
                TextValueRun* textValueRun = m_runs[run->styleId];
                TextStylePaint* style = textValueRun->style();
                // TextValueRun::onAddedDirty botches loading if it cannot
                // resolve a style, so we're confident we have a style here.
                assert(style != nullptr);

                if (style->addPath(path, opacity))
                {
                    // This was the first path added to the style, so let's
                    // mark it in our draw list.
                    m_renderStyles.push_back(style);
                    style->propagateOpacity(renderOpacity());
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
    for (TextValueRun* textValueRun : m_runs)
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
    ClipResult clipResult = applyClip(renderer);
    if (clipResult == ClipResult::noClip)
    {
        // We didn't clip, so make sure to save as we'll be doing some
        // transformations.
        renderer->save();
    }
    if (clipResult != ClipResult::emptyClip)
    {
        // For now we need to check both empty() and hasRenderPath() in
        // ShapePaintPath because the raw path gets cleared when the render path
        // is created.
        if (overflow() == TextOverflow::clipped &&
            (!m_clipPath.empty() || m_clipPath.hasRenderPath()))
        {
            renderer->clipPath(m_clipPath.renderPath(this));
        }
        auto worldTransform = shapeWorldTransform();
        for (auto style : m_renderStyles)
        {
            style->draw(renderer, worldTransform);
        }
    }
    renderer->restore();
}

void Text::addRun(TextValueRun* run) { m_runs.push_back(run); }

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

bool Text::makeStyled(StyledText& styledText, bool withModifiers) const
{
    styledText.clear();
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
        styledText.append(style->font(),
                          style->fontSize(),
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
        if (precomputeModifierCoverage &&
            makeStyled(m_modifierStyledText, false))
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
        if (makeStyled(m_styledText))
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
        Vec2D bounds;
        switch (sizing())
        {
            case TextSizing::autoWidth:
                bounds = Vec2D(maxWidth, std::max(minY, computedHeight));
                break;
            case TextSizing::autoHeight:
                bounds = Vec2D(width(), std::max(minY, computedHeight));
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

#else
// Text disabled.
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