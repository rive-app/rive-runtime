#ifndef _RIVE_TEXT_SELECTION_CONTROLLER_HPP_
#define _RIVE_TEXT_SELECTION_CONTROLLER_HPP_

#ifdef WITH_RIVE_TEXT
#include "rive/input/focusable.hpp"
#include "rive/text/cursor.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/selection_style.hpp"
#include <memory>
#include <string>

namespace rive
{
class Text;
class Artboard;
class Renderer;
class File;

// A host-owned selection region for regular Text. Add actual Text instances
// in reading order (including instances inside nested artboards). Registration
// opts each Text into selection; the Text continues to draw its own content.
// Advance the artboards before delivering input. Positions are in root
// artboard coordinates, before the host's canvas/view transform.
//
// This first core API deliberately leaves hit priority, pointer capture,
// clipboard access, scrolling, and focus ownership to the host. Pass the Text
// chosen by normal hit testing to pointerDown, and keep forwarding the captured
// pointer across object boundaries until up/cancel. The controller never edits
// text or requests an IME. Text modifiers and ellipsis are not supported yet.
class TextSelectionController
{
public:
    TextSelectionController();
    ~TextSelectionController();
    TextSelectionController(const TextSelectionController&) = delete;
    TextSelectionController& operator=(const TextSelectionController&) = delete;

    // A Text can belong to one controller. Removal/destruction of a participant
    // clears the active range; either controller or Text may be destroyed
    // first.
    bool add(Text* text, std::string separatorBefore = "\n");
    void remove(Text* text);

    // Discover opted-in Text instances, recursively including hosted artboards.
    // Existing registrations retain stable tokens; removed Text unregisters
    // through its destructor. These tokens are safe for host language bridges.
    void synchronize(Artboard* root);
    std::vector<uint32_t> targetIds() const;
    Text* target(uint32_t id) const;
    std::string sourceText(Text* text) const;
    bool isEligible(Text* text) const;
    AABB rootBounds(Text* text) const;
    bool hitTest(Text* text,
                 Vec2D rootPosition,
                 bool nearest,
                 uint32_t& offset,
                 float& distanceSquared);
    // A host coordinating native and external participants can supply each
    // local range. Source changes still invalidate these ranges.
    void setRange(Text* text, uint32_t start, uint32_t end);

    bool pointerDown(Text* hitText,
                     Vec2D rootPosition,
                     int pointerId = 0,
                     bool extend = false);
    bool pointerMove(Vec2D rootPosition, int pointerId = 0);
    bool pointerUp(Vec2D rootPosition, int pointerId = 0);
    bool pointerCancel(int pointerId = 0);
    bool isDragging() const { return m_dragging; }

    // Offsets are Unicode code points, inclusive at the start and exclusive at
    // the end. Layout/line changes preserve the source range; source changes
    // clear it when the Text updates. Reversing endpoints preserves copy order.
    bool select(Text* anchorText,
                uint32_t anchorOffset,
                Text* extentText,
                uint32_t extentOffset);
    void selectAll();
    void clear();
    bool hasSelection() const;
    std::string selectedText() const;
    Cursor selection(Text* text) const;

    // Borrowed rectangles in the Text's shape coordinates. These share the
    // same cursor geometry as TextInput and are also used by Text::draw.
    Span<const AABB> selectionRects(Text* text);

    // One style for the whole selection, including nested targets. A host
    // override must outlive the controller; nullptr restores the root file's
    // default style, or the built-in default when no resource is authored.
    const SelectionStyle& style() const;
    void style(const SelectionStyle* value) { m_style = value; }

#ifdef TESTING
    size_t lookupBuildCount(Text* text) const;
#endif

    // Returns true only for selection commands (Escape and Cmd/Ctrl+A).
    // The host handles copy by reading selectedText(). Editing keys are
    // ignored.
    bool keyInput(Key key, KeyModifiers modifiers, bool isPressed);

private:
    friend class Text;
    struct Participant;
    struct Endpoint
    {
        Participant* participant = nullptr;
        CursorPosition position = CursorPosition::zero();
    };

    Participant* find(Text* text) const;
    TextLayoutView layout(Participant& participant);
    bool eligible(const Participant& participant) const;
    bool validSelection() const;
    bool endpointAt(Participant& participant,
                    Vec2D rootPosition,
                    Endpoint& out);
    bool nearestEndpoint(Vec2D rootPosition, Endpoint& out);
    void updateRanges();
    void textUpdated(Text* text, bool shapeChanged);
    void draw(Text* text, Renderer* renderer);

    std::vector<std::unique_ptr<Participant>> m_participants;
    Endpoint m_anchor;
    Endpoint m_extent;
    int m_pointerId = 0;
    bool m_dragging = false;
    bool m_externalRanges = false;
    uint32_t m_nextTargetId = 1;
    const SelectionStyle* m_style = nullptr;
    // Keep the default resource alive even if the host releases its root.
    rcp<const File> m_styleFile;
};
} // namespace rive
#endif
#endif
