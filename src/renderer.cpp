#include "rive/math/mat2d.hpp"
#include "rive/renderer.hpp"

using namespace rive;

void Renderer::translate(float tx, float ty) {
    this->transform(Mat2D(1, 0, 0, 1, tx, ty));
}

void Renderer::scale(float sx, float sy) {
    this->transform(Mat2D(sx, 0, 0, sy, 0, 0));
}

void Renderer::rotate(float radians) {
    const float s = std::sin(radians);
    const float c = std::cos(radians);
    this->transform(Mat2D(c, s, -s, c, 0, 0));
}

void Renderer::computeAlignment(Mat2D& result,
                                Fit fit,
                                const Alignment& alignment,
                                const AABB& frame,
                                const AABB& content)
{
    float contentWidth = content[2] - content[0];
    float contentHeight = content[3] - content[1];
    float x = -content[0] - contentWidth / 2.0 -
            (alignment.x() * contentWidth / 2.0);
    float y = -content[1] - contentHeight / 2.0 -
            (alignment.y() * contentHeight / 2.0);

    float scaleX = 1.0, scaleY = 1.0;

    switch (fit) {
      case Fit::fill: {
          scaleX = frame.width() / contentWidth;
          scaleY = frame.height() / contentHeight;
          break;
      }
      case Fit::contain: {
          float minScale = std::fmin(frame.width() / contentWidth,
                                     frame.height() / contentHeight);
          scaleX = scaleY = minScale;
          break;
      }
      case Fit::cover: {
          float maxScale = std::fmax(frame.width() / contentWidth,
                                     frame.height() / contentHeight);
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
          scaleX = scaleY = 1.0;
          break;
      }
      case Fit::scaleDown: {
          float minScale = std::fmin(frame.width() / contentWidth,
                                     frame.height() / contentHeight);
          scaleX = scaleY = minScale < 1.0 ? minScale : 1.0;
          break;
      }
    }

    Mat2D translation;
    translation[4] = frame[0] + frame.width() / 2.0 +
                   (alignment.x() * frame.width() / 2.0);
    translation[5] = frame[1] + frame.height() / 2.0 +
                   (alignment.y() * frame.height() / 2.0);

    result = translation * Mat2D::fromScale(scaleX, scaleY) * Mat2D::fromTranslate(x, y);
}

void Renderer::align(Fit fit,
                     const Alignment& alignment,
                     const AABB& frame,
                     const AABB& content) {
    Mat2D result;
    computeAlignment(result, fit, alignment, frame, content);
    transform(result);
}
