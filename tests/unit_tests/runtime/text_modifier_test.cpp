#include "rive/text/text.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text/text_modifier_group.hpp"
#include "rive/text/text_modifier_range.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <utils/no_op_renderer.hpp>
#include <utils/serializing_factory.hpp>

TEST_CASE("text modifiers load correctly", "[text]")
{
    auto file = ReadRiveFile("assets/modifier_test.riv");
    auto artboard = file->artboard();

    auto textObjects = artboard->find<rive::Text>();
    REQUIRE(textObjects.size() == 1);

    auto text = textObjects[0];
    REQUIRE(text->modifierGroups().size() == 1);

    auto modifierGroup = text->modifierGroups()[0];
    REQUIRE(modifierGroup->ranges().size() == 1);
    REQUIRE(modifierGroup->modifiers().size() == 0);

    auto range = modifierGroup->ranges()[0];
    REQUIRE(range->interpolator() != nullptr);
}

TEST_CASE("text opacity falloff with feathered and plain fills", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/text_feather_falloff.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    // Validate the asset still exercises the repro: at least one text driven
    // by a modifier range, and at least one style with two fills of which
    // exactly one is feathered.
    auto textObjects = artboard->find<rive::Text>();
    REQUIRE(textObjects.size() == 3);
    int textsWithModifiers = 0;
    for (auto text : textObjects)
    {
        if (!text->modifierGroups().empty() &&
            !text->modifierGroups()[0]->ranges().empty())
        {
            textsWithModifiers++;
        }
    }
    REQUIRE(textsWithModifiers == 2);

    auto styles = artboard->find<rive::TextStylePaint>();
    int reproStyles = 0;
    for (auto style : styles)
    {
        auto& shapePaints = style->shapePaints();
        if (shapePaints.size() != 2)
        {
            continue;
        }
        int featheredCount = 0;
        for (auto shapePaint : shapePaints)
        {
            if (shapePaint->feather() != nullptr)
            {
                featheredCount++;
            }
        }
        if (featheredCount == 1)
        {
            reproStyles++;
        }
    }
    REQUIRE(reproStyles >= 1);

    silver.frameSize(artboard->width(), artboard->height());

    REQUIRE(artboard->animationCount() > 0);
    auto animation = artboard->animationAt(0);
    animation->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    // Draw every frame of the animation.
    uint32_t frames = animation->duration();
    float frameDuration = 1.0f / animation->fps();
    for (uint32_t i = 0; i < frames; i++)
    {
        silver.addFrame();
        animation->advanceAndApply(frameDuration);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("text_feather_falloff"));
}

TEST_CASE("inner feather on text rebuilds as modifiers change the glyphs",
          "[text]")
{
    auto file = ReadRiveFile("assets/text_feather_falloff.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    // A feathered fill on a text that also carries a modifier group: the
    // modifier is what we drive, so both have to hang off the same text.
    rive::Feather* feather = nullptr;
    rive::TextModifierGroup* modifierGroup = nullptr;
    for (auto style : artboard->find<rive::TextStylePaint>())
    {
        auto parent = style->parent();
        if (parent == nullptr || !parent->is<rive::Text>() ||
            parent->as<rive::Text>()->modifierGroups().empty())
        {
            continue;
        }
        for (auto shapePaint : style->shapePaints())
        {
            if (shapePaint->is<rive::Fill>() &&
                shapePaint->feather() != nullptr)
            {
                feather = shapePaint->feather();
                modifierGroup = parent->as<rive::Text>()->modifierGroups()[0];
                break;
            }
        }
        if (feather != nullptr)
        {
            break;
        }
    }
    REQUIRE(feather != nullptr);
    REQUIRE(modifierGroup != nullptr);

    // Flip to inner so the feather has to derive its path from the glyphs.
    feather->inner(true);
    REQUIRE(feather->isInner());

    artboard->advance(0.0f);
    int baseline = feather->renderCount;
    REQUIRE(baseline > 0);

    // The reported case: a modifier transform moves the glyphs. Text only
    // marks itself dirty for that, so the inner feather has to be dirtied
    // along with the style's paints or it keeps the old glyph geometry.
    modifierGroup->x(modifierGroup->x() + 10.0f);
    artboard->advance(0.0f);

    CHECK(feather->renderCount > baseline);
}
