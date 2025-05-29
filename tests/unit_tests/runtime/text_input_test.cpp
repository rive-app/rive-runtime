#ifdef WITH_RIVE_TEXT
#include "rive/text/cursor.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/text_input.hpp"
#include "rive/text/text_input_drawable.hpp"
#include "rive/text/text_input_text.hpp"
#include "rive/text/text_input_cursor.hpp"
#include "rive/text/text_input_selection.hpp"
#include "rive/text/text_input_selected_text.hpp"
#include "rive_testing.hpp"
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include "utils/serializing_factory.hpp"
using namespace rive;

TEST_CASE("file with text input loads correctly", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    CHECK(file != nullptr);
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard->objects<TextInput>().size() == 1);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    CHECK(textInput->children<TextInputDrawable>().size() == 4);
    CHECK(textInput->children<TextInputText>().size() == 1);
    CHECK(textInput->children<TextInputSelection>().size() == 1);
    CHECK(textInput->children<TextInputCursor>().size() == 1);
    CHECK(textInput->children<TextInputSelectedText>().size() == 1);
}

TEST_CASE("file with text input renders correctly", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/text_input.riv", &silver);
    auto artboard = file->artboardNamed("Text Input - Multiline");
    silver.frameSize(artboard->width(), artboard->height());

    artboard->advance(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    CHECK(silver.matches("text_input"));
}
#endif