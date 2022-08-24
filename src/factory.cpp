/*
 * Copyright 2022 Rive
 */

#include "rive/factory.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/raw_path.hpp"

using namespace rive;

std::unique_ptr<RenderPath> Factory::makeRenderPath(const AABB& r) {
    const Vec2D pts[] = {
        {r.left(), r.top()},
        {r.right(), r.top()},
        {r.right(), r.bottom()},
        {r.left(), r.bottom()},
    };
    const PathVerb verbs[] = {
        PathVerb::move,
        PathVerb::line,
        PathVerb::line,
        PathVerb::line,
        PathVerb::close,
    };
    return this->makeRenderPath(pts, verbs, FillRule::nonZero);
}

std::unique_ptr<RenderPath> Factory::makeRenderPath(const RawPath& rp, FillRule fill) {
    return this->makeRenderPath(rp.points(), rp.verbs(), fill);
}
