#include "rive/text/text.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "rive/text/glyph_lookup.hpp"
#include "rive/text/text_modifier_range.hpp"
#include "utils/no_op_renderer.hpp"
#include "rive/text/utf.hpp"
#include "rive/text/text_modifier_group.hpp"

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

TEST_CASE("can query for all text runs", "[text]")
{
    auto file = ReadRiveFile("../../test/assets/new_text.riv");
    auto artboard = file->artboard();

    auto textRunCount = artboard->count<rive::TextValueRun>();
    REQUIRE(textRunCount == 22);
}

TEST_CASE("can query for a text run at a given index", "[text]")
{
    auto file = ReadRiveFile("../../test/assets/hello_world.riv");
    auto artboard = file->artboard();

    auto textRun = artboard->objectAt<rive::TextValueRun>(0);
    REQUIRE(textRun->text() == "Hello World!");
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
    REQUIRE(runObjects[0]->text() == "Hello World!");

    rive::NoOpRenderer renderer;

    artboard->advance(0.0f);
    artboard->draw(&renderer);
    {
        REQUIRE(textObjects[0]->shape().size() == 1);
        const rive::Paragraph& paragraph = textObjects[0]->shape()[0];
        REQUIRE(paragraph.runs.size() == 1);
        REQUIRE(paragraph.runs[0].glyphs.size() == 12);
    }

    // Changing to "Just Hello" works.
    runObjects[0]->text("Just Hello");
    artboard->advance(0.0f);
    artboard->draw(&renderer);
    {
        REQUIRE(textObjects[0]->shape().size() == 1);
        const rive::Paragraph& paragraph = textObjects[0]->shape()[0];
        REQUIRE(paragraph.runs.size() == 1);
        REQUIRE(paragraph.runs[0].glyphs.size() == 10);
    }

    // Changing to an empty space " " works.
    runObjects[0]->text(" ");
    artboard->advance(0.0f);
    artboard->draw(&renderer);
    {
        REQUIRE(textObjects[0]->shape().size() == 1);
        const rive::Paragraph& paragraph = textObjects[0]->shape()[0];
        REQUIRE(paragraph.runs.size() == 1);
        REQUIRE(paragraph.runs[0].glyphs.size() == 1);
    }

    // Changing to completely empty works.
    runObjects[0]->text("");
    artboard->advance(0.0f);
    artboard->draw(&renderer);
    {
        REQUIRE(textObjects[0]->shape().size() == 0);
        REQUIRE(textObjects[0]->localBounds().width() == 0.0f);
        REQUIRE(textObjects[0]->localBounds().height() == 0.0f);
    }
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

    rive::GlyphLookup lookup;
    lookup.compute(text->unichars(), text->shape());
    REQUIRE(lookup.count(0) == 1);

    runObjects[0]->text("a -> b");
    artboard->advance(0.0f);
    lookup.compute(text->unichars(), text->shape());
    REQUIRE(lookup.count(0) == 1); // a
    REQUIRE(lookup.count(1) == 1); // space
    REQUIRE(lookup.count(2) == 2); // - ligates > to ->
    REQUIRE(lookup.count(4) == 1); // space
    REQUIRE(lookup.count(5) == 1); // b
}

static std::vector<rive::Unichar> toUnicode(const char text[])
{
    std::vector<rive::Unichar> codePoints;
    const uint8_t* ptr = (const uint8_t*)text;
    while (*ptr)
    {
        codePoints.push_back(rive::UTF::NextUTF8(&ptr));
    }
    return codePoints;
}

TEST_CASE("range mapper maps words", "[text]")
{
    auto codePoints = toUnicode("one two three four");
    rive::RangeMapper rangeMapper;
    rangeMapper.fromWords(codePoints, 0, (uint32_t)codePoints.size());
    REQUIRE(rangeMapper.unitCount() == 4);
}

TEST_CASE("run modifier ranges select runs", "[text]")
{
    auto file = ReadRiveFile("../../test/assets/modifier_to_run.riv");
    auto artboard = file->artboard();

    artboard->advance(0.0f);
    rive::NoOpRenderer renderer;
    artboard->draw(&renderer);

    {
        auto characterSelectedText = artboard->find<rive::Text>("Characters");
        REQUIRE(characterSelectedText != nullptr);
        REQUIRE(characterSelectedText->haveModifiers());
        REQUIRE(characterSelectedText->modifierGroups().size() == 2);
        auto firstModifierGroup = characterSelectedText->modifierGroups()[0];
        REQUIRE(firstModifierGroup->ranges().size() == 1);
        auto firstRange = firstModifierGroup->ranges()[0];
        REQUIRE(firstRange->run() != nullptr);
        for (int i = 0; i < 4; i++)
        {
            REQUIRE(firstModifierGroup->coverage(i) == 0.0f);
        }
        // Run from 4-9 got selected.
        REQUIRE(firstRange->run()->offset() == 4);
        REQUIRE(firstRange->run()->length() == 5);
        for (int i = 4; i < 9; i++)
        {
            REQUIRE(firstModifierGroup->coverage(i) != 0.0f);
        }
        for (int i = 9; i < 26; i++)
        {
            REQUIRE(firstModifierGroup->coverage(i) == 0.0f);
        }
    }
    {
        auto wordSelectedText = artboard->find<rive::Text>("Words");
        REQUIRE(wordSelectedText != nullptr);
        REQUIRE(wordSelectedText->haveModifiers());
        REQUIRE(wordSelectedText->modifierGroups().size() == 1);
        auto firstModifierGroup = wordSelectedText->modifierGroups()[0];
        REQUIRE(firstModifierGroup->ranges().size() == 1);
        auto firstRange = firstModifierGroup->ranges()[0];
        REQUIRE(firstRange->run() != nullptr);
        for (int i = 0; i < 4; i++)
        {
            REQUIRE(firstModifierGroup->coverage(i) == 0.0f);
        }
        // Run from 4-34 got selected.
        REQUIRE(firstRange->run()->offset() == 4);
        auto text = firstRange->run()->text();
        REQUIRE(text.size() == 34);
        for (int i = 4; i < 39; i++)
        {
            if (rive::isWhiteSpace(text[i - 4]))
            {
                REQUIRE(firstModifierGroup->coverage(i) == 0.0f);
            }
            else
            {
                REQUIRE(firstModifierGroup->coverage(i) != 0.0f);
            }
        }
        for (int i = 39; i < 50; i++)
        {
            REQUIRE(firstModifierGroup->coverage(i) == 0.0f);
        }
    }
}

TEST_CASE("run modifier ranges select runs with varying text size", "[text]")
{
    auto file = ReadRiveFile("../../test/assets/test_modifier_run.riv");
    auto artboard = file->artboard();

    artboard->advance(0.0f);
    rive::NoOpRenderer renderer;
    artboard->draw(&renderer);

    {
        /*
        Full text is:
        text for first run[UN]line separator[UN]text for second run
        [UN] is the united nations flag
        the first run is 18 characters long
        the second run is 16 characters long
        the second run is 20 characters long
        */
        auto characterSelectedText = artboard->find<rive::Text>("MultiRunText");
        REQUIRE(characterSelectedText != nullptr);
        REQUIRE(characterSelectedText->haveModifiers());
        REQUIRE(characterSelectedText->modifierGroups().size() == 1);
        auto firstModifierGroup = characterSelectedText->modifierGroups()[0];
        REQUIRE(firstModifierGroup->ranges().size() == 1);
        auto firstRange = firstModifierGroup->ranges()[0];
        REQUIRE(firstRange->run() != nullptr);
        for (int i = 0; i < 18; i++)
        {
            REQUIRE(firstModifierGroup->coverage(i) == 0.0f);
        }
        // // Run from 18-33 got selected.
        REQUIRE(firstRange->run()->offset() == 18);
        REQUIRE(firstRange->run()->length() == 16);
        // We confirm that the size and the length of the text are different and they're
        // both correct
        REQUIRE(firstRange->run()->text().size() == 22);
        for (int i = 18; i < 34; i++)
        {
            REQUIRE(firstModifierGroup->coverage(i) != 0.0f);
        }
        for (int i = 34; i < 54; i++)
        {
            REQUIRE(firstModifierGroup->coverage(i) == 0.0f);
        }
    }
}

TEST_CASE("double new line type works", "[text]")
{
    auto file = ReadRiveFile("../../test/assets/double_line.riv");
    auto artboard = file->artboard();

    auto textObjects = artboard->find<rive::Text>();
    REQUIRE(textObjects.size() == 1);

    auto styleObjects = artboard->find<rive::TextStyle>();
    REQUIRE(styleObjects.size() == 1);

    auto runObjects = artboard->find<rive::TextValueRun>();
    REQUIRE(runObjects.size() == 9);

    artboard->advance(0.0f);
    auto text = textObjects[0];
    auto lines = text->orderedLines();
    REQUIRE(lines.size() == 3);
}
