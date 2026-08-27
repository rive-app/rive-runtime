/*
 * Copyright 2026 Rive
 */

#include "utils/svg_factory.hpp"
#include <cstring>
#include <iomanip>

namespace rive
{

// ---- SVGRenderPath ----

SVGRenderPath::SVGRenderPath(RawPath& rawPath, FillRule fillRule) :
    m_rawPath(rawPath), m_fillRule(fillRule)
{}

void SVGRenderPath::rewind() { m_rawPath.rewind(); }

void SVGRenderPath::fillRule(FillRule value) { m_fillRule = value; }

void SVGRenderPath::addRenderPath(const RenderPath* path,
                                  const Mat2D& transform)
{
    auto* svgPath = static_cast<const SVGRenderPath*>(path);
    m_rawPath.addPath(svgPath->m_rawPath, &transform);
}

void SVGRenderPath::moveTo(float x, float y) { m_rawPath.moveTo(x, y); }

void SVGRenderPath::lineTo(float x, float y)
{
    m_rawPath.injectImplicitMoveIfNeeded();
    m_rawPath.line({x, y});
}

void SVGRenderPath::cubicTo(float ox,
                            float oy,
                            float ix,
                            float iy,
                            float x,
                            float y)
{
    m_rawPath.injectImplicitMoveIfNeeded();
    m_rawPath.cubic({ox, oy}, {ix, iy}, {x, y});
}

void SVGRenderPath::close() { m_rawPath.close(); }

void SVGRenderPath::addRawPath(const RawPath& path)
{
    m_rawPath.addPath(path, nullptr);
}

std::string SVGRenderPath::toSvgD(int floatPrecision) const
{
    auto verbs = m_rawPath.verbs();
    auto points = m_rawPath.points();

    std::ostringstream out;
    out << std::setprecision(floatPrecision);

    size_t ptIdx = 0;
    for (size_t i = 0; i < verbs.size(); ++i)
    {
        switch (verbs[i])
        {
            case PathVerb::move:
                out << "M" << points[ptIdx].x << " " << points[ptIdx].y;
                // SVG never strokes a subpath that is only a moveto, but it
                // does stroke a closed zero-length one, which is how a round or
                // square cap draws its dot.
                if (i + 1 == verbs.size() || verbs[i + 1] == PathVerb::move)
                {
                    out << "Z";
                }
                ptIdx += 1;
                break;
            case PathVerb::line:
                out << "L" << points[ptIdx].x << " " << points[ptIdx].y;
                ptIdx += 1;
                break;
            case PathVerb::quad:
                out << "Q" << points[ptIdx].x << " " << points[ptIdx].y << " "
                    << points[ptIdx + 1].x << " " << points[ptIdx + 1].y;
                ptIdx += 2;
                break;
            case PathVerb::cubic:
                out << "C" << points[ptIdx].x << " " << points[ptIdx].y << " "
                    << points[ptIdx + 1].x << " " << points[ptIdx + 1].y << " "
                    << points[ptIdx + 2].x << " " << points[ptIdx + 2].y;
                ptIdx += 3;
                break;
            case PathVerb::close:
                out << "Z";
                break;
        }
    }
    return out.str();
}

// ---- SVGLinearGradientShader ----

SVGLinearGradientShader::SVGLinearGradientShader(float sx,
                                                 float sy,
                                                 float ex,
                                                 float ey,
                                                 const ColorInt colors[],
                                                 const float stops[],
                                                 size_t count) :
    m_sx(sx),
    m_sy(sy),
    m_ex(ex),
    m_ey(ey),
    m_colors(colors, colors + count),
    m_stops(stops, stops + count)
{}

static void emitColorStop(std::ostream& out, ColorInt color, float offset)
{
    unsigned r = colorRed(color);
    unsigned g = colorGreen(color);
    unsigned b = colorBlue(color);
    float a = colorAlpha(color) / 255.0f;
    out << "<stop offset=\"" << offset << "\" stop-color=\"rgb(" << r << ","
        << g << "," << b << ")\" stop-opacity=\"" << a << "\"/>\n";
}

void SVGLinearGradientShader::emitDefs(std::ostream& out,
                                       const std::string& id) const
{
    out << "<linearGradient id=\"" << id << "\" x1=\"" << m_sx << "\" y1=\""
        << m_sy << "\" x2=\"" << m_ex << "\" y2=\"" << m_ey
        << "\" gradientUnits=\"userSpaceOnUse\">\n";
    for (size_t i = 0; i < m_colors.size(); i++)
    {
        emitColorStop(out, m_colors[i], m_stops[i]);
    }
    out << "</linearGradient>\n";
}

// ---- SVGRadialGradientShader ----

SVGRadialGradientShader::SVGRadialGradientShader(float cx,
                                                 float cy,
                                                 float radius,
                                                 const ColorInt colors[],
                                                 const float stops[],
                                                 size_t count) :
    m_cx(cx),
    m_cy(cy),
    m_radius(radius),
    m_colors(colors, colors + count),
    m_stops(stops, stops + count)
{}

void SVGRadialGradientShader::emitDefs(std::ostream& out,
                                       const std::string& id) const
{
    out << "<radialGradient id=\"" << id << "\" cx=\"" << m_cx << "\" cy=\""
        << m_cy << "\" r=\"" << m_radius
        << "\" gradientUnits=\"userSpaceOnUse\">\n";
    for (size_t i = 0; i < m_colors.size(); i++)
    {
        emitColorStop(out, m_colors[i], m_stops[i]);
    }
    out << "</radialGradient>\n";
}

// ---- SVGRenderPaint ----

void SVGRenderPaint::style(RenderPaintStyle value)
{
    m_isStroke = (value == RenderPaintStyle::stroke);
}

void SVGRenderPaint::color(ColorInt value) { m_color = value; }

void SVGRenderPaint::thickness(float value) { m_thickness = value; }

void SVGRenderPaint::join(StrokeJoin value) { m_join = value; }

void SVGRenderPaint::cap(StrokeCap value) { m_cap = value; }

void SVGRenderPaint::blendMode(BlendMode value) { m_blendMode = value; }

void SVGRenderPaint::shader(rcp<RenderShader> sh) { m_shader = std::move(sh); }

SVGRenderShader* SVGRenderPaint::getSvgShader() const
{
    return static_cast<SVGRenderShader*>(m_shader.get());
}

// ---- SVGRenderImage ----

SVGRenderImage::SVGRenderImage(Span<const uint8_t> encodedBytes,
                               uint32_t width,
                               uint32_t height) :
    m_encodedBytes(encodedBytes.begin(), encodedBytes.end())
{
    m_Width = width;
    m_Height = height;
}

static const char kBase64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64Encode(const uint8_t* data, size_t len)
{
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3)
    {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len)
            n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len)
            n |= (uint32_t)data[i + 2];

        out += kBase64Table[(n >> 18) & 0x3F];
        out += kBase64Table[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? kBase64Table[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? kBase64Table[n & 0x3F] : '=';
    }
    return out;
}

std::string SVGRenderImage::toDataURI() const
{
    // Detect format from magic bytes
    const char* mime = "application/octet-stream";
    if (m_encodedBytes.size() >= 8)
    {
        if (m_encodedBytes[0] == 0x89 && m_encodedBytes[1] == 'P')
            mime = "image/png";
        else if (m_encodedBytes[0] == 0xFF && m_encodedBytes[1] == 0xD8)
            mime = "image/jpeg";
        else if (m_encodedBytes[0] == 'R' && m_encodedBytes[1] == 'I' &&
                 m_encodedBytes[2] == 'F' && m_encodedBytes[3] == 'F')
            mime = "image/webp";
    }

    std::string uri = "data:";
    uri += mime;
    uri += ";base64,";
    uri += base64Encode(m_encodedBytes.data(), m_encodedBytes.size());
    return uri;
}

// ---- SVGRenderBuffer ----

SVGRenderBuffer::SVGRenderBuffer(RenderBufferType type,
                                 RenderBufferFlags flags,
                                 size_t sizeInBytes) :
    RenderBuffer(type, flags, sizeInBytes), m_bytes(sizeInBytes)
{}

void* SVGRenderBuffer::onMap() { return m_bytes.data(); }
void SVGRenderBuffer::onUnmap() {}

// ---- SVGFactory ----

rcp<RenderBuffer> SVGFactory::makeRenderBuffer(RenderBufferType type,
                                               RenderBufferFlags flags,
                                               size_t sizeInBytes)
{
    return make_rcp<SVGRenderBuffer>(type, flags, sizeInBytes);
}

rcp<RenderShader> SVGFactory::makeLinearGradient(float sx,
                                                 float sy,
                                                 float ex,
                                                 float ey,
                                                 const ColorInt colors[],
                                                 const float stops[],
                                                 size_t count)
{
    return make_rcp<SVGLinearGradientShader>(sx,
                                             sy,
                                             ex,
                                             ey,
                                             colors,
                                             stops,
                                             count);
}

rcp<RenderShader> SVGFactory::makeRadialGradient(float cx,
                                                 float cy,
                                                 float radius,
                                                 const ColorInt colors[],
                                                 const float stops[],
                                                 size_t count)
{
    return make_rcp<SVGRadialGradientShader>(cx,
                                             cy,
                                             radius,
                                             colors,
                                             stops,
                                             count);
}

rcp<RenderPath> SVGFactory::makeRenderPath(RawPath& rawPath, FillRule fillRule)
{
    return make_rcp<SVGRenderPath>(rawPath, fillRule);
}

rcp<RenderPath> SVGFactory::makeEmptyRenderPath()
{
    return make_rcp<SVGRenderPath>();
}

rcp<RenderPaint> SVGFactory::makeRenderPaint()
{
    return make_rcp<SVGRenderPaint>();
}

rcp<RenderImage> SVGFactory::decodeImage(Span<const uint8_t> data)
{
    uint32_t width = 0, height = 0;
    if (m_decodeImageSize == nullptr ||
        !m_decodeImageSize(data, &width, &height))
    {
        return nullptr;
    }
    return make_rcp<SVGRenderImage>(data, width, height);
}

} // namespace rive
