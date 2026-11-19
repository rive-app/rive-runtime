/*
 * Copyright 2026 Rive
 */

// depthStencil vertex shaders step from the next contour's texel back to their
// own by ID - 1, trusting the tessellation texture's layout instead of
// fetching. These tests rasterize each flush's TessVertexSpans into a CPU copy
// of that texture (the way the tessellator does, with its own flag narrowing
// compiled as C++) and walk it in memory order:
//
//   * Contour IDs start at 0 and increase by exactly 1 (or stay put).
//   * Every ID increment has LAST_IN_MEMORY on the texel before it and
//     FIRST_IN_MEMORY on the texel after it.
//   * No texel has both FIRST_IN_MEMORY and LAST_IN_MEMORY.
//   * Every contour's record points where its draw wraps to: its logical first
//     vertex for featherAtlas (which reads it as such), or its first texel in
//     memory for depthStencil (which walks every contour in memory order).
//   * Every texel is written exactly once, through the final padding vertex,
//     whose ID is one past the last contour.

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include <catch.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

// The shader's flag narrowing, compiled as C++. (Last: cpp.glsl's macros would
// break the headers above.)
#define RIVE_GLSL_CROSS_WANT_CONSTRUCTORS
#include "cpp.glsl"
namespace tess_glsl
{
RIVE_GLSL_CROSS_CONSTRUCTORS
#define GLSL
#include "generated/shaders/constants.minified.glsl"
#include "generated/shaders/common.minified.glsl"
#include "generated/shaders/tessellate.minified.glsl"
} // namespace tess_glsl

using namespace rive;
using namespace rive::gpu;

namespace
{
constexpr uint32_t TargetSize = 1024;

class TessCapturingRenderContext : public RenderContextNULL
{
public:
  void* mapTessVertexSpanBuffer(size_t mapSizeInBytes) override
  {
    m_mappedSpans = static_cast<const TessVertexSpan*>(
        RenderContextNULL::mapTessVertexSpanBuffer(mapSizeInBytes));
    return const_cast<TessVertexSpan*>(m_mappedSpans);
  }

  void unmapTessVertexSpanBuffer(size_t mapSizeInBytes) override
  {
    // Keep a copy so flush() can read the spans back.
    m_spans.assign(
        m_mappedSpans,
        m_mappedSpans + mapSizeInBytes / sizeof(TessVertexSpan));
    RenderContextNULL::unmapTessVertexSpanBuffer(mapSizeInBytes);
  }

  void* mapContourBuffer(size_t mapSizeInBytes) override
  {
    m_mappedContours = static_cast<const uint32_t*>(
        RenderContextNULL::mapContourBuffer(mapSizeInBytes));
    return const_cast<uint32_t*>(m_mappedContours);
  }

  void unmapContourBuffer(size_t mapSizeInBytes) override
  {
    // Keep a copy so flush() can look up each contour's path.
    m_contourWords.assign(
        m_mappedContours,
        m_mappedContours + mapSizeInBytes / sizeof(uint32_t));
    RenderContextNULL::unmapContourBuffer(mapSizeInBytes);
  }

  void* mapPathBuffer(size_t mapSizeInBytes) override
  {
    m_mappedPaths = static_cast<const uint32_t*>(
        RenderContextNULL::mapPathBuffer(mapSizeInBytes));
    return const_cast<uint32_t*>(m_mappedPaths);
  }

  void unmapPathBuffer(size_t mapSizeInBytes) override
  {
    // Keep a copy so flush() can tell which paths are feathered.
    m_pathWords.assign(
        m_mappedPaths,
        m_mappedPaths + mapSizeInBytes / sizeof(uint32_t));
    RenderContextNULL::unmapPathBuffer(mapSizeInBytes);
  }

  void flush(const FlushDescriptor& desc) override
  {
    if (desc.drawList != nullptr)
    {
      for (const DrawBatch& batch : *desc.drawList)
      {
        m_drawTypes.insert(batch.drawType);
      }
    }
    if (desc.tessVertexSpanCount != 0)
    {
      ++m_tessFlushCount;
      checkTessInvariants(desc);
    }
  }

  bool sawDrawType(DrawType drawType) const
  {
    return m_drawTypes.count(drawType) != 0;
  }

  size_t tessFlushCount() const { return m_tessFlushCount; }
  // Flushes whose padding contour sits between midpoint-fan and outer-cubic
  // contours.
  size_t mixedPatchFlushCount() const { return m_mixedPatchFlushCount; }
  // Contours the walk checked, by kind.
  size_t pathContourCount() const { return m_pathContourCount; }
  size_t featheredContourCount() const { return m_featheredContourCount; }
  size_t mirroredFeatheredContourCount() const
  {
    return m_mirroredFeatheredContourCount;
  }
  size_t mirroredDepthStencilContourCount() const
  {
    return m_mirroredDepthStencilContourCount;
  }

private:
  struct Texel
  {
    uint32_t contourIDWithFlags = 0;
    uint32_t writeCount = 0;
  };

  // Rasterizes one half of a span (forward or reflection) the way
  // tessellate.glsl does.
  static void rasterizeSpanHalf(
      const TessVertexSpan& span,
      float y,
      int32_t x0x1,
      uint32_t tessDataHeight,
      std::vector<Texel>* texels)
  {
    if (std::isnan(y))
    {
      return; // Discarded reflection.
    }
    const int32_t x0 = static_cast<int16_t>(x0x1 & 0xffff);
    const int32_t x1 = x0x1 >> 16;
    const int32_t row = static_cast<int32_t>(y);
    REQUIRE(row >= 0);
    REQUIRE(static_cast<uint32_t>(row) < tessDataHeight);
    const uint32_t parametricSegmentCount = span.segmentCounts & 0x3ff;
    const uint32_t polarSegmentCount = (span.segmentCounts >> 10) & 0x3ff;
    const uint32_t joinSegmentCount = span.segmentCounts >> 20;
    // (Ignores the tessellator's cull of excess outer-cubic segments, which
    // only spreads the start flags further into a contour's T=0 vertices.)
    const float totalVertexCount = static_cast<float>(
        parametricSegmentCount + polarSegmentCount + joinSegmentCount - 1);
    uint32_t contourIDWithFlags = span.contourIDWithFlags;
    if (x1 < x0) // Reflections are drawn right to left.
    {
      contourIDWithFlags |= TESS_FLAG_MIRRORED;
    }
    for (int32_t x = std::max(std::min(x0, x1), 0);
         x <
         std::min(std::max(x0, x1), static_cast<int32_t>(kTessTextureWidth));
         ++x)
    {
      // Floored and clamped like the tessellator's fragment shader.
      const float vertexIdx = std::max(
          std::floor(
              totalVertexCount - std::abs(static_cast<float>(x1) - (x + .5f))),
          0.f);
      Texel& texel = (*texels)[row * kTessTextureWidth + x];
      texel.contourIDWithFlags = tess_glsl::resolveContourBoundaryFlags(
          contourIDWithFlags,
          vertexIdx,
          totalVertexCount);
      ++texel.writeCount;
    }
  }

  void checkTessInvariants(const FlushDescriptor& desc)
  {
    REQUIRE(
        desc.firstTessVertexSpan + desc.tessVertexSpanCount <= m_spans.size());
    std::vector<Texel> texels(size_t(desc.tessDataHeight) * kTessTextureWidth);
    for (size_t i = 0; i < desc.tessVertexSpanCount; ++i)
    {
      const TessVertexSpan& span = m_spans[desc.firstTessVertexSpan + i];
      rasterizeSpanHalf(span, span.y, span.x0x1, desc.tessDataHeight, &texels);
      rasterizeSpanHalf(
          span,
          span.reflectionY,
          span.reflectionX0X1,
          desc.tessDataHeight,
          &texels);
    }

    size_t end = texels.size();
    while (end > 0 && texels[end - 1].writeCount == 0)
    {
      --end;
    }
    REQUIRE(end > 0);

    auto describe = [&](size_t i) {
      std::ostringstream s;
      s << "texel " << i << ": id "
        << (texels[i].contourIDWithFlags & CONTOUR_ID_MASK)
        << ((texels[i].contourIDWithFlags & TESS_FLAG_FIRST_IN_MEMORY)
                ? " FIRST"
                : "")
        << ((texels[i].contourIDWithFlags & TESS_FLAG_LAST_IN_MEMORY) ? " LAST"
                                                                      : "")
        << ", written " << texels[i].writeCount << "x";
      return s.str();
    };

    for (size_t i = 0; i < end; ++i)
    {
      if (texels[i].writeCount != 1)
      {
        FAIL_CHECK("written other than once: " << describe(i));
        return;
      }
      constexpr uint32_t BothBoundaries = TESS_FLAG_FIRST_IN_MEMORY |
                                          TESS_FLAG_LAST_IN_MEMORY;
      if ((texels[i].contourIDWithFlags & BothBoundaries) == BothBoundaries)
      {
        FAIL_CHECK("FIRST and LAST on the same texel: " << describe(i));
        return;
      }
    }
    if ((texels[0].contourIDWithFlags & CONTOUR_ID_MASK) != 0)
    {
      FAIL_CHECK("contour IDs don't start at 0: " << describe(0));
      return;
    }
    REQUIRE(
        (desc.firstContour + desc.contourCount) * 4 <= m_contourWords.size());
    // The padding between the midpoint-fan and outer-cubic regions is the
    // only contour record with no path.
    auto pathIDOf = [&](uint32_t contourID) {
      return m_contourWords[(desc.firstContour + contourID - 1) * 4 + 2] &
             0xffffu;
    };
    auto vertexIndex0Of = [&](uint32_t contourID) {
      return m_contourWords[(desc.firstContour + contourID - 1) * 4 + 3];
    };
    // Feathered paths draw through the featherAtlas. (gpu::PathData is 4
    // uint4s; the feather radius is the y of the third, where the tessellator
    // reads it.)
    static_assert(sizeof(gpu::PathData) == 16 * sizeof(uint32_t));
    auto isFeathered = [&](uint32_t pathID) {
      const size_t word = (desc.firstPath + pathID) * 16 + 2 * 4 + 1;
      REQUIRE(word < m_pathWords.size());
      return m_pathWords[word] != 0; // Any nonzero float.
    };
    // Checks a finished contour's record, given its texels in memory.
    auto checkVertexIndex0 =
        [&](uint32_t contourID, size_t first, size_t last) {
          const uint32_t pathID = pathIDOf(contourID);
          if (pathID == 0)
          {
            return true; // Padding: nothing wraps to it.
          }
          const bool feathered = isFeathered(pathID);
          const bool mirrored = (texels[first].contourIDWithFlags &
                                 TESS_FLAG_MIRRORED) != 0;
          ++m_pathContourCount;
          m_featheredContourCount += feathered;
          m_mirroredFeatheredContourCount += feathered && mirrored;
          m_mirroredDepthStencilContourCount += !feathered && mirrored;
          // A mirrored contour is written top-down, so its logical first vertex
          // is its last in memory.
          const size_t expected = feathered && mirrored ? last : first;
          if (vertexIndex0Of(contourID) != expected)
          {
            FAIL_CHECK(
                "contour " << contourID
                           << (feathered ? " (feathered"
                                         : " (not "
                                           "feathered")
                           << (mirrored ? ", mirrored)" : ")")
                           << "'s record points at texel "
                           << vertexIndex0Of(contourID) << ", expected "
                           << expected << ": " << describe(first));
            return false;
          }
          return true;
        };
    size_t contourBegin = 0;
    uint32_t paddingContourID = 0;
    size_t paddingContourBegin = 0;
    size_t paddingContourTexelCount = 0;
    for (size_t i = 1; i < end; ++i)
    {
      const uint32_t prev = texels[i - 1].contourIDWithFlags;
      const uint32_t curr = texels[i].contourIDWithFlags;
      const uint32_t prevID = prev & CONTOUR_ID_MASK;
      const uint32_t currID = curr & CONTOUR_ID_MASK;
      if (currID == prevID)
      {
        continue;
      }
      if (prevID != 0 && prevID == paddingContourID)
      {
        paddingContourTexelCount = i - paddingContourBegin;
      }
      if (currID < desc.contourCount && pathIDOf(currID) == 0)
      {
        if (paddingContourID != 0)
        {
          FAIL_CHECK(
              "two contours with no path: " << paddingContourID << " and "
                                            << currID);
          return;
        }
        paddingContourID = currID;
        paddingContourBegin = i;
      }
      if (currID != prevID + 1)
      {
        FAIL_CHECK(
            "contour ID doesn't step by 1:\n  " << describe(i - 1) << "\n  "
                                                << describe(i));
        return;
      }
      if (!(prev & TESS_FLAG_LAST_IN_MEMORY))
      {
        FAIL_CHECK(
            "no LAST before an ID increment:\n  " << describe(i - 1) << "\n  "
                                                  << describe(i));
        return;
      }
      if (!(curr & TESS_FLAG_FIRST_IN_MEMORY))
      {
        FAIL_CHECK(
            "no FIRST after an ID increment:\n  " << describe(i - 1) << "\n  "
                                                  << describe(i));
        return;
      }
      if (prevID != 0 && !checkVertexIndex0(prevID, contourBegin, i - 1))
      {
        return;
      }
      contourBegin = i;
    }
    // The final padding vertex takes the last ID, after every contour and the
    // padding between the patch types. Like that padding, its record has no
    // path, since the tessellator loads it.
    CHECK(
        (texels[end - 1].contourIDWithFlags & CONTOUR_ID_MASK) ==
        desc.contourCount);
    CHECK(pathIDOf(desc.contourCount) == 0);
    // The padding between patch types is always there, and at least 2 texels
    // (so its FIRST and LAST land on different ones).
    if (paddingContourID == 0)
    {
      FAIL_CHECK("no padding contour between the patch types");
      return;
    }
    CHECK(paddingContourTexelCount >= 2);
    // Midpoint-fan contours take the IDs below it, outer cubics the ones above.
    m_mixedPatchFlushCount += paddingContourID > 1 &&
                              paddingContourID + 1 < desc.contourCount;
  }

  const TessVertexSpan* m_mappedSpans = nullptr;
  const uint32_t* m_mappedContours = nullptr;
  std::vector<uint32_t> m_contourWords;
  const uint32_t* m_mappedPaths = nullptr;
  std::vector<uint32_t> m_pathWords;
  size_t m_pathContourCount = 0;
  size_t m_featheredContourCount = 0;
  size_t m_mirroredFeatheredContourCount = 0;
  size_t m_mirroredDepthStencilContourCount = 0;
  std::vector<TessVertexSpan> m_spans;
  std::set<DrawType> m_drawTypes;
  size_t m_tessFlushCount = 0;
  size_t m_mixedPatchFlushCount = 0;
};

// A closed loop of cubics around (cx, cy), wobbling in and out.
void addBlob(
    RenderPath* path,
    float cx,
    float cy,
    float r,
    int n,
    bool counterclockwise)
{
  const float dir = counterclockwise ? -1.f : 1.f;
  auto pt = [&](float theta, float radius) {
    return Vec2D{
        cx + radius * std::cos(dir * theta),
        cy + radius * std::sin(dir * theta)};
  };
  const float step = 2 * PI / n;
  Vec2D p0 = pt(0, r);
  path->moveTo(p0.x, p0.y);
  for (int i = 0; i < n; ++i)
  {
    const float t = i * step;
    const Vec2D c0 = pt(t + step / 3, r * 1.3f);
    const Vec2D c1 = pt(t + step * 2 / 3, r * .7f);
    const Vec2D p1 = pt(t + step, r);
    path->cubicTo(c0.x, c0.y, c1.x, c1.y, p1.x, p1.y);
  }
  path->close();
}

void addPolygon(
    RenderPath* path,
    float cx,
    float cy,
    float r,
    int n,
    bool counterclockwise)
{
  const float dir = counterclockwise ? -1.f : 1.f;
  for (int i = 0; i < n; ++i)
  {
    const float theta = dir * 2 * PI * i / n;
    // Alternate radii so the interior isn't a single fan.
    const float radius = (i & 1) ? r * .8f : r;
    const float x = cx + radius * std::cos(theta);
    const float y = cy + radius * std::sin(theta);
    if (i == 0)
      path->moveTo(x, y);
    else
      path->lineTo(x, y);
  }
  path->close();
}

rcp<RenderPaint> makeFill(RenderContext* ctx, float feather = 0)
{
  auto paint = ctx->makeRenderPaint();
  paint->color(0x80336699);
  paint->feather(feather);
  return paint;
}

rcp<RenderPaint> makeStroke(
    RenderContext* ctx,
    float thickness,
    StrokeJoin join,
    StrokeCap cap,
    float feather = 0)
{
  auto paint = ctx->makeRenderPaint();
  paint->style(RenderPaintStyle::stroke);
  paint->color(0x80996633);
  paint->thickness(thickness);
  paint->join(join);
  paint->cap(cap);
  paint->feather(feather);
  return paint;
}

// Forward and mirrored paths, midpoint fans and (when the frame triangulates)
// outer cubics, and every flavor of empty path and empty contour.
void drawScene(RenderContext* ctx, RiveRenderer* renderer)
{
  auto fill = makeFill(ctx);
  auto featherFill = makeFill(ctx, 12);
  auto draw = [&](RenderPaint* paint, auto&& build) {
    auto path = ctx->makeEmptyRenderPath();
    build(path.get());
    renderer->drawPath(path.get(), paint);
  };

  // Midpoint fans: forward, then counterclockwise (mirrored).
  draw(fill.get(), [](RenderPath* p) { addBlob(p, 120, 120, 60, 7, false); });
  draw(fill.get(), [](RenderPath* p) { addBlob(p, 260, 120, 60, 7, true); });

  // Empty contours between, before, and after real ones.
  draw(fill.get(), [](RenderPath* p) {
    p->moveTo(10, 10);
    p->moveTo(20, 20);
    p->close();
    addBlob(p, 120, 260, 50, 5, false);
    p->moveTo(50, 50);
    addBlob(p, 120, 260, 30, 5, true);
    p->moveTo(5, 5);
  });

  // Mirrored by a left-handed matrix.
  renderer->save();
  renderer->transform(Mat2D(-1, 0, 0, 1, 520, 0));
  draw(fill.get(), [](RenderPath* p) {
    p->fillRule(FillRule::clockwise);
    addBlob(p, 120, 400, 50, 6, false);
  });
  renderer->restore();

  // Large enough to triangulate (outer cubics) when the frame allows it:
  // forward with empty contours around it, a polygon (no edge patches in
  // depthStencil, only interior and grout), and mirrored by the matrix.
  draw(fill.get(), [](RenderPath* p) {
    p->moveTo(1, 1);
    addBlob(p, 512, 512, 380, 9, false);
    p->moveTo(2, 2);
  });
  draw(fill.get(), [](RenderPath* p) {
    addPolygon(p, 512, 512, 400, 24, true);
    p->moveTo(3, 3);
  });
  renderer->save();
  renderer->transform(Mat2D(-1, 0, 0, 1, TargetSize, 0));
  draw(fill.get(), [](RenderPath* p) {
    p->fillRule(FillRule::clockwise);
    addBlob(p, 512, 512, 360, 11, false);
    p->moveTo(4, 4);
    addPolygon(p, 512, 512, 100, 8, false);
    p->moveTo(5, 5);
  });
  renderer->restore();

  // Ending in bare moveTos: midpoint fans, both directions, and a mirrored
  // triangulated polygon (whose interior and grout patches can't attach to
  // the trailing moveTos).
  draw(fill.get(), [](RenderPath* p) {
    addBlob(p, 400, 120, 40, 5, false);
    p->moveTo(6, 6);
    p->moveTo(7, 7);
  });
  draw(fill.get(), [](RenderPath* p) {
    addBlob(p, 400, 220, 40, 5, true);
    p->moveTo(6, 6);
    p->close();
    p->moveTo(7, 7);
  });
  renderer->save();
  renderer->transform(Mat2D(1, 0, 0, -1, 0, TargetSize));
  draw(fill.get(), [](RenderPath* p) {
    p->fillRule(FillRule::clockwise);
    addPolygon(p, 512, 512, 420, 30, false);
    p->moveTo(8, 8);
    p->moveTo(9, 9);
  });
  renderer->restore();

  // Fills that draw nothing.
  draw(fill.get(), [](RenderPath*) {});
  draw(fill.get(), [](RenderPath* p) { p->moveTo(30, 30); });
  draw(fill.get(), [](RenderPath* p) {
    p->moveTo(30, 30);
    p->close();
  });
  draw(fill.get(), [](RenderPath* p) {
    p->moveTo(30, 30);
    p->moveTo(40, 40);
    p->close();
    p->moveTo(50, 50);
  });
  // A degenerate (but not empty) fill.
  draw(fill.get(), [](RenderPath* p) {
    p->moveTo(60, 60);
    p->lineTo(60, 60);
  });

  // Strokes, open and closed, with empty contours that do and don't draw
  // caps. An epsilon width takes round joins and caps down to their minimum
  // segment counts.
  for (float thickness : {9.f, math::EPSILON})
  {
    for (StrokeCap cap : {StrokeCap::butt, StrokeCap::round, StrokeCap::square})
    {
      for (StrokeJoin join :
           {StrokeJoin::miter, StrokeJoin::round, StrokeJoin::bevel})
      {
        auto stroke = makeStroke(ctx, thickness, join, cap);
        const float x = 600 + 120 * static_cast<int>(cap);
        const float y = 600 + 120 * static_cast<int>(join);
        draw(stroke.get(), [&](RenderPath* p) {
          p->moveTo(x, y);
          p->cubicTo(x + 60, y - 40, x + 20, y + 80, x + 90, y + 30);
          p->moveTo(x + 10, y + 10); // Empty, open.
          p->moveTo(x + 20, y + 20); // Empty, closed.
          p->close();
          addBlob(p, x + 50, y + 50, 20, 3, (static_cast<int>(join) & 1) != 0);
          p->moveTo(x + 30, y + 30);
        });
        // A near-180-degree join.
        draw(stroke.get(), [&](RenderPath* p) {
          p->moveTo(x, y + 90);
          p->lineTo(x + 80, y + 90);
          p->lineTo(x, y + 91);
        });
        // Only empty contours: draws caps, or nothing.
        draw(stroke.get(), [&](RenderPath* p) { p->moveTo(x, y); });
        draw(stroke.get(), [&](RenderPath* p) {
          p->moveTo(x, y);
          p->close();
        });
        draw(stroke.get(), [&](RenderPath* p) {
          p->moveTo(x, y);
          p->moveTo(x + 10, y);
          p->close();
        });
        draw(stroke.get(), [&](RenderPath* p) {
          p->moveTo(x, y);
          p->moveTo(x + 10, y);
          p->moveTo(x + 20, y);
        });
        draw(stroke.get(), [&](RenderPath* p) {
          p->moveTo(x, y);
          p->close();
          p->moveTo(x + 10, y);
          p->close();
        });
        draw(stroke.get(), [&](RenderPath*) {});
      }
    }
  }

  // Hairlines.
  auto thin = makeStroke(ctx, .5f, StrokeJoin::miter, StrokeCap::butt);
  draw(thin.get(), [](RenderPath* p) { addBlob(p, 400, 300, 40, 5, false); });
  draw(thin.get(), [](RenderPath* p) { addBlob(p, 500, 300, 40, 5, true); });
  auto thinRound = makeStroke(ctx, .5f, StrokeJoin::round, StrokeCap::round);
  for (RenderPaint* hairline : {thin.get(), thinRound.get()})
  {
    draw(hairline, [](RenderPath* p) {
      p->moveTo(400, 400); // Empty, open.
      addBlob(p, 450, 400, 30, 4, false);
      p->moveTo(500, 400); // Empty, closed.
      p->close();
    });
    // Only empty contours.
    draw(hairline, [](RenderPath* p) {
      p->moveTo(400, 460);
      p->moveTo(420, 460);
      p->close();
    });
  }

  // Feathered fills: one contour, then several with empty contours between,
  // around, and after them, in both directions. (They must be clockwise, or
  // RiveRenderer drops them unless the frame overrides it.)
  draw(featherFill.get(), [](RenderPath* p) {
    p->fillRule(FillRule::clockwise);
    addBlob(p, 120, 700, 50, 5, false);
  });
  draw(featherFill.get(), [](RenderPath* p) {
    p->fillRule(FillRule::clockwise);
    p->moveTo(15, 15);
    addBlob(p, 120, 820, 40, 5, false);
    p->moveTo(25, 25);
    p->close();
    addBlob(p, 120, 820, 20, 4, true);
    p->moveTo(35, 35);
  });

  // Feathered strokes. A feather ignores the paint's join and cap (they're
  // always round, and caps go out as feather joins), so vary the contours
  // instead: closed, open, empty, and a sharp join, at both widths.
  for (float thickness : {6.f, math::EPSILON})
  {
    auto featherStroke = makeStroke(
        ctx,
        thickness,
        StrokeJoin::round,
        StrokeCap::round,
        /*feather=*/8);
    draw(featherStroke.get(), [](RenderPath* p) {
      addBlob(p, 260, 700, 50, 5, true);
    });
    draw(featherStroke.get(), [](RenderPath* p) {
      p->moveTo(200, 800);
      p->cubicTo(260, 760, 220, 880, 290, 830);
      p->moveTo(210, 810); // Empty, open.
      p->moveTo(220, 820); // Empty, closed.
      p->close();
      p->moveTo(230, 830);
    });
    draw(featherStroke.get(), [](RenderPath* p) {
      p->moveTo(200, 900);
      p->lineTo(280, 900);
      p->lineTo(200, 901);
    });
    // Only empty contours: a dot, open and closed.
    draw(featherStroke.get(), [](RenderPath* p) { p->moveTo(300, 760); });
    draw(featherStroke.get(), [](RenderPath* p) {
      p->moveTo(300, 780);
      p->close();
    });
  }

  // Clips that draw nothing, and a clip with an empty contour.
  renderer->save();
  {
    auto clip = ctx->makeEmptyRenderPath();
    clip->moveTo(10, 10);
    clip->close();
    renderer->clipPath(clip.get());
    draw(fill.get(), [](RenderPath* p) { addBlob(p, 120, 120, 60, 7, true); });
  }
  renderer->restore();
  renderer->save();
  {
    auto clip = ctx->makeEmptyRenderPath();
    clip->moveTo(10, 10);
    addBlob(clip.get(), 400, 850, 90, 6, true);
    renderer->clipPath(clip.get());
    draw(fill.get(), [](RenderPath* p) { addBlob(p, 400, 850, 80, 7, false); });
  }
  renderer->restore();

  // Clip strokes, open and closed, every join and cap.
  for (StrokeCap cap : {StrokeCap::butt, StrokeCap::round, StrokeCap::square})
  {
    for (StrokeJoin join :
         {StrokeJoin::miter, StrokeJoin::round, StrokeJoin::bevel})
    {
      const float x = 560 + 50 * static_cast<int>(cap);
      const float y = 60 + 50 * static_cast<int>(join);
      renderer->save();
      {
        auto clip = ctx->makeEmptyRenderPath();
        clip->moveTo(x, y);
        clip->lineTo(x + 30, y + 5);
        clip->lineTo(x + 5, y + 30);
        clip->moveTo(x + 10, y + 10); // Empty.
        addBlob(clip.get(), x + 20, y + 20, 10, 3, false);
        renderer->clipStroke(
            clip.get(),
            {6, join, cap, StrokePosition::center});
        draw(fill.get(), [&](RenderPath* p) {
          addBlob(p, x + 20, y + 20, 25, 5, false);
        });
      }
      renderer->restore();
      // Only empty contours: a clip of caps, or of nothing.
      renderer->save();
      {
        auto clip = ctx->makeEmptyRenderPath();
        clip->moveTo(x, y);
        clip->moveTo(x + 10, y + 10);
        clip->close();
        renderer->clipStroke(
            clip.get(),
            {6, join, cap, StrokePosition::center});
        draw(fill.get(), [&](RenderPath* p) {
          addBlob(p, x + 20, y + 20, 25, 5, false);
        });
      }
      renderer->restore();
    }
  }

  // Inner and outer strokes, which draw through clip strokes.
  for (StrokePosition position :
       {StrokePosition::inside, StrokePosition::outside})
  {
    auto stroke = makeStroke(ctx, 8, StrokeJoin::miter, StrokeCap::butt);
    stroke->strokePosition(position);
    const float y = position == StrokePosition::inside ? 260 : 360;
    draw(stroke.get(), [&](RenderPath* p) {
      addBlob(p, 620, y, 40, 5, false);
    });

    // Only bare moveTos (with nonempty bounds): the force-closed stroke draws
    // dots or nothing, but the bare path (clipped by, for inside strokes) is
    // empty.
    for (auto [join, cap] :
         {std::pair{StrokeJoin::round, StrokeCap::butt},
          std::pair{StrokeJoin::bevel, StrokeCap::round},
          std::pair{StrokeJoin::miter, StrokeCap::square}})
    {
      for (float feather : {0.f, 8.f})
      {
        auto dots = makeStroke(ctx, 8, join, cap, feather);
        dots->strokePosition(position);
        draw(dots.get(), [&](RenderPath* p) {
          p->moveTo(560, y + 50);
          p->moveTo(600, y + 70);
        });
      }
      renderer->save();
      {
        auto clip = ctx->makeEmptyRenderPath();
        clip->moveTo(560, y + 50);
        clip->moveTo(600, y + 70);
        renderer->clipStroke(clip.get(), {8, join, cap, position});
        draw(fill.get(), [&](RenderPath* p) {
          addBlob(p, 580, y + 60, 30, 5, false);
        });
      }
      renderer->restore();
    }
  }
}

std::unique_ptr<RenderContext> drawFrame(
    const RenderContext::FrameDescriptor& frameDesc,
    const Mat2D& sceneMatrix,
    TessCapturingRenderContext** capture,
    const std::function<void(RenderContext*, RiveRenderer*)>& scene = drawScene)
{
  auto impl = std::make_unique<TessCapturingRenderContext>();
  *capture = impl.get();
  auto ctx = std::make_unique<RenderContext>(std::move(impl));
  ctx->beginFrame(frameDesc);
  REQUIRE(ctx->frameInterlockMode() == InterlockMode::depthStencil);
  RiveRenderer renderer(ctx.get());
  renderer.transform(sceneMatrix);
  scene(ctx.get(), &renderer);
  auto target = (*capture)->makeRenderTarget(TargetSize, TargetSize);
  ctx->flush({.renderTarget = target.get()});
  return ctx;
}
// Identity, then negative determinants, which flip every path's direction
// (forward fills turn mirrored and vice versa).
// The walk saw feathered (featherAtlas) contours, and under a left-handed
// matrix, mirrored ones of both kinds.
void checkContourKinds(
    const TessCapturingRenderContext& capture,
    const Mat2D& matrix)
{
  CHECK(capture.featheredContourCount() > 0);
  if (matrix.determinant() < 0)
  {
    CHECK(capture.mirroredFeatheredContourCount() > 0);
    CHECK(capture.mirroredDepthStencilContourCount() > 0);
  }
}

Mat2D sceneMatrix(int i)
{
  switch (i)
  {
    case 1: // Flip x.
      return Mat2D(-1, 0, 0, 1, TargetSize, 0);
    case 2: // Flip y.
      return Mat2D(1, 0, 0, -1, 0, TargetSize);
    case 3: // Rotate 90 degrees and flip.
      return Mat2D(0, 1, 1, 0, 0, 0);
  }
  return Mat2D();
}
} // namespace

TEST_CASE("tess contour IDs: inkbleed", "[tess_contour_ids]")
{
  RenderContext::FrameDescriptor frameDesc;
  frameDesc.renderTargetWidth = TargetSize;
  frameDesc.renderTargetHeight = TargetSize;
  frameDesc.msaaSampleCount = 1;
  frameDesc.clockwiseFillOverride = true;
  frameDesc.inkbleedOverride = true;
  TessCapturingRenderContext* capture;
  const Mat2D matrix = sceneMatrix(GENERATE(0, 1, 2, 3));
  INFO(
      "scene matrix: " << matrix.xx() << " " << matrix.xy() << " "
                       << matrix.yx() << " " << matrix.yy());
  auto ctx = drawFrame(frameDesc, matrix, &capture);
  CHECK(capture->tessFlushCount() > 0);
  checkContourKinds(*capture, matrix);
}

TEST_CASE("tess contour IDs: msaa with outer cubics", "[tess_contour_ids]")
{
  RenderContext::FrameDescriptor frameDesc;
  frameDesc.renderTargetWidth = TargetSize;
  frameDesc.renderTargetHeight = TargetSize;
  frameDesc.msaaSampleCount = 4;
  // Triangulate on the size guards alone, so the large fills always take
  // outer cubics.
  frameDesc.triangulationThresholds
      .frameBudgetMs = std::numeric_limits<float>::infinity();
  TessCapturingRenderContext* capture;
  const Mat2D matrix = sceneMatrix(GENERATE(0, 1, 2, 3));
  INFO(
      "scene matrix: " << matrix.xx() << " " << matrix.xy() << " "
                       << matrix.yx() << " " << matrix.yy());
  auto ctx = drawFrame(frameDesc, matrix, &capture);
  CHECK(capture->tessFlushCount() > 0);
  checkContourKinds(*capture, matrix);
  CHECK(
      (capture->sawDrawType(DrawType::stencilDynamicOuterCubics) ||
       capture->sawDrawType(DrawType::stencilOuterCubics) ||
       capture->sawDrawType(DrawType::stencilOuterCubicWinding)));
  // At least one flush has both patch types, so the walk crosses the padding
  // between them.
  CHECK(capture->mixedPatchFlushCount() > 0);
}

namespace
{
RenderContext::FrameDescriptor inkbleedFrame()
{
  RenderContext::FrameDescriptor frameDesc;
  frameDesc.renderTargetWidth = TargetSize;
  frameDesc.renderTargetHeight = TargetSize;
  frameDesc.msaaSampleCount = 1;
  frameDesc.inkbleedOverride = true;
  return frameDesc;
}

// Bare moveTos (with nonempty bounds) as a path.
rcp<RenderPath> makeBareMoveTos(RenderContext* ctx)
{
  auto path = ctx->makeEmptyRenderPath();
  path->moveTo(100, 100);
  path->moveTo(200, 150);
  return path;
}
} // namespace

TEST_CASE(
    "tess contour IDs: outside strokes of bare moveTos draw dots",
    "[tess_contour_ids]")
{
  // Inside and outside strokes render with a force-closed stroke, so an empty
  // contour takes its cap from the join: round and miter joins draw dots even
  // with butt caps. An outside stroke shows them; an inside stroke clips them
  // to the (empty) fill.
  const StrokePosition position = GENERATE(
      StrokePosition::outside,
      StrokePosition::inside);
  const float feather = GENERATE(0.f, 8.f);
  const StrokeJoin join = GENERATE(StrokeJoin::round, StrokeJoin::miter);
  INFO(
      "position " << static_cast<int>(position) << ", feather " << feather
                  << ", join " << static_cast<int>(join));
  TessCapturingRenderContext* capture;
  auto ctx = drawFrame(
      inkbleedFrame(),
      Mat2D(),
      &capture,
      [=](RenderContext* ctx, RiveRenderer* renderer) {
        auto stroke = makeStroke(ctx, 8, join, StrokeCap::butt, feather);
        stroke->strokePosition(position);
        renderer->drawPath(makeBareMoveTos(ctx).get(), stroke.get());
      });
  if (position == StrokePosition::outside)
  {
    CHECK(capture->pathContourCount() > 0);
  }
  else
  {
    CHECK(capture->pathContourCount() == 0);
  }
}

TEST_CASE(
    "tess contour IDs: feathered fill of zero-length segments",
    "[tess_contour_ids]")
{
  // Segments that start and end on the same point pass the "has segments"
  // check, but softening a feathered fill prunes them, leaving nothing to
  // tessellate. (addRawPath() keeps them, unlike lineTo().)
  TessCapturingRenderContext* capture;
  auto ctx = drawFrame(
      inkbleedFrame(),
      Mat2D(),
      &capture,
      [](RenderContext* ctx, RiveRenderer* renderer) {
        RawPath rawPath;
        rawPath.move({100, 100});
        rawPath.line({100, 100});
        rawPath.move({200, 150});
        rawPath.line({200, 150});
        auto path = ctx->makeEmptyRenderPath();
        path->fillRule(FillRule::clockwise);
        path->addRawPath(rawPath);
        auto featherFill = makeFill(ctx, 12);
        renderer->drawPath(path.get(), featherFill.get());
      });
  CHECK(capture->pathContourCount() == 0);
}

TEST_CASE(
    "tess contour IDs: feathered outside stroke, float-singular matrix",
    "[tess_contour_ids]")
{
  // The determinant is nonzero in double (so the draw proceeds) but
  // underflows in float, so inverting the path for the outside stroke's clip
  // comes back empty.
  TessCapturingRenderContext* capture;
  auto ctx = drawFrame(
      inkbleedFrame(),
      Mat2D(1e-25f, 0, 0, 1e-25f, 0, 0),
      &capture,
      [](RenderContext* ctx, RiveRenderer* renderer) {
        auto
            stroke = makeStroke(ctx, 8, StrokeJoin::round, StrokeCap::round, 8);
        stroke->strokePosition(StrokePosition::outside);
        auto path = ctx->makeEmptyRenderPath();
        addBlob(path.get(), 100, 100, 50, 5, false);
        renderer->drawPath(path.get(), stroke.get());
      });
  CHECK(capture->pathContourCount() == 0);
}
