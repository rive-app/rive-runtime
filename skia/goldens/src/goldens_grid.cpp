#ifdef TESTING

// Don't compile this file as part of the "tests" project.

#else

#include "goldens_grid.hpp"

#include "rive_mgr.hpp"
#include "rive/animation/animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/artboard.hpp"

void RenderGoldensGrid(rive::Renderer* renderer,
                       const char* source,
                       const char* artboardName,
                       const char* animationName) {
    RiveMgr mgr;
    if (!mgr.load(source, artboardName, animationName)) {
        throw "Can't load animation";
    }
    auto artboard = mgr.artboard();
    if (!artboard) {
        throw "No artboard";
    }
    auto animation = mgr.animation();
    if (!animation) {
        throw "No animation";
    }

    const int FRAMES = H * W;
    const double duration = animation->durationSeconds();
    const double frameDuration = duration / FRAMES;
    const rive::AABB dstBounds = rive::AABB(0, 0, CELL, CELL);

    artboard->advance(0);
    animation->advance(0);

    renderer->translate(GAP, GAP);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            renderer->save();

            animation->apply();

            renderer->translate(x * (CELL + GAP), y * (CELL + GAP));
            renderer->align(
                rive::Fit::cover, rive::Alignment::center, dstBounds, artboard->bounds());
            artboard->draw(renderer);

            animation->advance(frameDuration);
            artboard->advance(frameDuration);

            renderer->restore();
        }
    }
}

#endif
