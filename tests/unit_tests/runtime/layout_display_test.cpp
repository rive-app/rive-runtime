#include "rive/artboard.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout_component.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// A layout keeps whether its style hides it in a flag, since isCollapsed()
// and isHidden() run for every layout on every advance and draw. The flag has
// to follow the style's display the moment it changes.
TEST_CASE("a layout hidden through its style reads as collapsed at once",
          "[layout]")
{
    auto file = ReadRiveFile("assets/layout/layout_anim_bound.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);
    rive::LayoutComponent* layout = nullptr;
    for (auto* candidate : artboard->find<rive::LayoutComponent>())
    {
        if (!candidate->is<rive::Artboard>() && candidate->style() != nullptr)
        {
            layout = candidate;
            break;
        }
    }
    REQUIRE(layout != nullptr);
    // isCollapsed() is public through Component.
    rive::Component* component = layout;
    REQUIRE_FALSE(component->isCollapsed());
    REQUIRE_FALSE(layout->isHidden());

    layout->style()->displayValue(1); // none
    CHECK(component->isCollapsed());
    CHECK(layout->isHidden());

    layout->style()->displayValue(0); // flex
    CHECK_FALSE(component->isCollapsed());
    CHECK_FALSE(layout->isHidden());
}
