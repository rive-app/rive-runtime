/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_HITTEST_HPP_
#define _RIVE_HITTEST_HPP_

#include "rive/span.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/path_types.hpp"
#include "rive/math/vec2d.hpp"

#include <cstdint>
#include <vector>

namespace rive
{

class HitTester
{
  std::vector<int> m_DW; // width * height delta-windings
  Vec2D m_First, m_Prev;
  Vec2D m_offset;
  float m_height;
  int m_IWidth, m_IHeight;
  bool m_ExpectsMove;

  void recurse_cubic(Vec2D b, Vec2D c, Vec2D d, int count);

public:
  HitTester() {}
  HitTester(const IAABB& area) { reset(area); }

  void reset();
  void reset(const IAABB& area);

  void move(Vec2D);
  void line(Vec2D);
  void quad(Vec2D, Vec2D);
  void cubic(Vec2D, Vec2D, Vec2D);
  void close();

  void addRect(const AABB&, const Mat2D&, PathDirection = PathDirection::ccw);

  bool test(FillRule = rive::FillRule::nonZero);

  static bool testMesh(Vec2D point, Span<Vec2D> verts, Span<uint16_t> indices);
  static bool testMesh(const IAABB&, Span<Vec2D> verts, Span<uint16_t> indices);

  // Finds `point` in the triangles of `from` and returns the matching point
  // of `to`, which shares `indices` (from positions to UVs, or back). The
  // topmost triangle holding the point wins; when none does, `inside` is
  // false and the nearest triangle is extended past its edges. False when
  // there's no usable triangle. `map`, when given, is set to the affine map
  // of the triangle used, from `from` to `to`.
  static bool mapMesh(
      Vec2D point,
      Span<const Vec2D> from,
      Span<const Vec2D> to,
      Span<const uint16_t> indices,
      Vec2D* out,
      bool* inside,
      Mat2D* map = nullptr);
};

} // namespace rive
#endif
