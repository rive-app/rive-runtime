#include "rive/math/mat2d.hpp"
#include "rive/renderer.hpp"
#include "rive/rive_counter.hpp"

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

RenderBuffer::RenderBuffer(size_t count) : m_Count(count) { Counter::update(Counter::kBuffer, 1); }

RenderBuffer::~RenderBuffer() { Counter::update(Counter::kBuffer, -1); }

RenderShader::RenderShader() { Counter::update(Counter::kShader, 1); }
RenderShader::~RenderShader() { Counter::update(Counter::kShader, -1); }

RenderPaint::RenderPaint() { Counter::update(Counter::kPaint, 1); }
RenderPaint::~RenderPaint() { Counter::update(Counter::kPaint, -1); }

RenderImage::RenderImage(const Mat2D& uvTransform) : m_uvTransform(uvTransform)
{
    Counter::update(Counter::kImage, 1);
}
RenderImage::RenderImage() { Counter::update(Counter::kImage, 1); }
RenderImage::~RenderImage() { Counter::update(Counter::kImage, -1); }

RenderPath::RenderPath() { Counter::update(Counter::kPath, 1); }
RenderPath::~RenderPath() { Counter::update(Counter::kPath, -1); }

#include "rive/render_text.hpp"

static bool isWhiteSpace(rive::Unichar c) { return c <= ' '; }

rive::SimpleArray<RenderGlyphRun>
RenderFont::shapeText(rive::Span<const rive::Unichar> text,
                      rive::Span<const rive::RenderTextRun> runs) const
{
#ifdef DEBUG
    size_t count = 0;
    for (const auto& tr : runs)
    {
        assert(tr.unicharCount > 0);
        count += tr.unicharCount;
    }
    assert(count <= text.size());
#endif

    auto gruns = onShapeText(text, runs);
    bool wantWhiteSpace = false;

    rive::RenderGlyphRun* lastRun = nullptr;
    size_t reserveSize = text.size() / 4;
    rive::SimpleArrayBuilder<uint32_t> breakBuilder(reserveSize);
    for (auto& gr : gruns)
    {
        if (lastRun != nullptr)
        {
            lastRun->breaks = std::move(breakBuilder);
            // Reset the builder.
            breakBuilder = rive::SimpleArrayBuilder<uint32_t>(reserveSize);
        }
        uint32_t glyphIndex = 0;
        for (auto offset : gr.textIndices)
        {

            auto unicode = text[offset];
            if (unicode == '\n')
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
    if (lastRun != nullptr)
    {
        if (wantWhiteSpace)
        {
            breakBuilder.add((uint32_t)lastRun->glyphs.size());
        }
        lastRun->breaks = std::move(breakBuilder);
    }

#ifdef DEBUG
    for (const auto& gr : gruns)
    {
        assert(gr.glyphs.size() > 0);
        assert(gr.glyphs.size() == gr.textIndices.size());
        assert(gr.glyphs.size() + 1 == gr.xpos.size());
    }
#endif
    return gruns;
}
