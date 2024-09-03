/*
 * Copyright 2023 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer.hpp"

using namespace rivegm;
using namespace rive;

// The fill rule is allowed to mutate during the artboard draw process. Make sure this works as
// expected when renderers defer their drawing.
class MutatingFillRuleGM : public GM
{
public:
    MutatingFillRuleGM() : GM(600, 300, "mutating_fill_rule") {}

    void onDraw(rive::Renderer* renderer) override
    {
        Path path;
        path_addOval(path, {10, 10, 290, 290});
        path_addOval(path, {50, 50, 250, 250});
        path->fillRule(FillRule::evenOdd);

        Paint paint;
        paint->color(0xffff4040);

        renderer->drawPath(path, paint);

        renderer->translate(300, 0);
        renderer->clipPath(path);

        // Simulate the fill rule mutating during the artboard draw process.
        path->fillRule(FillRule::nonZero);

        renderer->drawPath(PathBuilder::Rect({0, 0, 300, 300}), paint);
    }
};

GMREGISTER(return new MutatingFillRuleGM())
