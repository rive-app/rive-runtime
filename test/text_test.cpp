#include "rive/text/text.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <utils/no_op_renderer.hpp>

TEST_CASE("file with text loads correctly", "[text]")
{
    auto file = ReadRiveFile("../../test/assets/new_text.riv");
    auto artboard = file->artboard();

    auto textObjects = artboard->find<rive::Text>();
    REQUIRE(textObjects.size() == 5);

    auto styleObjects = artboard->find<rive::TextStyle>();
    REQUIRE(styleObjects.size() == 13);

    auto runObjects = artboard->find<rive::TextValueRun>();
    REQUIRE(runObjects.size() == 22);

    artboard->advance(0.0f);
    rive::NoOpRenderer renderer;
    artboard->draw(&renderer);
}

TEST_CASE("simple text loads", "[text]")
{
    auto file = ReadRiveFile("../../test/assets/hello_world.riv");
    auto artboard = file->artboard();

    auto textObjects = artboard->find<rive::Text>();
    REQUIRE(textObjects.size() == 1);

    auto styleObjects = artboard->find<rive::TextStyle>();
    REQUIRE(styleObjects.size() == 1);

    auto runObjects = artboard->find<rive::TextValueRun>();
    REQUIRE(runObjects.size() == 1);

    artboard->advance(0.0f);
    rive::NoOpRenderer renderer;
    artboard->draw(&renderer);
}