#include "rive/math/mat2d.hpp"
#include "rive/renderer.hpp"
#include "rive/text_engine.hpp"

using namespace rive;

Mat2D rive::computeAlignment(Fit fit, Alignment alignment, const AABB& frame, const AABB& content)
{
    float contentWidth = content.width();
    float contentHeight = content.height();
    float x = -content.left() - contentWidth * 0.5f - (alignment.x() * contentWidth * 0.5f);
    float y = -content.top() - contentHeight * 0.5f - (alignment.y() * contentHeight * 0.5f);

    float scaleX = 1.0f, scaleY = 1.0f;

    switch (fit)
    {
        case Fit::fill:
        {
            scaleX = frame.width() / contentWidth;
            scaleY = frame.height() / contentHeight;
            break;
        }
        case Fit::contain:
        {
            float minScale =
                std::fmin(frame.width() / contentWidth, frame.height() / contentHeight);
            scaleX = scaleY = minScale;
            break;
        }
        case Fit::cover:
        {
            float maxScale =
                std::fmax(frame.width() / contentWidth, frame.height() / contentHeight);
            scaleX = scaleY = maxScale;
            break;
        }
        case Fit::fitHeight:
        {
            float minScale = frame.height() / contentHeight;
            scaleX = scaleY = minScale;
            break;
        }
        case Fit::fitWidth:
        {
            float minScale = frame.width() / contentWidth;
            scaleX = scaleY = minScale;
            break;
        }
        case Fit::none:
        {
            scaleX = scaleY = 1.0f;
            break;
        }
        case Fit::scaleDown:
        {
            float minScale =
                std::fmin(frame.width() / contentWidth, frame.height() / contentHeight);
            scaleX = scaleY = minScale < 1.0f ? minScale : 1.0f;
            break;
        }
    }

    Mat2D translation;
    translation[4] = frame.left() + frame.width() * 0.5f + (alignment.x() * frame.width() * 0.5f);
    translation[5] = frame.top() + frame.height() * 0.5f + (alignment.y() * frame.height() * 0.5f);

    return translation * Mat2D::fromScale(scaleX, scaleY) * Mat2D::fromTranslate(x, y);
}

void Renderer::translate(float tx, float ty) { this->transform(Mat2D(1, 0, 0, 1, tx, ty)); }

void Renderer::scale(float sx, float sy) { this->transform(Mat2D(sx, 0, 0, sy, 0, 0)); }

void Renderer::rotate(float radians)
{
    const float s = std::sin(radians);
    const float c = std::cos(radians);
    this->transform(Mat2D(c, s, -s, c, 0, 0));
}

RenderBuffer::RenderBuffer(RenderBufferType type, RenderBufferFlags flags, size_t sizeInBytes) :
    m_type(type), m_flags(flags), m_sizeInBytes(sizeInBytes)
{}

RenderBuffer::~RenderBuffer() {}

void* RenderBuffer::map()
{
    assert(m_mapCount == 0 || !(m_flags & RenderBufferFlags::mappedOnceAtInitialization));
    assert(m_mapCount == m_unmapCount);
    RIVE_DEBUG_CODE(++m_mapCount;)
    return onMap();
}

void RenderBuffer::unmap()
{
    assert(m_unmapCount + 1 == m_mapCount);
    RIVE_DEBUG_CODE(++m_unmapCount;)
    onUnmap();
}

RenderShader::RenderShader() {}
RenderShader::~RenderShader() {}

RenderPaint::RenderPaint() {}
RenderPaint::~RenderPaint() {}

RenderImage::RenderImage(const Mat2D& uvTransform) : m_uvTransform(uvTransform) {}
RenderImage::RenderImage() {}
RenderImage::~RenderImage() {}

RenderPath::RenderPath() {}
RenderPath::~RenderPath() {}

bool rive::isWhiteSpace(Unichar c) { return c <= ' ' || c == 0x2028; }

SimpleArray<Paragraph> Font::shapeText(Span<const Unichar> text, Span<const TextRun> runs) const
{
#ifdef DEBUG
    size_t count = 0;
    for (const TextRun& tr : runs)
    {
        assert(tr.unicharCount > 0);
        count += tr.unicharCount;
    }
    assert(count <= text.size());
#endif

    SimpleArray<Paragraph> paragraphs = onShapeText(text, runs);
    bool wantWhiteSpace = false;
    GlyphRun* lastRun = nullptr;
    size_t reserveSize = text.size() / 4;
    SimpleArrayBuilder<uint32_t> breakBuilder(reserveSize);
    for (const Paragraph& para : paragraphs)
    {
        for (GlyphRun& gr : para.runs)
        {
            if (lastRun != nullptr)
            {
                lastRun->breaks = std::move(breakBuilder);
                // Reset the builder.
                breakBuilder = SimpleArrayBuilder<uint32_t>(reserveSize);
            }
            uint32_t glyphIndex = 0;
            for (uint32_t offset : gr.textIndices)
            {
                Unichar unicode = text[offset];
                if (unicode == '\n' || unicode == 0x2028)
                {
                    breakBuilder.add(glyphIndex);
                    breakBuilder.add(glyphIndex);
                }
                if (wantWhiteSpace == isWhiteSpace(unicode))
                {
                    breakBuilder.add(glyphIndex);
                    wantWhiteSpace = !wantWhiteSpace;
                }
                glyphIndex++;
            }

            lastRun = &gr;
        }
    }
    if (lastRun != nullptr)
    {
        if (wantWhiteSpace)
        {
            breakBuilder.add((uint32_t)lastRun->glyphs.size());
        }
        else
        {
            // Consume the rest of the run.
            breakBuilder.add(breakBuilder.empty() ? 0 : breakBuilder.back());
            breakBuilder.add((uint32_t)lastRun->glyphs.size());
        }
        lastRun->breaks = std::move(breakBuilder);
    }

#ifdef DEBUG
    for (const Paragraph& para : paragraphs)
    {
        for (const GlyphRun& gr : para.runs)
        {
            assert(gr.glyphs.size() > 0);
            assert(gr.glyphs.size() == gr.textIndices.size());
            assert(gr.glyphs.size() + 1 == gr.xpos.size());
        }
    }
#endif
    return paragraphs;
}
