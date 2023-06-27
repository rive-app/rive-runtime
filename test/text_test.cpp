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

TEST_CASE("ellipsis is shown", "[text]")
{
    auto file = ReadRiveFile("../../test/assets/ellipsis.riv");
    auto artboard = file->artboard();

    auto textObjects = artboard->find<rive::Text>();
    REQUIRE(textObjects.size() == 1);

    auto styleObjects = artboard->find<rive::TextStyle>();
    REQUIRE(styleObjects.size() == 1);

    auto runObjects = artboard->find<rive::TextValueRun>();
    REQUIRE(runObjects.size() == 1);

    artboard->advance(0.0f);
    auto text = textObjects[0];
    auto lines = text->orderedLines();
    REQUIRE(lines.size() == 1);

    auto orderedLine = lines[0];
    int glyphCount = 0;
    std::vector<rive::GlyphID> glyphIds;
    // for (auto itr = orderedLine.begin(); itr != orderedLine.end(); ++itr)
    for (auto glyphItr : orderedLine)
    {
        auto run = std::get<0>(glyphItr);
        size_t glyphIndex = std::get<1>(glyphItr);
        glyphIds.push_back(run->glyphs[glyphIndex]);
        glyphCount++;
    }
    // Expect 10 glyphs 'one two...'
    REQUIRE(glyphCount == 10);
    // Third to last glyph should be the "." for Inter.
    REQUIRE(glyphIds[7] == 1405);
    // Last three glyphs should be the same '.'
    REQUIRE(glyphIds[7] == glyphIds[8]);
    REQUIRE(glyphIds[8] == glyphIds[9]);

    // now set overflow to visible.
    text->overflow(rive::TextOverflow::visible);
    artboard->advance(0.0f);
    lines = text->orderedLines();
    // 2 lines after we set overflow to visible and advance which updates the
    // ordered lines.
    REQUIRE(lines.size() == 2);

    orderedLine = lines[0];
    glyphCount = 0;
    for (auto itr = orderedLine.begin(); itr != orderedLine.end(); ++itr)
    {
        glyphCount++;
    }
    // Expect 7 glyphs 'one two' as now 'three' is on line 2.
    REQUIRE(glyphCount == 7);

    rive::NoOpRenderer renderer;
    artboard->draw(&renderer);
}