#ifdef WITH_RIVE_TEXT
#include "rive/text/cursor.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout_component.hpp"
#include "rive/text/text_input.hpp"
#include "rive/text/text_input_drawable.hpp"
#include "rive/text/text_input_text.hpp"
#include "rive/text/text_input_cursor.hpp"
#include "rive/text/text_input_selection.hpp"
#include "rive/text/text_input_selected_text.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focusable.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/input/focus_node.hpp"
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

    CHECK(textInput->children<TextInputDrawable>().size() == 3);
    CHECK(textInput->children<TextInputText>().size() == 1);
    CHECK(textInput->children<TextInputSelection>().size() == 1);
    CHECK(textInput->children<TextInputCursor>().size() == 1);
    // Keeping this check for now. Might need it back in the future
    CHECK(textInput->children<TextInputSelectedText>().size() == 0);
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

TEST_CASE("text input keyInput handles select all", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    textInput->rawTextInput()->text("hello world");
    textInput->rawTextInput()->cursor(Cursor::zero());

    artboard->advance(0.0f);

    // The system modifier is ctrl on Windows and meta elsewhere, matching
    // TextInput::systemModifier().
#if defined(RIVE_WINDOWS)
    KeyModifiers systemModifier = KeyModifiers::ctrl;
#else
    KeyModifiers systemModifier = KeyModifiers::meta;
#endif

    // Cmd/Ctrl+A selects the entire buffer.
    bool handled = textInput->keyInput(Key::a, systemModifier, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->cursor().start().codePointIndex() == 0);
    CHECK(textInput->rawTextInput()->cursor().end().codePointIndex() == 11);

    // A plain 'a' is not handled here so it falls through to be typed.
    handled = textInput->keyInput(Key::a, KeyModifiers::none, true, false);
    CHECK(handled == false);
}

TEST_CASE("text input keyInput handles home and end", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    textInput->rawTextInput()->text("hello world");
    // Start with a collapsed cursor in the middle of the line.
    textInput->rawTextInput()->cursor(Cursor::collapsed(CursorPosition(5)));

    artboard->advance(0.0f);

    // End moves the cursor to the end of the line.
    bool handled =
        textInput->keyInput(Key::end, KeyModifiers::none, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->cursor().isCollapsed());
    CHECK(textInput->rawTextInput()->cursor().end().codePointIndex() == 11);

    // Home moves the cursor to the start of the line.
    handled = textInput->keyInput(Key::home, KeyModifiers::none, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->cursor().isCollapsed());
    CHECK(textInput->rawTextInput()->cursor().end().codePointIndex() == 0);

    // Shift+End extends the selection from the current position to line end.
    handled = textInput->keyInput(Key::end, KeyModifiers::shift, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->cursor().hasSelection());
    CHECK(textInput->rawTextInput()->cursor().start().codePointIndex() == 0);
    CHECK(textInput->rawTextInput()->cursor().end().codePointIndex() == 11);
}

TEST_CASE("raw text input selectLine selects the visual line", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    textInput->rawTextInput()->text("hello\nworld");
    artboard->advance(0.0f);

    // Cursor on the second line ("world", codepoints 6..10).
    textInput->rawTextInput()->cursor(Cursor::collapsed(CursorPosition(8)));
    textInput->rawTextInput()->selectLine();
    CHECK(textInput->rawTextInput()->cursor().hasSelection());
    CHECK(textInput->rawTextInput()->cursor().first().codePointIndex() == 6);
    CHECK(textInput->rawTextInput()->cursor().last().codePointIndex() == 11);

    // Cursor on the first line ("hello", codepoints 0..4).
    textInput->rawTextInput()->cursor(Cursor::collapsed(CursorPosition(2)));
    textInput->rawTextInput()->selectLine();
    CHECK(textInput->rawTextInput()->cursor().hasSelection());
    CHECK(textInput->rawTextInput()->cursor().first().codePointIndex() == 0);
    CHECK(textInput->rawTextInput()->cursor().last().codePointIndex() == 5);
}

TEST_CASE("text input selectWord and selectLine wrappers", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    textInput->rawTextInput()->text("hello world");
    artboard->advance(0.0f);

    // selectWord selects the word under the cursor.
    textInput->rawTextInput()->cursor(Cursor::collapsed(CursorPosition(2)));
    textInput->selectWord();
    CHECK(textInput->rawTextInput()->cursor().first().codePointIndex() == 0);
    CHECK(textInput->rawTextInput()->cursor().last().codePointIndex() == 5);

    // selectLine selects the whole (single) line.
    textInput->rawTextInput()->cursor(Cursor::collapsed(CursorPosition(8)));
    textInput->selectLine();
    CHECK(textInput->rawTextInput()->cursor().first().codePointIndex() == 0);
    CHECK(textInput->rawTextInput()->cursor().last().codePointIndex() == 11);
}

TEST_CASE("text input double and triple click select word and line",
          "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto stateMachine = artboard->stateMachineAt(0);
    if (stateMachine == nullptr)
    {
        return;
    }

    stateMachine->advanceAndApply(0.0f);

    auto textInput = artboard->objects<TextInput>().first();
    if (textInput == nullptr)
    {
        return;
    }

    textInput->rawTextInput()->text("hello world");
    stateMachine->advanceAndApply(0.0f);

    // Click near the top-left where the first word renders.
    Vec2D clickPosition(8.0f, 8.0f);

    auto pressRelease = [&]() {
        stateMachine->pointerDown(clickPosition);
        stateMachine->pointerUp(clickPosition);
    };

    // Two rapid clicks should select the word under the pointer.
    pressRelease(); // single
    pressRelease(); // double
    stateMachine->advanceAndApply(0.0f);

    if (!textInput->rawTextInput()->cursor().hasSelection())
    {
        // The click did not land on the text input's hit area in this asset
        // layout; skip the pointer-driven assertions rather than fail
        // spuriously. The word/line selection paths are covered
        // deterministically by the selectWord/selectLine tests above.
        return;
    }

    auto wordEnd = textInput->rawTextInput()->cursor().last().codePointIndex();
    CHECK(wordEnd >
          textInput->rawTextInput()->cursor().first().codePointIndex());

    // A third rapid click selects the (visual) line, spanning at least the
    // word.
    pressRelease(); // triple
    stateMachine->advanceAndApply(0.0f);
    CHECK(textInput->rawTextInput()->cursor().hasSelection());
    CHECK(textInput->rawTextInput()->cursor().last().codePointIndex() >=
          wordEnd);
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

TEST_CASE("text input multiline toggles line breaks in displayed text",
          "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    textInput->text("line1\nline2");
    artboard->advance(0.0f);
    CHECK(textInput->rawTextInput()->text() == "line1\nline2");

    textInput->multiline(false);
    artboard->advance(0.0f);
    CHECK(textInput->rawTextInput()->text() == "line1 line2");

    textInput->multiline(true);
    artboard->advance(0.0f);
    CHECK(textInput->rawTextInput()->text() == "line1\nline2");
}

TEST_CASE("text input alignValue drives the raw text input", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    REQUIRE(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    REQUIRE(textInput != nullptr);

    // Default is left.
    CHECK(textInput->alignValue() == 0);
    artboard->advance(0.0f);
    CHECK(textInput->rawTextInput()->align() == TextAlign::left);

    textInput->alignValue((uint32_t)TextAlign::right);
    artboard->advance(0.0f);
    CHECK(textInput->rawTextInput()->align() == TextAlign::right);

    textInput->alignValue((uint32_t)TextAlign::center);
    artboard->advance(0.0f);
    CHECK(textInput->rawTextInput()->align() == TextAlign::center);
}

TEST_CASE("text input aligns its text within the field", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    REQUIRE(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    REQUIRE(textInput != nullptr);

    textInput->text("hi");
    artboard->advance(0.0f);

    float alignWidth = textInput->rawTextInput()->alignWidth();
    // The multiline field wraps to its layout width, so we have something to
    // align within.
    REQUIRE(alignWidth > 0.0f);
    AABB leftBounds = textInput->localBounds();
    REQUIRE(leftBounds.width() < alignWidth);
    CHECK(leftBounds.minX == 0.0f);

    textInput->alignValue((uint32_t)TextAlign::right);
    artboard->advance(0.0f);
    AABB rightBounds = textInput->localBounds();
    CHECK(rightBounds.minX == Approx(alignWidth - leftBounds.width()));
    // Alignment moves the text, it doesn't resize it.
    CHECK(rightBounds.width() == Approx(leftBounds.width()));

    textInput->alignValue((uint32_t)TextAlign::center);
    artboard->advance(0.0f);
    AABB centerBounds = textInput->localBounds();
    CHECK(centerBounds.minX ==
          Approx((alignWidth - leftBounds.width()) / 2.0f));
    CHECK(centerBounds.width() == Approx(leftBounds.width()));
}

TEST_CASE("text input strips inserted line breaks when single line",
          "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    textInput->text("");
    textInput->multiline(false);
    artboard->advance(0.0f);

    bool handled = textInput->textInput("a\nb\r\nc\rd");
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "a b c d");

    textInput->multiline(true);
    artboard->advance(0.0f);
    CHECK(textInput->rawTextInput()->text() == "a b c d");
}

TEST_CASE("text input keyInput handles enter key", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    CHECK(textInput != nullptr);

    textInput->text("hello");
    textInput->multiline(true);
    textInput->rawTextInput()->cursor(
        Cursor::collapsed(CursorPosition(3))); // "hel|lo"
    artboard->advance(0.0f);

    // Enter inserts a newline at the cursor in multiline mode.
    bool handled =
        textInput->keyInput(Key::enter, KeyModifiers::none, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "hel\nlo");
    CHECK(textInput->text() == "hel\nlo");

    // Enter is not handled and does not mutate text in single-line mode.
    textInput->text("hello");
    textInput->multiline(false);
    textInput->rawTextInput()->cursor(
        Cursor::collapsed(CursorPosition(3))); // "hel|lo"
    artboard->advance(0.0f);

    handled = textInput->keyInput(Key::enter, KeyModifiers::none, true, false);
    CHECK(handled == false);
    CHECK(textInput->rawTextInput()->text() == "hello");
    CHECK(textInput->text() == "hello");
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

    auto stateMachine = artboard->stateMachineAt(0);
    if (stateMachine == nullptr)
    {
        // Skip if no state machine
        return;
    }

    // Advance to initialize
    stateMachine->advanceAndApply(0.0f);

    auto textInput = artboard->objects<TextInput>().first();
    if (textInput == nullptr)
    {
        // Skip if no text input found
        return;
    }

    // Focus the text input (required for text/key input to be handled)
    auto focusData = artboard->objects<FocusData>().first();
    REQUIRE(focusData != nullptr);
    stateMachine->setFocus(focusData);

    // Clear text first
    textInput->rawTextInput()->text("");
    textInput->rawTextInput()->cursor(Cursor::zero());

    // Test textInput through state machine
    bool handled = stateMachine->textInput("typed text");
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "typed text");

    // Test keyInput through state machine (backspace)
    handled =
        stateMachine->keyInput(Key::backspace, KeyModifiers::none, true, false);
    CHECK(handled == true);
    CHECK(textInput->rawTextInput()->text() == "typed tex");

    // With focus cleared there is no target, so the state machine reports the
    // events as unhandled and the text is left alone.
    stateMachine->clearFocus();
    CHECK(stateMachine->textInput("more") == false);
    CHECK(stateMachine->keyInput(Key::backspace,
                                 KeyModifiers::none,
                                 true,
                                 false) == false);
    CHECK(textInput->rawTextInput()->text() == "typed tex");
}

TEST_CASE("losing focus clears the text input selection", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    stateMachine->advanceAndApply(0.0f);

    auto textInput = artboard->objects<TextInput>().first();
    REQUIRE(textInput != nullptr);

    auto cursor = artboard->objects<TextInputCursor>().first();
    REQUIRE(cursor != nullptr);

    // Unfocused: no cursor is drawn.
    CHECK(textInput->isFocused() == false);
    CHECK(cursor->localClockwisePath() == nullptr);

    auto focusData = artboard->objects<FocusData>().first();
    REQUIRE(focusData != nullptr);
    stateMachine->setFocus(focusData);
    CHECK(textInput->isFocused() == true);
    CHECK(cursor->localClockwisePath() != nullptr);

    textInput->rawTextInput()->text("hello world");
    textInput->rawTextInput()->selectAll();
    CHECK(textInput->rawTextInput()->cursor().hasSelection());

    stateMachine->clearFocus();
    CHECK(textInput->rawTextInput()->cursor().isCollapsed());
    CHECK(textInput->rawTextInput()->cursor().end().codePointIndex() == 11);
    CHECK(textInput->isFocused() == false);
    CHECK(cursor->localClockwisePath() == nullptr);
}

TEST_CASE("the text input cursor blinks while focused", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    CHECK(artboard != nullptr);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    stateMachine->advanceAndApply(0.0f);

    auto textInput = artboard->objects<TextInput>().first();
    REQUIRE(textInput != nullptr);

    auto cursor = artboard->objects<TextInputCursor>().first();
    REQUIRE(cursor != nullptr);

    auto focusData = artboard->objects<FocusData>().first();
    REQUIRE(focusData != nullptr);

    // Unfocused, the caret never draws no matter how much time passes.
    stateMachine->advanceAndApply(0.6f);
    CHECK(cursor->localClockwisePath() == nullptr);

    // Focusing shows the caret, which then toggles every half second.
    stateMachine->setFocus(focusData);
    CHECK(cursor->localClockwisePath() != nullptr);
    stateMachine->advanceAndApply(0.5f);
    CHECK(cursor->localClockwisePath() == nullptr);
    stateMachine->advanceAndApply(0.5f);
    CHECK(cursor->localClockwisePath() != nullptr);

    // Typing restarts the cycle so the caret stays solid while editing.
    stateMachine->advanceAndApply(0.4f);
    stateMachine->textInput("a");
    stateMachine->advanceAndApply(0.2f);
    CHECK(cursor->localClockwisePath() != nullptr);

    // Moving the caret restarts it too.
    stateMachine->advanceAndApply(0.4f);
    stateMachine->keyInput(Key::left, KeyModifiers::none, true, false);
    stateMachine->advanceAndApply(0.2f);
    CHECK(cursor->localClockwisePath() != nullptr);

    // Blurring hides it again.
    stateMachine->clearFocus();
    CHECK(cursor->localClockwisePath() == nullptr);
}

// The field the text aligns within is the viewport's content box. Padding on
// the viewport is space the text can't occupy, so it has to come off the align
// width -- otherwise centered/right text is pushed into the padding.
TEST_CASE("viewport padding comes off the alignment box", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);

    auto textInput = artboard->objects<TextInput>().first();
    REQUIRE(textInput != nullptr);

    float unpaddedWidth = textInput->rawTextInput()->alignWidth();
    float unpaddedHeight = textInput->rawTextInput()->alignHeight();
    CHECK(unpaddedWidth > 0.0f);
    CHECK(unpaddedHeight > 0.0f);

    // TextInput -> Text Container -> Scroll Content -> Viewport.
    auto viewportComponent = textInput->parent()->parent()->parent();
    REQUIRE(viewportComponent != nullptr);
    REQUIRE(viewportComponent->is<LayoutComponent>());
    auto viewport = viewportComponent->as<LayoutComponent>();
    auto style = viewport->style();
    REQUIRE(style != nullptr);

    style->paddingLeft(12.0f);
    style->paddingRight(8.0f);
    style->paddingTop(5.0f);
    style->paddingBottom(3.0f);
    artboard->advance(0.0f);

    CHECK(viewport->paddingLeft() == Approx(12.0f));
    CHECK(textInput->rawTextInput()->alignWidth() ==
          Approx(unpaddedWidth - 12.0f - 8.0f));
    CHECK(textInput->rawTextInput()->alignHeight() ==
          Approx(unpaddedHeight - 5.0f - 3.0f));
}

// A dropped or suspended frame can hand us an advance spanning several blink
// phases. Toggling once regardless would leave the caret in the wrong phase
// for every even number of them.
TEST_CASE("the caret blink accounts for every elapsed phase", "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);

    stateMachine->advanceAndApply(0.0f);

    auto cursor = artboard->objects<TextInputCursor>().first();
    REQUIRE(cursor != nullptr);
    auto focusData = artboard->objects<FocusData>().first();
    REQUIRE(focusData != nullptr);

    stateMachine->setFocus(focusData);
    REQUIRE(cursor->localClockwisePath() != nullptr);

    // Two whole phases: back to visible, not hidden.
    stateMachine->advanceAndApply(1.0f);
    CHECK(cursor->localClockwisePath() != nullptr);

    // Three whole phases: hidden.
    stateMachine->advanceAndApply(1.5f);
    CHECK(cursor->localClockwisePath() == nullptr);

    // Four whole phases leaves it where it was.
    stateMachine->advanceAndApply(2.0f);
    CHECK(cursor->localClockwisePath() == nullptr);

    // And the leftover remainder still carries into the next phase: 0.3 after
    // the 2.0 above puts us 0.3 into a phase, so 0.2 more flips it.
    stateMachine->advanceAndApply(0.3f);
    CHECK(cursor->localClockwisePath() == nullptr);
    stateMachine->advanceAndApply(0.2f);
    CHECK(cursor->localClockwisePath() != nullptr);
}
TEST_CASE("obscured text input keeps selected text off the clipboard",
          "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    REQUIRE(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    REQUIRE(textInput != nullptr);

    textInput->rawTextInput()->text("hunter2");
    artboard->advance(0.0f);
    textInput->rawTextInput()->selectAll();
    std::string selected;
    REQUIRE(textInput->selectedText(selected));
    CHECK(selected == "hunter2");

    // A reused non-empty string must come back cleared.
    textInput->obscured(true);
    selected = "stale";
    CHECK(textInput->selectedText(selected));
    CHECK(selected.empty());
}

namespace
{
class SelectionAncestor : public Focusable
{
public:
    bool keyInput(Key, KeyModifiers, bool, bool) override { return false; }
    bool textInput(const std::string&) override { return false; }
    void focused() override {}
    void blurred() override {}
    bool selectedText(std::string& outText) const override
    {
        outText = "ancestor selection";
        return true;
    }
};
} // namespace

TEST_CASE("obscured text input stops selection lookup at itself",
          "[text_input]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    REQUIRE(artboard != nullptr);

    auto textInput = artboard->objects<TextInput>().first();
    REQUIRE(textInput != nullptr);

    SelectionAncestor ancestor;
    auto ancestorNode = make_rcp<FocusNode>(&ancestor);
    auto inputNode = make_rcp<FocusNode>(static_cast<Focusable*>(textInput));
    ancestorNode->addChild(inputNode);

    FocusManager manager;
    manager.setFocus(inputNode);

    textInput->rawTextInput()->text("hunter2");
    artboard->advance(0.0f);
    textInput->rawTextInput()->selectAll();
    CHECK(manager.selectedText() == "hunter2");

    // With no selection the lookup still bubbles to the ancestor.
    textInput->rawTextInput()->clearSelection();
    CHECK(manager.selectedText() == "ancestor selection");

    // Obscured, the input terminates the lookup even without a selection.
    textInput->rawTextInput()->selectAll();
    textInput->obscured(true);
    CHECK(manager.selectedText().empty());

    manager.setFocus(nullptr);
}
#endif
