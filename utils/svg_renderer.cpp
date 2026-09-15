/*
 * Copyright 2026 Rive
 */

#include "utils/svg_renderer.hpp"
#include "rive/shapes/paint/color.hpp"
#include <cmath>
#include <cstdio>
#include <iomanip>

namespace rive
{
namespace
{
constexpr float kEpsilon = 1e-6f;

const char* blendModeToCss(BlendMode mode)
{
    switch (mode)
    {
        case BlendMode::srcOver:
            return nullptr;
        case BlendMode::screen:
            return "screen";
        case BlendMode::overlay:
            return "overlay";
        case BlendMode::darken:
            return "darken";
        case BlendMode::lighten:
            return "lighten";
        case BlendMode::colorDodge:
            return "color-dodge";
        case BlendMode::colorBurn:
            return "color-burn";
        case BlendMode::hardLight:
            return "hard-light";
        case BlendMode::softLight:
            return "soft-light";
        case BlendMode::difference:
            return "difference";
        case BlendMode::exclusion:
            return "exclusion";
        case BlendMode::multiply:
            return "multiply";
        case BlendMode::hue:
            return "hue";
        case BlendMode::saturation:
            return "saturation";
        case BlendMode::color:
            return "color";
        case BlendMode::luminosity:
            return "luminosity";
    }
    RIVE_UNREACHABLE();
}

// Inline " style=\"mix-blend-mode:..\"" suffix for a draw item.
std::string blendStyle(BlendMode mode)
{
    const char* css = blendModeToCss(mode);
    if (css == nullptr)
    {
        return {};
    }
    return std::string(" style=\"mix-blend-mode:") + css + "\"";
}
} // namespace

void SVGRenderer::save()
{
    StackEntry next;
    next.matrix = m_stack.back().matrix;
    next.opacity = m_stack.back().opacity;
    m_stack.emplace_back(std::move(next));
}

void SVGRenderer::restore()
{
    // The bottom-most entry is the implicit root and must never be popped.
    if (m_stack.size() <= 1)
    {
        return;
    }

    StackEntry entry = std::move(m_stack.back());
    m_stack.pop_back();

    // Close any clip-path <g> wrappers we opened directly into this entry.
    for (int i = 0; i < entry.openClipGroups; ++i)
    {
        Item closeItem;
        closeItem.body = "</g>\n";
        closeItem.isDraw = false;
        entry.items.emplace_back(std::move(closeItem));
    }

    // Flush this entry's items into the parent's items as a single
    // pass-through chunk so the parent never re-wraps our content.
    std::ostringstream chunk;
    writeEntry(chunk, entry);

    auto& parent = m_stack.back();
    Item passthrough;
    passthrough.body = chunk.str();
    passthrough.isDraw = false;
    // When the chunk is exactly one top-level clip group (the clip opened
    // first and its close is last), record the clip id so writeEntry can
    // merge adjacent chunks sharing the same clip.
    if (entry.openClipGroups >= 1 && !entry.items.empty() &&
        entry.items.front().clipOpenId >= 0 && !wantsBlendWrap(entry))
    {
        passthrough.singleClipId = entry.items.front().clipOpenId;
        passthrough.clipOpenLen = entry.items.front().body.size();
    }
    parent.items.emplace_back(std::move(passthrough));
}

void SVGRenderer::transform(const Mat2D& mat)
{
    auto& top = m_stack.back();
    top.matrix = top.matrix * mat;
}

void SVGRenderer::modulateOpacity(float opacity)
{
    auto& top = m_stack.back();
    top.opacity *= opacity;
}

void SVGRenderer::writeTransform(std::ostream& out, const Mat2D& m)
{
    bool isIdentityLinear =
        std::abs(m[0] - 1) < kEpsilon && std::abs(m[1]) < kEpsilon &&
        std::abs(m[2]) < kEpsilon && std::abs(m[3] - 1) < kEpsilon;
    bool zeroT = std::abs(m[4]) < kEpsilon && std::abs(m[5]) < kEpsilon;

    if (isIdentityLinear && zeroT)
    {
        return;
    }

    if (isIdentityLinear)
    {
        out << " transform=\"translate(" << m[4] << ' ' << m[5] << ")\"";
        return;
    }

    out << " transform=\"matrix(" << m[0] << ' ' << m[1] << ' ' << m[2] << ' '
        << m[3] << ' ' << m[4] << ' ' << m[5] << ")\"";
}

void SVGRenderer::writeColor(std::ostream& out,
                             unsigned r,
                             unsigned g,
                             unsigned b)
{
    auto prevFlags = out.flags();
    auto prevFill = out.fill();
    out << '#' << std::hex << std::setfill('0') << std::setw(2) << r
        << std::setw(2) << g << std::setw(2) << b;
    out.flags(prevFlags);
    out.fill(prevFill);
}

void SVGRenderer::emitPaintAttributes(std::ostream& out,
                                      SVGRenderPaint* paint,
                                      SVGRenderPath* path,
                                      float extraOpacity)
{
    ColorInt c = paint->getColor();
    unsigned r = colorRed(c);
    unsigned g = colorGreen(c);
    unsigned b = colorBlue(c);
    float a = (colorAlpha(c) / 255.0f) * extraOpacity;

    auto* shader = paint->getSvgShader();

    if (paint->isStroke())
    {
        out << " fill=\"none\"";

        if (shader)
        {
            std::string gradId = "grad" + std::to_string(m_gradientIdCounter++);
            shader->emitDefs(m_defs, gradId);
            out << " stroke=\"url(#" << gradId << ")\"";
        }
        else
        {
            out << " stroke=\"";
            writeColor(out, r, g, b);
            out << "\"";
        }
        if (a < 1.0f - kEpsilon)
        {
            out << " stroke-opacity=\"" << a << "\"";
        }
        out << " stroke-width=\"" << paint->getThickness() << "\"";

        switch (paint->getJoin())
        {
            case StrokeJoin::miter:
                break;
            case StrokeJoin::round:
                out << " stroke-linejoin=\"round\"";
                break;
            case StrokeJoin::bevel:
                out << " stroke-linejoin=\"bevel\"";
                break;
        }
        switch (paint->getCap())
        {
            case StrokeCap::butt:
                break;
            case StrokeCap::round:
                out << " stroke-linecap=\"round\"";
                break;
            case StrokeCap::square:
                out << " stroke-linecap=\"square\"";
                break;
        }
    }
    else
    {
        if (shader)
        {
            std::string gradId = "grad" + std::to_string(m_gradientIdCounter++);
            shader->emitDefs(m_defs, gradId);
            out << " fill=\"url(#" << gradId << ")\"";
        }
        else if (r != 0 || g != 0 || b != 0)
        {
            // Skip the attribute entirely for the SVG default (black).
            out << " fill=\"";
            writeColor(out, r, g, b);
            out << "\"";
        }
        if (a < 1.0f - kEpsilon)
        {
            out << " fill-opacity=\"" << a << "\"";
        }

        if (path->getFillRule() == FillRule::evenOdd)
        {
            out << " fill-rule=\"evenodd\"";
        }
    }
}

void SVGRenderer::drawPath(RenderPath* renderPath, RenderPaint* renderPaint)
{
    auto* path = static_cast<SVGRenderPath*>(renderPath);
    auto* paint = static_cast<SVGRenderPaint*>(renderPaint);

    std::string d = path->toSvgD(m_floatPrecision);
    if (d.empty())
    {
        return;
    }

    auto& top = m_stack.back();

    // Build the element body without any inline blend style. Blend is applied
    // at flush time so that multiple draws inside a single save block sharing
    // a non-srcOver blend can be wrapped in a <g style="..."> instead.
    std::ostringstream body;
    body << std::setprecision(m_floatPrecision);
    body << "<path";
    writeTransform(body, top.matrix);
    emitPaintAttributes(body, paint, path, top.opacity);
    body << " d=\"" << d << "\"/>\n";

    Item item;
    item.body = body.str();
    item.blend = paint->getBlendMode();
    item.isDraw = true;
    top.items.emplace_back(std::move(item));
}

void SVGRenderer::clipPath(RenderPath* renderPath)
{
    auto* path = static_cast<SVGRenderPath*>(renderPath);
    std::string d = path->toSvgD(m_floatPrecision);
    if (d.empty())
    {
        return;
    }

    auto& top = m_stack.back();

    // The clip geometry is in the coordinate space current at clip time, so
    // bake the CTM onto the def's <path>. The wrapping <g> must stay
    // transform-free: children write their own absolute matrix on each
    // <path>, and a transform here would compose with (double-apply) it.
    std::ostringstream defPath;
    defPath << std::setprecision(m_floatPrecision);
    defPath << "<path";
    writeTransform(defPath, top.matrix);
    defPath << " d=\"" << d << "\"";
    if (path->getFillRule() == FillRule::evenOdd)
    {
        defPath << " clip-rule=\"evenodd\"";
    }
    defPath << "/>";

    // Identical geometry reuses the existing def.
    auto found = m_clipDefIds.find(defPath.str());
    int clipId;
    if (found != m_clipDefIds.end())
    {
        clipId = found->second;
    }
    else
    {
        clipId = m_clipIdCounter++;
        m_clipDefIds.emplace(defPath.str(), clipId);
        m_defs << "<clipPath id=\"clip" << clipId << "\">" << defPath.str()
               << "</clipPath>\n";
    }

    Item item;
    item.body = "<g clip-path=\"url(#clip" + std::to_string(clipId) + ")\">\n";
    item.clipOpenId = clipId;
    top.items.emplace_back(std::move(item));
    top.openClipGroups++;
}

void SVGRenderer::drawImage(const RenderImage* image,
                            ImageSampler,
                            BlendMode blendMode,
                            float opacity)
{
    auto* svgImage = static_cast<const SVGRenderImage*>(image);

    auto& top = m_stack.back();
    float a = top.opacity * opacity;

    std::ostringstream body;
    body << std::setprecision(m_floatPrecision);
    body << "<image width=\"" << image->width() << "\" height=\""
         << image->height() << "\" href=\"" << svgImage->toDataURI() << "\"";
    writeTransform(body, top.matrix);
    if (a < 1.0f - kEpsilon)
    {
        body << " opacity=\"" << a << "\"";
    }
    body << "/>\n";

    Item item;
    item.body = body.str();
    item.blend = blendMode;
    item.isDraw = true;
    top.items.emplace_back(std::move(item));
}

void SVGRenderer::drawImageMesh(const RenderImage*,
                                ImageSampler,
                                rcp<RenderBuffer>,
                                rcp<RenderBuffer>,
                                rcp<RenderBuffer>,
                                uint32_t,
                                uint32_t,
                                BlendMode,
                                float)
{
    static bool warned = false;
    if (!warned)
    {
        fprintf(stderr,
                "SVGRenderer: drawImageMesh is not supported in SVG output "
                "and will be skipped.\n");
        warned = true;
    }
}

bool SVGRenderer::wantsBlendWrap(const StackEntry& entry)
{
    // All draw items must share a single non-srcOver blend mode.
    BlendMode firstBlend = BlendMode::srcOver;
    int drawCount = 0;
    for (const auto& it : entry.items)
    {
        if (!it.isDraw)
        {
            continue;
        }
        if (drawCount == 0)
        {
            firstBlend = it.blend;
        }
        else if (it.blend != firstBlend)
        {
            return false;
        }
        drawCount++;
    }
    return drawCount > 1 && firstBlend != BlendMode::srcOver;
}

void SVGRenderer::writeEntry(std::ostream& dst, const StackEntry& entry)
{
    if (entry.items.empty())
    {
        return;
    }

    // If all draw items share a single non-srcOver blend mode, wrap them in
    // a group with that blend (children get no inline style). Otherwise,
    // emit each item with its own inline blend style.
    bool wrap = wantsBlendWrap(entry);
    if (wrap)
    {
        BlendMode firstBlend = BlendMode::srcOver;
        for (const auto& it : entry.items)
        {
            if (it.isDraw)
            {
                firstBlend = it.blend;
                break;
            }
        }
        // wantsBlendWrap() guarantees firstBlend is not srcOver.
        dst << "<g" << blendStyle(firstBlend) << ">\n";
    }

    constexpr size_t kClipCloseLen = sizeof("</g>\n") - 1;
    const auto& items = entry.items;
    for (size_t i = 0; i < items.size(); i++)
    {
        const Item& it = items[i];
        if (!it.isDraw || wrap || it.blend == BlendMode::srcOver)
        {
            // Pass-through (clip groups, restored chunks) or wrapped draws or
            // a draw with no blend at all: emit body verbatim — except that
            // adjacent single-clip chunks sharing a clip merge into one
            // group, by dropping this chunk's close and/or open tag.
            bool mergePrev = it.singleClipId >= 0 && i > 0 &&
                             items[i - 1].singleClipId == it.singleClipId;
            bool mergeNext = it.singleClipId >= 0 && i + 1 < items.size() &&
                             items[i + 1].singleClipId == it.singleClipId;
            size_t begin = mergePrev ? it.clipOpenLen : 0;
            size_t end = it.body.size() - (mergeNext ? kClipCloseLen : 0);
            dst.write(it.body.data() + begin,
                      static_cast<std::streamsize>(end - begin));
        }
        else
        {
            // Single-draw blend (or mixed-blend save block): inline the style
            // by inserting it just before the self-closing "/>".
            std::string body = it.body;
            std::string style = blendStyle(it.blend);
            auto close = body.rfind("/>");
            if (close != std::string::npos && !style.empty())
            {
                body.insert(close, style);
            }
            dst << body;
        }
    }

    if (wrap)
    {
        dst << "</g>\n";
    }
}

std::string SVGRenderer::finalize(int width, int height) const
{
    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
        << "viewBox=\"0 0 " << width << " " << height << "\" "
        << "width=\"" << width << "\" height=\"" << height << "\">\n";

    std::string defs = m_defs.str();
    if (!defs.empty())
    {
        svg << "<defs>\n" << defs << "</defs>\n";
    }

    // The root entry is never restored. Flush it directly, applying the same
    // blend-grouping rules so that root-level draws behave consistently.
    if (!m_stack.empty())
    {
        writeEntry(svg, m_stack.front());
    }

    svg << "</svg>\n";
    return svg.str();
}

} // namespace rive
