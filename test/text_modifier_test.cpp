#include "rive/text/text.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text/text_modifier_group.hpp"
#include "rive/text/text_modifier_range.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <utils/no_op_renderer.hpp>

TEST_CASE("text modifiers load correctly", "[text]")
{
    auto file = ReadRiveFile("../../test/assets/modifier_test.riv");
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
