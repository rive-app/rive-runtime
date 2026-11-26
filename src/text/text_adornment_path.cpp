#include "rive/text/text_adornment_path.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/path.hpp"
#include "rive/text/text_style_geometry.hpp"
#include <algorithm>

using namespace rive;

struct TextAdornmentPath::Modified
{
  struct Rect
  {
    AABB rect;
    Mat2D transform;
    float opacity;
  };
  struct FadedRect
  {
    AABB rect;
    float opacity;
    // The order it was added in, so sorting by opacity keeps it.
    uint32_t order;
  };
  // Rects a modifier moved, turned or scaled: each one is a shape of its own.
  std::vector<Rect> transformed;
  // Rects a modifier only faded: they join with the others of their opacity.
  std::vector<FadedRect> faded;
  // One opacity's faded rects, and the paths they or a single rect make.
  // Kept across layouts so building them doesn't allocate once they've
  // grown.
  std::vector<AABB> joined;
  TextSelectionPath scratch{true, FillRule::clockwise};
  RawPath rectPath;
  // Every rect, also bucketed by opacity, as a style's glyphs are.
  TextStyleGeometry geometry;
  // Whether this layout added any; while it hasn't, m_path is every rect.
  bool active = false;
};

// Adds [rect] with its corners rounded by [radius], as TextSelectionPath
// rounds a contour's, wound clockwise. A single rect needs none of its
// contour finding, which allocates.
static void addRoundedRect(RawPath& path, const AABB& rect, float radius)
{
  if (!(rect.width() > 0.0f && rect.height() > 0.0f))
  {
    return;
  }
  if (radius <= 0.0f)
  {
    path.addRect(rect);
    return;
  }
  const Vec2D corners[4] = {
      Vec2D(rect.minX, rect.minY),
      Vec2D(rect.maxX, rect.minY),
      Vec2D(rect.maxX, rect.maxY),
      Vec2D(rect.minX, rect.maxY),
  };
  for (int i = 0; i < 4; i++)
  {
    const Vec2D pos = corners[i];
    Vec2D toPrev = corners[(i + 3) % 4] - pos;
    const float toPrevLength = toPrev.length();
    toPrev = toPrev * (1.0f / toPrevLength);
    Vec2D toNext = corners[(i + 1) % 4] - pos;
    const float toNextLength = toNext.length();
    toNext = toNext * (1.0f / toNextLength);
    const float renderRadius = std::min(
        toPrevLength / 2.0f,
        std::min(toNextLength / 2.0f, radius));
    const float idealDistance = Path::computeIdealControlPointDistance(
        toPrev,
        toNext,
        renderRadius);
    const Vec2D start = Vec2D::scaleAndAdd(pos, toPrev, renderRadius);
    if (i == 0)
    {
      path.moveTo(start.x, start.y);
    }
    else
    {
      path.lineTo(start.x, start.y);
    }
    const Vec2D out = Vec2D::scaleAndAdd(
        pos,
        toPrev,
        renderRadius - idealDistance);
    const Vec2D in = Vec2D::scaleAndAdd(
        pos,
        toNext,
        renderRadius - idealDistance);
    const Vec2D end = Vec2D::scaleAndAdd(pos, toNext, renderRadius);
    path.cubicTo(out.x, out.y, in.x, in.y, end.x, end.y);
  }
  path.close();
}

TextAdornmentPath::TextAdornmentPath() = default;
TextAdornmentPath::~TextAdornmentPath() = default;

void TextAdornmentPath::reset()
{
  m_rects.clear();
  m_path.rewind();
  if (m_modified != nullptr)
  {
    m_modified->transformed.clear();
    m_modified->faded.clear();
    m_modified->geometry.rewind();
    m_modified->active = false;
  }
}

void TextAdornmentPath::addModifiedRect(
    const AABB& rect,
    const Mat2D* transform,
    float opacity)
{
  if (opacity <= 0.0f)
  {
    return;
  }
  if (transform == nullptr && opacity == 1.0f)
  {
    m_rects.push_back(rect);
    return;
  }
  if (m_modified == nullptr)
  {
    m_modified = std::make_unique<Modified>();
  }
  if (transform == nullptr)
  {
    m_modified->faded.push_back(
        {rect, opacity, (uint32_t)m_modified->faded.size()});
  }
  else
  {
    m_modified->transformed.push_back({rect, *transform, opacity});
  }
}

bool TextAdornmentPath::empty() const
{
  return m_rects.empty() &&
         (m_modified == nullptr ||
          (m_modified->transformed.empty() && m_modified->faded.empty()));
}

void TextAdornmentPath::update(float cornerRadius)
{
  m_path.update(m_rects, cornerRadius);
  if (m_modified == nullptr)
  {
    return;
  }
  Modified& modified = *m_modified;
  modified.geometry.rewind();
  modified.active = !modified.transformed.empty() || !modified.faded.empty();
  if (!modified.active)
  {
    return;
  }
  TextStyleGeometry& geometry = modified.geometry;
  if (!m_path.empty())
  {
    geometry.addPath(*m_path.rawPath(), 1.0f);
  }
  auto& faded = modified.faded;
  // Sorted by opacity, then the order they came in: std::sort doesn't
  // allocate a buffer, as std::stable_sort can, and this one's order is
  // still the same everywhere.
  std::sort(
      faded.begin(),
      faded.end(),
      [](const Modified::FadedRect& a, const Modified::FadedRect& b) {
        return a.opacity < b.opacity ||
               (a.opacity == b.opacity && a.order < b.order);
      });
  for (size_t i = 0; i < faded.size();)
  {
    const float opacity = faded[i].opacity;
    size_t end = i;
    while (end < faded.size() && faded[end].opacity == opacity)
    {
      end++;
    }
    if (end - i == 1)
    {
      // A glyph of its own opacity, as each one is while a fade animates.
      modified.rectPath.rewind();
      addRoundedRect(modified.rectPath, faded[i].rect, cornerRadius);
      geometry.addPath(modified.rectPath, opacity);
    }
    else
    {
      modified.joined.clear();
      for (size_t j = i; j < end; j++)
      {
        modified.joined.push_back(faded[j].rect);
      }
      modified.scratch.update(modified.joined, cornerRadius);
      geometry.addPath(*modified.scratch.rawPath(), opacity);
    }
    i = end;
  }
  for (const Modified::Rect& rect : modified.transformed)
  {
    modified.rectPath.rewind();
    addRoundedRect(modified.rectPath, rect.rect, cornerRadius);
    geometry.addPath(modified.rectPath, rect.opacity, &rect.transform);
  }
}

ShapePaintPath* TextAdornmentPath::path()
{
  if (m_modified != nullptr && m_modified->active)
  {
    return m_modified->geometry.path();
  }
  return &m_path;
}

void TextAdornmentPath::draw(
    Renderer* renderer,
    ShapePaint* shapePaint,
    const Mat2D& worldTransform,
    Factory* factory,
    BlendMode parentMode,
    uint8_t parentAdditiveAmount,
    const AABB* imageBounds)
{
  // A PaintImage fits [imageBounds] in every draw, so neither fading part
  // of the adornment nor a glyph fading in or out moves the image.
  const bool modulated = shapePaint->paintImage() != nullptr;
  if (m_modified == nullptr || !m_modified->active)
  {
    shapePaint->draw(
        renderer,
        &m_path,
        worldTransform,
        true,
        nullptr,
        true,
        imageBounds);
    return;
  }
  TextStyleGeometry& geometry = m_modified->geometry;
  auto& opacityPaths = geometry.opacityPaths();
  if (opacityPaths.empty())
  {
    return;
  }
  const bool inherits = shapePaint->inheritsBlendMode();
  const BlendMode mode = inherits ? parentMode : shapePaint->blendMode();
  const uint8_t amount = inherits ? parentAdditiveAmount
                                  : shapePaint->additiveAmount();
  auto drawPooled =
      [&](ShapePaintPath* path, float opacity, uint32_t paintIndex) {
        RenderPaint* renderPaint = geometry.pooledPaint(paintIndex, modulated);
        shapePaint->applyTo(renderPaint, opacity);
        // Pooled paints are shared across shape paints; always set feather
        // and blend mode so one paint's settings don't leak into the others.
        auto feather = shapePaint->feather();
        renderPaint->feather(feather != nullptr ? feather->strength() : 0.0f);
        renderPaint->blendMode(renderBlendModeFor(mode, amount));
        renderPaint->additiveness(additivenessFor(mode, amount));
        shapePaint->draw(
            renderer,
            path,
            worldTransform,
            true,
            renderPaint,
            true,
            imageBounds);
      };

  // An effect (dash, trim...) is built from every rect and replaces
  // whichever bucket's path ShapePaint::draw is handed. Draw it once, at the
  // highest opacity any rect has, as a style's glyphs do.
  if (shapePaint->lastEffectPath(shapePaint) != nullptr)
  {
    const float opacity = opacityPaths.rbegin()->first;
    if (opacity == 1.0f)
    {
      shapePaint->draw(
          renderer,
          geometry.path(),
          worldTransform,
          true,
          nullptr,
          true,
          imageBounds);
    }
    else
    {
      geometry.reservePaints(factory);
      drawPooled(geometry.path(), opacity, 0);
    }
    return;
  }

  // Opaque rects first, with the paint's own render paint.
  auto opaque = opacityPaths.find(1.0f);
  if (opaque != opacityPaths.end())
  {
    shapePaint->draw(
        renderer,
        &opaque->second,
        worldTransform,
        true,
        nullptr,
        true,
        imageBounds);
  }
  geometry.reservePaints(factory);
  uint32_t paintIndex = 0;
  for (auto& entry : opacityPaths)
  {
    if (entry.first == 1.0f)
    {
      continue;
    }
    drawPooled(&entry.second, entry.first, paintIndex++);
  }
}
