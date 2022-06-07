#include "rive/math/mat2d.hpp"
#include "rive/renderer.hpp"

using namespace rive;

Mat2D rive::computeAlignment(Fit fit, Alignment alignment, const AABB& frame, const AABB& content) {
    float contentWidth = content.width();
    float contentHeight = content.height();
    float x = -content.left() - contentWidth * 0.5f - (alignment.x() * contentWidth * 0.5f);
    float y = -content.top() - contentHeight * 0.5f - (alignment.y() * contentHeight * 0.5f);

    float scaleX = 1.0f, scaleY = 1.0f;

    switch (fit) {
        case Fit::fill: {
            scaleX = frame.width() / contentWidth;
            scaleY = frame.height() / contentHeight;
            break;
        }
        case Fit::contain: {
            float minScale =
                std::fmin(frame.width() / contentWidth, frame.height() / contentHeight);
            scaleX = scaleY = minScale;
            break;
        }
        case Fit::cover: {
            float maxScale =
                std::fmax(frame.width() / contentWidth, frame.height() / contentHeight);
            scaleX = scaleY = maxScale;
            break;
        }
        case Fit::fitHeight: {
            float minScale = frame.height() / contentHeight;
            scaleX = scaleY = minScale;
            break;
        }
        case Fit::fitWidth: {
            float minScale = frame.width() / contentWidth;
            scaleX = scaleY = minScale;
            break;
        }
        case Fit::none: {
            scaleX = scaleY = 1.0f;
            break;
        }
        case Fit::scaleDown: {
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

void Renderer::rotate(float radians) {
    const float s = std::sin(radians);
    const float c = std::cos(radians);
    this->transform(Mat2D(c, s, -s, c, 0, 0));
}

/////////////////////////////////////////

#include "../src/render_counter.hpp"

static RenderCounter gCounter;

const char* gCounterNames[] = {
    "buffer", "path", "paint", "shader", "image",
};

void RenderCounter::dump(const char label[]) const {
    if (label == nullptr) {
        label = "RenderCounters";
    }
    printf("%s:", label);
    for (int i = 0; i <= kLastCounterType; ++i) {
        printf(" [%s]:%d", gCounterNames[i], counts[i]);
    }
    printf("\n");
}

RenderCounter& RenderCounter::globalCounter() { return gCounter; }

RenderBuffer::RenderBuffer(size_t count) : m_Count(count) {
    gCounter.update(kBuffer, 1);
}

RenderBuffer::~RenderBuffer() {
    gCounter.update(kBuffer, -1);
}

RenderShader::RenderShader() { gCounter.update(kShader, 1); }
RenderShader::~RenderShader() { gCounter.update(kShader, -1); }

RenderPaint::RenderPaint() { gCounter.update(kPaint, 1); }
RenderPaint::~RenderPaint() { gCounter.update(kPaint, -1); }

RenderImage::RenderImage() { gCounter.update(kImage, 1); }
RenderImage::~RenderImage() { gCounter.update(kImage, -1); }

RenderPath::RenderPath() { gCounter.update(kPath, 1); }
RenderPath::~RenderPath() { gCounter.update(kPath, -1); }
