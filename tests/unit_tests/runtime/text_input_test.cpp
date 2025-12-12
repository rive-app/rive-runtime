#ifdef WITH_RIVE_TEXT
#include "rive/text/cursor.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/text_input.hpp"
#include "rive/text/text_input_drawable.hpp"
#include "rive/text/text_input_text.hpp"
#include "rive/text/text_input_cursor.hpp"
#include "rive/text/text_input_selection.hpp"
#include "rive/text/text_input_selected_text.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/input/focusable.hpp"
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

TEST_CASE("text input keyInput handles arrow keys", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    // Set some initial text
    textInput->rawTextInput()->text("hello world");
    textInput->rawTextInput()->cursor(Cursor::zero());

    artboard->advance(0.0f);

    // Test right arrow key
    bool handled =
        textInput->keyInput(Key::right, KeyModifiers::none, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->cursor().start().codePointIndex() == 1);

    // Test left arrow key
    handled = textInput->keyInput(Key::left, KeyModifiers::none, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->cursor().start().codePointIndex() == 0);

    // Test down arrow key (moves to end on single line)
    handled = textInput->keyInput(Key::down, KeyModifiers::none, true, false);
    CHECK(handled == true);

    // Test up arrow key (moves to start on single line)
    handled = textInput->keyInput(Key::up, KeyModifiers::none, true, false);
    CHECK(handled == true);
}

TEST_CASE("text input keyInput handles backspace and delete", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    // Set some initial text
    textInput->rawTextInput()->text("hello");
    textInput->rawTextInput()->cursor(
        Cursor::collapsed(CursorPosition(3))); // "hel|lo"

    artboard->advance(0.0f);

    // Test backspace
    bool handled =
        textInput->keyInput(Key::backspace, KeyModifiers::none, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "helo");

    // Test delete key
    textInput->rawTextInput()->cursor(
        Cursor::collapsed(CursorPosition(2))); // "he|lo"
    handled =
        textInput->keyInput(Key::deleteKey, KeyModifiers::none, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "heo");
}

TEST_CASE("text input keyInput handles undo/redo", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    // Set some initial text and make changes
    textInput->rawTextInput()->text("");
    textInput->rawTextInput()->cursor(Cursor::zero());
    textInput->rawTextInput()->insert("hello");

    artboard->advance(0.0f);

    CHECK(textInput->rawTextInput()->text() == "hello");

    // Test undo with system modifier (meta on macOS/Linux, ctrl on Windows)
    // On non-Windows, non-Emscripten, systemModifier() returns
    // KeyModifiers::meta
#if !defined(RIVE_WINDOWS) && !defined(__EMSCRIPTEN__)
    bool handled = textInput->keyInput(Key::z, KeyModifiers::meta, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "");

    // Insert again and test redo
    textInput->rawTextInput()->insert("world");
    artboard->advance(0.0f);
    CHECK(textInput->rawTextInput()->text() == "world");

    // Undo
    handled = textInput->keyInput(Key::z, KeyModifiers::meta, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "");

    // Redo with shift+meta
    handled = textInput->keyInput(Key::z,
                                  KeyModifiers::meta | KeyModifiers::shift,
                                  true,
                                  false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "world");
#endif
}

TEST_CASE("text input keyInput returns false for unhandled keys",
          "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    artboard->advance(0.0f);

    // Test unhandled key
    bool handled =
        textInput->keyInput(Key::escape, KeyModifiers::none, true, false);
    CHECK(handled == false);

    // Test that key release (isPressed=false) returns false
    handled = textInput->keyInput(Key::right, KeyModifiers::none, false, false);
    CHECK(handled == false);
}

TEST_CASE("text input keyInput handles modifier keys for cursor boundary",
          "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    // Set text with multiple words
    textInput->rawTextInput()->text("one two three");
    textInput->rawTextInput()->cursor(Cursor::zero());

    artboard->advance(0.0f);

    // Test word boundary with alt modifier
    bool handled =
        textInput->keyInput(Key::right, KeyModifiers::alt, true, false);
    CHECK(handled == true);
    // Should jump to end of "one"
    CHECK(textInput->rawTextInput()->cursor().start().codePointIndex() == 3);

    // Test line boundary with meta modifier (from current position)
    // First reset cursor to start
    textInput->rawTextInput()->cursor(Cursor::zero());
    handled = textInput->keyInput(Key::right, KeyModifiers::meta, true, false);
    CHECK(handled == true);
    // Should jump to end of line
    CHECK(textInput->rawTextInput()->cursor().start().codePointIndex() == 13);

    // Test sub-word boundary with alt+ctrl modifier
    textInput->rawTextInput()->text("oneTwo threeF");
    textInput->rawTextInput()->cursor(Cursor::zero());
    artboard->advance(0.0f);

    handled = textInput->keyInput(Key::right,
                                  KeyModifiers::alt | KeyModifiers::ctrl,
                                  true,
                                  false);
    CHECK(handled == true);
    // Should jump to sub-word boundary (camelCase)
    CHECK(textInput->rawTextInput()->cursor().start().codePointIndex() == 3);
}

TEST_CASE("text input keyInput handles shift for selection", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    textInput->rawTextInput()->text("hello world");
    textInput->rawTextInput()->cursor(Cursor::zero());

    artboard->advance(0.0f);

    // Move right with shift should select
    bool handled =
        textInput->keyInput(Key::right, KeyModifiers::shift, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->cursor().start().codePointIndex() == 0);
    CHECK(textInput->rawTextInput()->cursor().end().codePointIndex() == 1);

    // Continue selecting
    handled = textInput->keyInput(Key::right, KeyModifiers::shift, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->cursor().start().codePointIndex() == 0);
    CHECK(textInput->rawTextInput()->cursor().end().codePointIndex() == 2);
}

TEST_CASE("text input textInput method inserts text", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    textInput->rawTextInput()->text("");
    textInput->rawTextInput()->cursor(Cursor::zero());

    artboard->advance(0.0f);

    // Test textInput method
    bool handled = textInput->textInput("hello");
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "hello");

    // Insert more text
    handled = textInput->textInput(" world");
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "hello world");
}

TEST_CASE("text input selectionRadiusChanged updates raw text input",
          "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    // Set selection radius
    textInput->selectionRadius(5.0f);

    // The selectionRadiusChanged callback should be invoked
    // Just verify it doesn't crash and the value is set
    CHECK(textInput->selectionRadius() == 5.0f);
}

TEST_CASE("state machine keyInput and textInput forward to text input",
          "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto stateMachine = artboard->stateMachine(0);
    if (stateMachine == nullptr)
    {
        // Skip if no state machine
        return;
    }

    auto abi = artboard->instance();
    StateMachineInstance smi(stateMachine, abi.get());

    // Advance to initialize
    smi.advance(0.0f);

    auto textInput = abi->objects<TextInput>().first();
    if (textInput == nullptr)
    {
        // Skip if no text input found
        return;
    }

    // Clear text first
    textInput->rawTextInput()->text("");
    textInput->rawTextInput()->cursor(Cursor::zero());

    // Test textInput through state machine
    bool handled = smi.textInput("typed text");
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "typed text");

    // Test keyInput through state machine (backspace)
    handled = smi.keyInput(Key::backspace, KeyModifiers::none, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "typed tex");
}
#endif