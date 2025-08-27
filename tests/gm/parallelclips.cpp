/*
 * Copyright 2024 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/math_types.hpp"

using namespace rivegm;

namespace rive::gpu
{
// Draws non-overlapping clips, which will get reordered and batched together in
// atomic and msaa modes.
class ParallelClipsGM : public GM
{
public:
    ParallelClipsGM() : GM(850, 850) {}

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(Renderer* renderer) override
    {
        for (uint32_t y = 0; y < 7; ++y)
        {
            renderer->save();
            for (uint32_t x = 0; x < 7; ++x)
            {
                bool nested = (x ^ y) & 1;
                renderer->save();
                {
                    PathBuilder clip;
                    clip.addRect({10, 10, 110, 110},
                                 rivegm::PathDirection::counterclockwise);
                    clip.moveTo(cos(math::PI / 2) * 30 + 60,
                                sin(math::PI / 2) * 30 + 60)
                        .lineTo(cos(2 * math::PI / 3 + math::PI / 2) * 30 + 60,
                                sin(2 * math::PI / 3 + math::PI / 2) * 30 + 60)
                        .lineTo(cos(4 * math::PI / 3 + math::PI / 2) * 30 + 60,
                                sin(4 * math::PI / 3 + math::PI / 2) * 30 + 60);
                    renderer->clipPath(clip.detach());
                }
                if (nested)
                {
                    PathBuilder clip;
                    clip.addRect({10, 10, 110, 110},
                                 rivegm::PathDirection::counterclockwise);
                    clip.addRect({55, 10, 65, 110},
                                 rivegm::PathDirection::clockwise);
                    renderer->clipPath(clip.detach());
                }
                if (nested)
                {
                    PathBuilder clip;
                    clip.addRect({10, 10, 110, 110},
                                 rivegm::PathDirection::counterclockwise);
                    clip.addRect({10, 55, 110, 65},
                                 rivegm::PathDirection::clockwise);
                    renderer->clipPath(clip.detach());
                }
                renderer->drawPath(
                    PathBuilder::Circle(60, 60, 50),
                    Paint(((y * x + 123458383u) * 285018463u) | 0xff808080));
                renderer->restore();
                renderer->translate(120, 0);
            }
            renderer->restore();
            renderer->translate(0, 120);
        }
    }
};
GMREGISTER(parallelclips, return new ParallelClipsGM)
} // namespace rive::gpu
