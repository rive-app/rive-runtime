#include "rive/text/text_selection_controller.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/nested_artboard.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_renderer.hpp"
#include <algorithm>
#include <limits>

using namespace rive;

namespace
{
// Use the actual drawn lines to locate a source boundary, independently of
// controller hit testing and without reshaping a second copy of the text.
Vec2D pointAt(Text* text, uint32_t offset)
{
    GlyphLookup lookup;
    lookup.compute(text->unichars(), text->shape());
    SimpleArray<SimpleArray<GlyphLine>> unusedLines;
    TextLayoutView view(text->shape(),
                        unusedLines,
                        text->orderedLines(),
                        lookup,
                        (uint32_t)text->unichars().size());
    CursorPosition position(offset);
    position.resolveLine(view);
    auto visual = position.clamped(view).visualPosition(view);
    REQUIRE(visual.found());
    return text->artboard()->rootTransform(
        text->shapeWorldTransform() *
        Vec2D(visual.x(), (visual.top() + visual.bottom()) / 2));
}

void setText(Text* text, const std::string& value)
{
    auto runs = text->runs();
    REQUIRE(!runs.empty());
    runs.front()->text(value);
    for (size_t i = 1; i < runs.size(); i++)
    {
        runs[i]->text("");
    }
}

struct TextSelectionScene
{
    rcp<File> file = ReadRiveFile("assets/new_text.riv");
    std::unique_ptr<ArtboardInstance> artboard = file->artboard()->instance();
    std::vector<Text*> texts = artboard->find<Text>();
    TextSelectionController selection;

    TextSelectionScene()
    {
        REQUIRE(texts.size() >= 4);
        const char* values[] = {"Introduction",
                                "Rive supports interactive graphics.",
                                "Learn more",
                                "Outside the selected range"};
        for (size_t i = 0; i < 4; i++)
        {
            auto* text = texts[i];
            setText(text, values[i]);
            text->x(0);
            text->y((float)i * 200);
            text->originX(0);
            text->originY(0);
            text->sizingValue((uint32_t)TextSizing::autoWidth);
            text->overflowValue((uint32_t)TextOverflow::visible);
            for (auto* style : text->textStylePaints())
            {
                style->fontSize(24);
            }
        }
        artboard->advance(0);
        for (size_t i = 0; i < 4; i++)
        {
            REQUIRE(selection.add(texts[i]));
        }
    }
};

class CountingRenderer : public NoOpRenderer
{
public:
    size_t paths = 0;
    void drawPath(RenderPath*, RenderPaint*) override { paths++; }
};
} // namespace

TEST_CASE("regular text drag spans objects and gaps in reading order",
          "[text_selection]")
{
    TextSelectionScene scene;
    auto& selection = scene.selection;
    auto& text = scene.texts;
    auto start = pointAt(text[0], 5);
    auto end = pointAt(text[2], 5);
    REQUIRE(selection.pointerDown(text[0], start, 7));
    CHECK(selection.selectedText().empty());
    REQUIRE(selection.pointerMove(Vec2D(end.x + 20, (start.y + end.y) / 2), 7));
    REQUIRE(selection.pointerUp(end, 7));
    CHECK_FALSE(selection.isDragging());
    CHECK(selection.selectedText() ==
          "duction\nRive supports interactive graphics.\nLearn");
    CHECK_FALSE(selection.selectionRects(text[0]).empty());
    CHECK_FALSE(selection.selectionRects(text[1]).empty());
    CHECK_FALSE(selection.selectionRects(text[2]).empty());
    CHECK(selection.selectionRects(text[3]).empty());

    REQUIRE(selection.pointerDown(text[2], end));
    REQUIRE(selection.pointerUp(start));
    CHECK(selection.selectedText() ==
          "duction\nRive supports interactive graphics.\nLearn");
}

TEST_CASE("regular text selection builds its lookup only when needed",
          "[text_selection]")
{
    TextSelectionScene scene;
    auto& selection = scene.selection;
    auto* a = scene.texts[0];
    auto* b = scene.texts[1];
    CountingRenderer renderer;
    for (auto* text : scene.texts)
        CHECK(selection.lookupBuildCount(text) == 0);

    selection.setRange(a, 0, 3);
    selection.setRange(b, 0, 0);
    b->draw(&renderer);
    CHECK(selection.selectionRects(b).empty());
    CHECK(selection.lookupBuildCount(b) == 0);
    REQUIRE_FALSE(selection.selectionRects(a).empty());
    CHECK(selection.lookupBuildCount(a) == 1);

    SelectionStyle style;
    style.highlightColor(0x80FF8800);
    style.cornerRadius(6);
    selection.style(&style);
    a->markPaintDirty();
    scene.artboard->advance(0);
    selection.setRange(a, 0, 3);
    REQUIRE_FALSE(selection.selectionRects(a).empty());
    CHECK(selection.lookupBuildCount(a) == 1);

    a->textStylePaints().front()->fontSize(32);
    scene.artboard->advance(0);
    CHECK(selection.lookupBuildCount(a) == 1);
    REQUIRE_FALSE(selection.selectionRects(a).empty());
    CHECK(selection.lookupBuildCount(a) == 2);

    setText(a, "Changed source");
    scene.artboard->advance(0);
    a->draw(&renderer);
    CHECK(selection.selectedText().empty());
    CHECK(selection.lookupBuildCount(a) == 2);
}

TEST_CASE(
    "regular text selection includes the last character and object separators",
    "[text_selection]")
{
    TextSelectionScene scene;
    auto& s = scene.selection;
    auto a = scene.texts[0];
    auto b = scene.texts[1];
    REQUIRE(s.select(a, 0, a, UINT32_MAX));
    CHECK(s.selectedText() == "Introduction");
    CHECK(s.selection(a).last().codePointIndex() == 12);
    REQUIRE(s.select(a, 12, b, 0));
    CHECK(s.selectedText() == "\n");
    REQUIRE(s.select(b, 0, a, 12));
    CHECK(s.selectedText() == "\n");
    REQUIRE(s.select(a, 12, a, 12));
    CHECK_FALSE(s.hasSelection());
    CHECK(s.selectedText().empty());
    CHECK(s.selectionRects(a).empty());
}

TEST_CASE(
    "regular text selection copies runs exactly without visual wrap breaks",
    "[text_selection]")
{
    TextSelectionScene scene;
    auto text = scene.texts[0];
    // The fixture contains multiple independently styled runs.
    auto rich = std::find_if(scene.texts.begin(),
                             scene.texts.end(),
                             [](Text* t) { return t->runs().size() >= 2; });
    REQUIRE(rich != scene.texts.end());
    text = *rich;
    setText(text, "Hello ");
    text->runs()[1]->text("styled world\nNext paragraph");
    text->sizingValue((uint32_t)TextSizing::autoHeight);
    text->width(100);
    scene.artboard->advance(0);
    TextSelectionController separate;
    scene.selection.remove(text);
    REQUIRE(separate.add(text));
    REQUIRE(text->orderedLines().size() > 2);
    separate.selectAll();
    CHECK(separate.selectedText() == "Hello styled world\nNext paragraph");
    CHECK_FALSE(separate.selectionRects(text).empty());
}

TEST_CASE("regular text highlights draw within the existing Text render pass",
          "[text_selection]")
{
    TextSelectionScene scene;
    CountingRenderer before;
    scene.artboard->draw(&before);
    REQUIRE(scene.selection.select(scene.texts[0], 3, scene.texts[2], 5));
    CountingRenderer selected;
    scene.artboard->draw(&selected);
    CHECK(selected.paths == before.paths + 3);
    scene.selection.clear();
    CountingRenderer after;
    scene.artboard->draw(&after);
    CHECK(after.paths == before.paths);
}

TEST_CASE(
    "regular text selection survives relayout and whole object transforms",
    "[text_selection]")
{
    TextSelectionScene scene;
    auto text = scene.texts[1];
    auto& s = scene.selection;
    REQUIRE(s.select(text, 0, text, UINT32_MAX));
    auto value = s.selectedText();
    text->sizingValue((uint32_t)TextSizing::autoHeight);
    text->width(130);
    text->originX(0.5f);
    text->rotation(0.35f);
    text->scaleX(1.5f);
    scene.artboard->advance(0);
    CHECK(s.selectedText() == value);
    REQUIRE(text->orderedLines().size() > 1);
    REQUIRE(s.pointerDown(text, pointAt(text, 5)));
    REQUIRE(s.pointerUp(pointAt(text, 13)));
    CHECK(s.selectedText() == "supports");
    CHECK_FALSE(s.selectionRects(text).empty());
}

TEST_CASE("regular text source changes invalidate the shared range and capture",
          "[text_selection]")
{
    TextSelectionScene scene;
    auto& s = scene.selection;
    REQUIRE(s.pointerDown(scene.texts[0], pointAt(scene.texts[0], 1)));
    REQUIRE(s.pointerMove(pointAt(scene.texts[2], 5)));
    REQUIRE(s.hasSelection());
    setText(scene.texts[1], "Updated from data binding");
    scene.artboard->advance(0);
    CHECK_FALSE(s.hasSelection());
    CHECK_FALSE(s.isDragging());
    CHECK(s.selectedText().empty());
    CHECK(s.selectionRects(scene.texts[0]).empty());
    CHECK_FALSE(s.pointerMove(pointAt(scene.texts[2], 6)));
}

TEST_CASE("regular text selection keeps one pointer and handles cancellation",
          "[text_selection]")
{
    TextSelectionScene scene;
    auto text = scene.texts[0];
    auto& s = scene.selection;
    REQUIRE(s.pointerDown(text, pointAt(text, 1), 9));
    CHECK_FALSE(s.pointerDown(text, pointAt(text, 8), 10));
    CHECK_FALSE(s.pointerMove(pointAt(text, 8), 10));
    REQUIRE(s.pointerMove(pointAt(text, 5), 9));
    CHECK(s.selectedText() == "ntro");
    CHECK_FALSE(s.pointerCancel(10));
    REQUIRE(
        s.pointerMove(Vec2D(std::numeric_limits<float>::quiet_NaN(), 0), 9));
    CHECK(s.selectedText() == "ntro");
    REQUIRE(s.pointerCancel(9));
    CHECK_FALSE(s.isDragging());
    CHECK(s.selectedText().empty());
    CHECK_FALSE(s.pointerDown(nullptr, pointAt(text, 0)));
}

TEST_CASE("regular text accepts selection commands without editing",
          "[text_selection]")
{
    TextSelectionScene scene;
    auto& s = scene.selection;
    auto text = scene.texts[0];
    REQUIRE(s.select(text, 1, text, 4));
    CHECK_FALSE(s.keyInput(Key::backspace, KeyModifiers::none, true));
    CHECK_FALSE(s.keyInput(Key::deleteKey, KeyModifiers::none, true));
    CHECK_FALSE(s.keyInput(Key::v, KeyModifiers::meta, true));
    CHECK_FALSE(s.keyInput(Key::x, KeyModifiers::ctrl, true));
    CHECK(s.selectedText() == "ntr");
    REQUIRE(s.keyInput(Key::a, KeyModifiers::ctrl, true));
    CHECK(
        s.selectedText() ==
        "Introduction\nRive supports interactive graphics.\nLearn more\nOutside the selected range");
    REQUIRE(s.keyInput(Key::escape, KeyModifiers::none, true));
    CHECK(s.selectedText().empty());
    CHECK(text->runs()[0]->text() == "Introduction");
}

TEST_CASE("regular text selection ownership survives either destruction order",
          "[text_selection]")
{
    TextSelectionScene scene;
    auto text = scene.texts[0];
    CHECK_FALSE(scene.selection.add(text));
    TextSelectionController other;
    CHECK_FALSE(other.add(text));
    scene.selection.remove(text);
    REQUIRE(other.add(text));
    other.selectAll();
    REQUIRE(other.hasSelection());
    scene.artboard.reset();
    CHECK_FALSE(other.hasSelection());
    CHECK(other.selectedText().empty());
    CHECK(scene.selection.selectedText().empty());
}

TEST_CASE("regular text selection supports inline object separators",
          "[text_selection]")
{
    TextSelectionScene scene;
    auto a = scene.texts[0];
    auto b = scene.texts[1];
    scene.selection.remove(a);
    scene.selection.remove(b);
    TextSelectionController inlineSelection;
    REQUIRE(inlineSelection.add(a));
    REQUIRE(inlineSelection.add(b, " "));
    REQUIRE(inlineSelection.select(a, 5, b, 4));
    CHECK(inlineSelection.selectedText() == "duction Rive");
}

TEST_CASE("regular text selection copies Unicode source offsets",
          "[text_selection]")
{
    TextSelectionScene scene;
    auto text = scene.texts[0];
    setText(text, "A\xC3\xA9\xF0\x9F\x98\x80Z");
    scene.artboard->advance(0);
    REQUIRE(scene.selection.select(text, 1, text, 3));
    CHECK(scene.selection.selectedText() == "\xC3\xA9\xF0\x9F\x98\x80");
    REQUIRE(scene.selection.select(text, 0, text, UINT32_MAX));
    CHECK(scene.selection.selectedText() == "A\xC3\xA9\xF0\x9F\x98\x80Z");
}

TEST_CASE("empty and hidden text cannot leave stale selection highlights",
          "[text_selection]")
{
    TextSelectionScene scene;
    auto text = scene.texts[0];
    setText(text, "");
    scene.artboard->advance(0);
    CHECK_FALSE(scene.selection.select(text, 0, text, 1));
    CHECK(scene.selection.selectionRects(text).empty());
    scene.selection.selectAll();
    CHECK(
        scene.selection.selectedText() ==
        "Rive supports interactive graphics.\nLearn more\nOutside the selected range");
    scene.texts[1]->opacity(0);
    scene.artboard->advance(0);
    CHECK(scene.selection.selectedText().empty());
    CHECK(scene.selection.selectionRects(scene.texts[2]).empty());
}

TEST_CASE("destroying a selection controller unregisters its Text objects",
          "[text_selection]")
{
    auto file = ReadRiveFile("assets/hello_world.riv");
    auto artboard = file->artboard();
    artboard->advance(0);
    auto text = artboard->find<Text>()[0];
    {
        TextSelectionController selection;
        REQUIRE(selection.add(text));
        selection.selectAll();
        CHECK(selection.selectedText() == "Hello World!");
    }
    TextSelectionController next;
    REQUIRE(next.add(text));
    CHECK(next.selectedText().empty());
    text->overflowValue((uint32_t)TextOverflow::ellipsis);
    artboard->advance(0);
    CHECK_FALSE(next.select(text, 0, text, 5));
}

TEST_CASE("regular text selection spans actual nested artboard instances",
          "[text_selection]")
{
    auto file = ReadRiveFile("assets/runtime_nested_text_runs.riv");
    auto root = file->artboard("ArtboardA")->instance();
    auto firstRun = root->getTextRun("ArtboardBRun", "ArtboardB-1");
    auto middleRun =
        root->getTextRun("ArtboardCRun", "ArtboardB-1/ArtboardC-1");
    auto lastRun = root->getTextRun("ArtboardBRun", "ArtboardB-2");
    REQUIRE(firstRun != nullptr);
    REQUIRE(middleRun != nullptr);
    REQUIRE(lastRun != nullptr);
    firstRun->text("Introduction");
    middleRun->text("Nested middle");
    lastRun->text("Learn more");
    root->advance(0);
    auto first = firstRun->textComponent();
    auto middle = middleRun->textComponent();
    auto last = lastRun->textComponent();
    REQUIRE(first != last);
    // These are two instances of the same authored text component.
    CHECK(first->artboard() != last->artboard());
    CHECK(first->name() == last->name());
    TextSelectionController selection;
    REQUIRE(selection.add(first));
    REQUIRE(selection.add(middle));
    REQUIRE(selection.add(last));
    REQUIRE(selection.pointerDown(first, pointAt(first, 5)));
    REQUIRE(selection.pointerUp(pointAt(last, 5)));
    CHECK(selection.selectedText() == "duction\nNested middle\nLearn");
    CHECK_FALSE(selection.selectionRects(middle).empty());
}

TEST_CASE("Selectable discovery exposes stable safe host tokens",
          "[text_selection]")
{
    TextSelectionScene scene;
    scene.selection.synchronize(scene.artboard.get());
    REQUIRE(scene.selection.targetIds().empty());
    auto* a = scene.texts[0];
    auto* b = scene.texts[1];
    a->drawableFlags((uint16_t)DrawableFlag::Selectable);
    b->drawableFlags((uint16_t)DrawableFlag::Selectable);
    scene.selection.synchronize(scene.artboard.get());
    auto ids = scene.selection.targetIds();
    REQUIRE(ids.size() == 2);
    REQUIRE(scene.selection.target(ids[0]) == a);
    scene.selection.synchronize(scene.artboard.get());
    REQUIRE(scene.selection.targetIds() == ids);
    REQUIRE(scene.selection.sourceText(a) == "Introduction");

    uint32_t offset = 0;
    float distance = 0;
    REQUIRE(scene.selection.hitTest(a, pointAt(a, 5), false, offset, distance));
    REQUIRE(offset == 5);
    REQUIRE(distance == 0);
    REQUIRE_FALSE(scene.selection.hitTest(a,
                                          Vec2D(10000, 10000),
                                          false,
                                          offset,
                                          distance));
    REQUIRE(scene.selection
                .hitTest(a, Vec2D(10000, 10000), true, offset, distance));
    scene.selection.setRange(a, 5, 12);
    scene.selection.setRange(b, 0, 4);
    REQUIRE(scene.selection.hasSelection());
    REQUIRE(scene.selection.selectedText() == "duction\nRive");
    REQUIRE_FALSE(scene.selection.selectionRects(a).empty());
    REQUIRE_FALSE(scene.selection.selectionRects(b).empty());

    a->drawableFlags(0);
    scene.selection.synchronize(scene.artboard.get());
    REQUIRE(scene.selection.target(ids[0]) == nullptr);
    REQUIRE_FALSE(scene.selection.hasSelection());
    a->drawableFlags((uint16_t)DrawableFlag::Selectable);
    scene.selection.synchronize(scene.artboard.get());
    REQUIRE(scene.selection.target(ids[0]) == nullptr);
    REQUIRE(scene.selection.targetIds().size() == 2);
}
