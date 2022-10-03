/*
 * Copyright 2022 Rive
 */

#include "rive/factory.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/raw_path.hpp"

using namespace rive;

std::unique_ptr<RenderPath> Factory::makeRenderPath(const AABB& r)
{
    RawPath rawPath;
    rawPath.addRect(r);
    return makeRenderPath(rawPath, FillRule::nonZero);
}
