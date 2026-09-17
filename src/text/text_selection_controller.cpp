#include "rive/text/text_selection_controller.hpp"
#ifdef WITH_RIVE_TEXT
#include "rive/artboard.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/factory.hpp"
#include "rive/file.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_selection_path.hpp"
#include "rive/text/utf.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace rive;

struct TextSelectionController::Participant
{
    Text* text;
    uint32_t id = 0;
    std::string separatorBefore;
    std::vector<Unichar> source;
    GlyphLookup lookup;
    Cursor cursor = Cursor::zero();
    std::vector<AABB> rects;
    TextSelectionPath path;
    rcp<RenderPaint> paint;
    bool lookupDirty = true;
    bool geometryDirty = true;
    float radius = -1;
#ifdef TESTING
    size_t lookupBuilds = 0;
#endif

    Participant(Text* text, std::string separator) :
        text(text), separatorBefore(std::move(separator))
    {}
};

// rootTransform includes every nested host and its artboard's self transform.
// Sampling the affine basis also covers Dart-mounted artboard callbacks.
static Mat2D shapeToRoot(Text* text)
{
    auto* artboard = text->artboard();
    auto origin = artboard->rootTransform(Vec2D(0, 0));
    auto x = artboard->rootTransform(Vec2D(1, 0)) - origin;
    auto y = artboard->rootTransform(Vec2D(0, 1)) - origin;
    return Mat2D(x.x, x.y, y.x, y.y, origin.x, origin.y) *
           text->shapeWorldTransform();
}

static bool finite(Vec2D position)
{
    return std::isfinite(position.x) && std::isfinite(position.y);
}

static AABB lineBounds(const OrderedLine& line)
{
    float x = line.glyphLine().startX;
    auto bounds = AABB::forExpansion();
    for (auto glyph : line)
    {
        auto* run = std::get<0>(glyph);
        auto index = std::get<1>(glyph);
        AABB::expandTo(bounds,
                       Vec2D(x, line.y() + run->font->ascent(run->size)));
        x += run->advances[index];
        AABB::expandTo(bounds,
                       Vec2D(x, line.y() + run->font->descent(run->size)));
    }
    return bounds;
}

TextSelectionController::TextSelectionController() = default;

TextSelectionController::~TextSelectionController()
{
    for (auto& participant : m_participants)
    {
        participant->text->m_selectionController = nullptr;
    }
}

TextSelectionController::Participant* TextSelectionController::find(
    Text* text) const
{
    for (auto& participant : m_participants)
    {
        if (participant->text == text)
        {
            return participant.get();
        }
    }
    return nullptr;
}

bool TextSelectionController::add(Text* text, std::string separatorBefore)
{
    if (text == nullptr || text->artboard() == nullptr ||
        text->m_selectionController != nullptr || text->haveModifiers() ||
        text->overflow() == TextOverflow::ellipsis)
    {
        return false;
    }
    clear();
    auto participant =
        std::make_unique<Participant>(text, std::move(separatorBefore));
    participant->source = text->m_styledText.unichars();
    participant->id = m_nextTargetId++;
    text->m_selectionController = this;
    m_participants.push_back(std::move(participant));
    return true;
}

void TextSelectionController::remove(Text* text)
{
    auto it = std::find_if(m_participants.begin(),
                           m_participants.end(),
                           [text](const std::unique_ptr<Participant>& p) {
                               return p->text == text;
                           });
    if (it == m_participants.end())
    {
        return;
    }
    clear();
    text->m_selectionController = nullptr;
    m_participants.erase(it);
}

bool TextSelectionController::eligible(const Participant& participant) const
{
    auto* text = participant.text;
    return !text->isHidden() && text->renderOpacity() > 0 &&
           !text->haveModifiers() &&
           text->overflow() != TextOverflow::ellipsis &&
           !text->m_orderedLines.empty() && !participant.source.empty();
}

static void collectSelectable(Artboard* artboard, std::vector<Text*>& result)
{
    if (artboard == nullptr)
        return;
    for (auto* object : artboard->objects())
    {
        if (object != nullptr && object->is<Text>() &&
            object->as<Text>()->isSelectable())
            result.push_back(object->as<Text>());
    }
    for (auto* nested : artboard->nestedArtboards())
        collectSelectable(nested->artboardInstance(), result);
    for (auto* list : artboard->artboardComponentLists())
        for (size_t i = 0; i < list->artboardCount(); ++i)
            collectSelectable(list->artboardInstance((int)i), result);
}

void TextSelectionController::synchronize(Artboard* root)
{
    m_styleFile = root != nullptr && root->isInstance()
                      ? static_cast<ArtboardInstance*>(root)->file()
                      : nullptr;
    std::vector<Text*> texts;
    collectSelectable(root, texts);
    std::vector<Text*> removed;
    for (const auto& p : m_participants)
        if (std::find(texts.begin(), texts.end(), p->text) == texts.end())
            removed.push_back(p->text);
    for (auto* text : removed)
        remove(text);
    for (auto* text : texts)
        if (find(text) == nullptr)
            add(text);
}

std::vector<uint32_t> TextSelectionController::targetIds() const
{
    std::vector<uint32_t> ids;
    for (const auto& p : m_participants)
        ids.push_back(p->id);
    return ids;
}

Text* TextSelectionController::target(uint32_t id) const
{
    for (const auto& p : m_participants)
        if (p->id == id)
            return p->text;
    return nullptr;
}

bool TextSelectionController::isEligible(Text* text) const
{
    auto* p = find(text);
    return p != nullptr && eligible(*p);
}

std::string TextSelectionController::sourceText(Text* text) const
{
    auto* p = find(text);
    std::string result;
    if (p == nullptr)
        return result;
    for (auto ch : p->source)
    {
        uint8_t bytes[4];
        auto count = UTF::Encode(bytes, ch);
        result.append(reinterpret_cast<char*>(bytes), count);
    }
    return result;
}

AABB TextSelectionController::rootBounds(Text* text) const
{
    auto* p = find(text);
    AABB result = AABB::forExpansion();
    if (p == nullptr || !eligible(*p))
        return result;
    auto transform = shapeToRoot(text);
    for (const auto& line : text->m_orderedLines)
    {
        auto b = lineBounds(line);
        if (b.isEmptyOrNaN())
            continue;
        AABB::expandTo(result, transform * Vec2D(b.minX, b.minY));
        AABB::expandTo(result, transform * Vec2D(b.maxX, b.minY));
        AABB::expandTo(result, transform * Vec2D(b.minX, b.maxY));
        AABB::expandTo(result, transform * Vec2D(b.maxX, b.maxY));
    }
    return result;
}

bool TextSelectionController::hitTest(Text* text,
                                      Vec2D point,
                                      bool nearest,
                                      uint32_t& offset,
                                      float& distanceSquared)
{
    auto* p = find(text);
    if (p == nullptr || !eligible(*p) || !finite(point))
        return false;
    auto transform = shapeToRoot(text);
    Mat2D inverse;
    if (!transform.invert(&inverse))
        return false;
    auto local = inverse * point;
    if (!finite(local))
        return false;
    if (!nearest)
    {
        auto artboardPoint = text->shapeWorldTransform() * local;
        if (!text->hitTestPoint(artboardPoint, false, true))
            return false;
        if (text->overflow() == TextOverflow::clipped)
        {
            Mat2D inverseWorld;
            if (!text->worldTransform().invert(&inverseWorld) ||
                !text->localBounds().contains(inverseWorld * artboardPoint))
                return false;
        }
    }
    bool found = false;
    distanceSquared = std::numeric_limits<float>::infinity();
    uint32_t index = 0;
    for (const auto& line : text->m_orderedLines)
    {
        auto b = lineBounds(line);
        if (!b.isEmptyOrNaN() && (nearest || b.contains(local)))
        {
            Vec2D closest(std::max(b.minX, std::min(local.x, b.maxX)),
                          std::max(b.minY, std::min(local.y, b.maxY)));
            auto delta = transform * closest - point;
            auto distance = delta.x * delta.x + delta.y * delta.y;
            if (distance < distanceSquared)
            {
                distanceSquared = distance;
                offset = CursorPosition::fromLineX(index, local.x, layout(*p))
                             .codePointIndex();
                found = true;
            }
        }
        ++index;
    }
    return found;
}

void TextSelectionController::setRange(Text* text, uint32_t start, uint32_t end)
{
    auto* p = find(text);
    if (p == nullptr)
        return;
    if (!m_externalRanges)
        clear();
    m_externalRanges = true;
    auto length = (uint32_t)p->source.size();
    start = std::min(start, length);
    end = std::min(end, length);
    if (p->cursor.first().codePointIndex() == std::min(start, end) &&
        p->cursor.last().codePointIndex() == std::max(start, end))
        return;
    p->cursor = Cursor(CursorPosition(start), CursorPosition(end));
    p->geometryDirty = true;
}

TextLayoutView TextSelectionController::layout(Participant& participant)
{
    auto* text = participant.text;
    if (participant.lookupDirty)
    {
        participant.lookup.compute(text->m_styledText.unichars(),
                                   text->m_shape);
        participant.lookupDirty = false;
#ifdef TESTING
        participant.lookupBuilds++;
#endif
    }
    return TextLayoutView(text->m_shape,
                          text->m_lines,
                          text->m_orderedLines,
                          participant.lookup,
                          (uint32_t)participant.source.size());
}

void TextSelectionController::textUpdated(Text* text, bool shapeChanged)
{
    auto* participant = find(text);
    if (participant == nullptr)
    {
        return;
    }
    if (shapeChanged)
    {
        const auto& source = text->m_styledText.unichars();
        if (participant->source != source)
        {
            clear();
            participant->source = source;
        }
        participant->lookupDirty = true;
    }
    participant->geometryDirty = true;
    if (m_anchor.participant != nullptr && !validSelection())
    {
        clear();
    }
}

#ifdef TESTING
size_t TextSelectionController::lookupBuildCount(Text* text) const
{
    auto* participant = find(text);
    return participant == nullptr ? 0 : participant->lookupBuilds;
}
#endif

bool TextSelectionController::validSelection() const
{
    if (m_externalRanges)
    {
        for (auto& participant : m_participants)
        {
            if (participant->cursor.hasSelection() && !eligible(*participant))
                return false;
        }
        return true;
    }
    if (m_anchor.participant == nullptr || m_extent.participant == nullptr ||
        !eligible(*m_anchor.participant) || !eligible(*m_extent.participant))
    {
        return false;
    }
    for (auto& participant : m_participants)
    {
        if (participant->cursor.first().codePointIndex() !=
                participant->cursor.last().codePointIndex() &&
            !eligible(*participant))
        {
            return false;
        }
    }
    return true;
}

void TextSelectionController::clear()
{
    m_anchor = {};
    m_extent = {};
    m_dragging = false;
    m_externalRanges = false;
    for (auto& participant : m_participants)
    {
        participant->cursor = Cursor::zero();
        participant->geometryDirty = true;
    }
}

bool TextSelectionController::endpointAt(Participant& participant,
                                         Vec2D rootPosition,
                                         Endpoint& out)
{
    Mat2D inverse;
    if (!eligible(participant) || !finite(rootPosition) ||
        !shapeToRoot(participant.text).invert(&inverse))
    {
        return false;
    }
    auto local = inverse * rootPosition;
    if (!finite(local))
    {
        return false;
    }
    out = {&participant,
           CursorPosition::fromTranslation(local, layout(participant))};
    return true;
}

bool TextSelectionController::pointerDown(Text* hitText,
                                          Vec2D rootPosition,
                                          int pointerId,
                                          bool extend)
{
    // A second pointer must not replace the anchor or take over capture.
    if (m_dragging)
    {
        return false;
    }
    auto* participant = find(hitText);
    Endpoint endpoint;
    if (participant == nullptr ||
        !endpointAt(*participant, rootPosition, endpoint))
    {
        clear();
        return false;
    }
    if (!extend || m_externalRanges || !validSelection())
    {
        m_anchor = endpoint;
    }
    m_externalRanges = false;
    m_extent = endpoint;
    m_pointerId = pointerId;
    m_dragging = true;
    updateRanges();
    return true;
}

bool TextSelectionController::nearestEndpoint(Vec2D rootPosition, Endpoint& out)
{
    if (!finite(rootPosition))
    {
        return false;
    }
    float bestDistance = std::numeric_limits<float>::infinity();
    bool found = false;
    for (auto& participant : m_participants)
    {
        Mat2D inverse;
        auto transform = shapeToRoot(participant->text);
        if (!eligible(*participant) || !transform.invert(&inverse))
        {
            continue;
        }
        auto local = inverse * rootPosition;
        if (!finite(local))
        {
            continue;
        }
        uint32_t lineIndex = 0;
        for (const auto& line : participant->text->m_orderedLines)
        {
            auto bounds = lineBounds(line);
            if (!bounds.isEmptyOrNaN())
            {
                Vec2D closest(
                    std::max(bounds.minX, std::min(local.x, bounds.maxX)),
                    std::max(bounds.minY, std::min(local.y, bounds.maxY)));
                auto delta = transform * closest - rootPosition;
                float distance = delta.x * delta.x + delta.y * delta.y;
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    out = {participant.get(),
                           CursorPosition::fromLineX(lineIndex,
                                                     local.x,
                                                     layout(*participant))};
                    found = true;
                }
            }
            lineIndex++;
        }
    }
    return found;
}

bool TextSelectionController::pointerMove(Vec2D rootPosition, int pointerId)
{
    if (!m_dragging || pointerId != m_pointerId)
    {
        return false;
    }
    if (!validSelection())
    {
        clear();
        return false;
    }
    Endpoint endpoint;
    if (nearestEndpoint(rootPosition, endpoint))
    {
        m_extent = endpoint;
        updateRanges();
    }
    return true;
}

bool TextSelectionController::pointerUp(Vec2D rootPosition, int pointerId)
{
    if (!pointerMove(rootPosition, pointerId))
    {
        return false;
    }
    m_dragging = false;
    return true;
}

bool TextSelectionController::pointerCancel(int pointerId)
{
    if (!m_dragging || pointerId != m_pointerId)
    {
        return false;
    }
    clear();
    return true;
}

bool TextSelectionController::select(Text* anchorText,
                                     uint32_t anchorOffset,
                                     Text* extentText,
                                     uint32_t extentOffset)
{
    auto* anchor = find(anchorText);
    auto* extent = find(extentText);
    if (anchor == nullptr || extent == nullptr || !eligible(*anchor) ||
        !eligible(*extent))
    {
        return false;
    }
    m_dragging = false;
    m_anchor = {anchor, CursorPosition::atIndex(anchorOffset, layout(*anchor))};
    m_externalRanges = false;
    m_extent = {extent, CursorPosition::atIndex(extentOffset, layout(*extent))};
    updateRanges();
    return true;
}

void TextSelectionController::selectAll()
{
    Participant* first = nullptr;
    Participant* last = nullptr;
    for (auto& participant : m_participants)
    {
        if (eligible(*participant))
        {
            if (first == nullptr)
            {
                first = participant.get();
            }
            last = participant.get();
        }
    }
    if (first != nullptr)
    {
        select(first->text, 0, last->text, (uint32_t)last->source.size());
    }
    else
    {
        clear();
    }
}

void TextSelectionController::updateRanges()
{
    auto first = m_anchor;
    auto last = m_extent;
    size_t anchorIndex = 0, extentIndex = 0;
    for (size_t i = 0; i < m_participants.size(); i++)
    {
        if (m_participants[i].get() == first.participant)
            anchorIndex = i;
        if (m_participants[i].get() == last.participant)
            extentIndex = i;
    }
    if (anchorIndex > extentIndex ||
        (anchorIndex == extentIndex && first.position > last.position))
    {
        std::swap(first, last);
        std::swap(anchorIndex, extentIndex);
    }
    for (size_t i = 0; i < m_participants.size(); i++)
    {
        auto& participant = *m_participants[i];
        uint32_t start = 0, end = 0;
        if (i >= anchorIndex && i <= extentIndex && eligible(participant))
        {
            start = i == anchorIndex ? first.position.codePointIndex() : 0;
            end = i == extentIndex ? last.position.codePointIndex()
                                   : (uint32_t)participant.source.size();
        }
        participant.cursor = Cursor(CursorPosition(start), CursorPosition(end));
        participant.geometryDirty = true;
    }
}

bool TextSelectionController::hasSelection() const
{
    if (!validSelection())
    {
        return false;
    }
    if (m_externalRanges)
    {
        for (const auto& p : m_participants)
            if (p->cursor.hasSelection())
                return true;
        return false;
    }
    return m_anchor.participant != m_extent.participant ||
           m_anchor.position.codePointIndex() !=
               m_extent.position.codePointIndex();
}

Cursor TextSelectionController::selection(Text* text) const
{
    auto* participant = find(text);
    return validSelection() && participant != nullptr ? participant->cursor
                                                      : Cursor::zero();
}

std::string TextSelectionController::selectedText() const
{
    std::string result;
    if (!hasSelection())
    {
        return result;
    }
    if (m_externalRanges)
    {
        for (const auto& p : m_participants)
        {
            if (!p->cursor.hasSelection())
                continue;
            if (!result.empty())
                result += p->separatorBefore;
            for (auto i = p->cursor.first().codePointIndex();
                 i < p->cursor.last().codePointIndex();
                 ++i)
            {
                uint8_t bytes[4];
                auto count = UTF::Encode(bytes, p->source[i]);
                result.append(reinterpret_cast<char*>(bytes), count);
            }
        }
        return result;
    }
    bool started = false;
    bool inRange = false;
    uint32_t endpointsSeen = 0;
    // Include object boundaries even when an endpoint contributes zero
    // characters (end of A -> start of B still selects the separator).
    for (auto& participant : m_participants)
    {
        bool endpoint = participant.get() == m_anchor.participant ||
                        participant.get() == m_extent.participant;
        if (endpoint && !inRange)
        {
            inRange = true;
        }
        if (inRange && eligible(*participant))
        {
            if (started)
            {
                result += participant->separatorBefore;
            }
            started = true;
            auto first = participant->cursor.first().codePointIndex();
            auto last = participant->cursor.last().codePointIndex();
            for (auto i = first; i < last; i++)
            {
                uint8_t bytes[4];
                auto count = UTF::Encode(bytes, participant->source[i]);
                result.append(reinterpret_cast<char*>(bytes), count);
            }
        }
        if (endpoint)
        {
            endpointsSeen++;
            if (m_anchor.participant == m_extent.participant ||
                endpointsSeen == 2)
            {
                break;
            }
        }
    }
    return result;
}

Span<const AABB> TextSelectionController::selectionRects(Text* text)
{
    auto* participant = find(text);
    if (participant == nullptr || !participant->cursor.hasSelection() ||
        !validSelection())
    {
        return {};
    }
    float radius = std::max(0.0f, style().cornerRadius());
    if (participant->geometryDirty || participant->radius != radius)
    {
        participant->rects.clear();
        auto view = layout(*participant);
        if (!view.orderedLines().empty())
        {
            // Range endpoints are source offsets. Walk every drawn line, so
            // hidden/trimmed lines do not confuse logical and visible indices.
            auto cursor = Cursor(
                CursorPosition(0, participant->cursor.first().codePointIndex()),
                CursorPosition((uint32_t)view.orderedLines().size() - 1,
                               participant->cursor.last().codePointIndex()));
            cursor.selectionRects(participant->rects, view);
        }
        participant->path.update(participant->rects, radius);
        participant->radius = radius;
        participant->geometryDirty = false;
    }
    return participant->rects;
}

void TextSelectionController::draw(Text* text, Renderer* renderer)
{
    if (m_anchor.participant != nullptr && !validSelection())
    {
        clear();
    }
    if (selectionRects(text).empty())
    {
        return;
    }
    auto* participant = find(text);
    auto* factory = text->artboard()->factory();
    if (participant->paint == nullptr)
    {
        participant->paint = factory->makeRenderPaint();
        participant->paint->style(RenderPaintStyle::fill);
    }
    participant->paint->color(
        colorModulateOpacity(style().highlightColor(), text->renderOpacity()));
    renderer->save();
    renderer->transform(text->shapeWorldTransform());
    renderer->drawPath(participant->path.renderPath(factory),
                       participant->paint.get());
    renderer->restore();
}

const SelectionStyle& TextSelectionController::style() const
{
    static const SelectionStyle defaults;
    const auto* fileStyle =
        m_styleFile == nullptr ? nullptr : m_styleFile->selectionStyle();
    return m_style != nullptr     ? *m_style
           : fileStyle != nullptr ? *fileStyle
                                  : defaults;
}

bool TextSelectionController::keyInput(Key key,
                                       KeyModifiers modifiers,
                                       bool isPressed)
{
    if (!isPressed || !validSelection())
    {
        return false;
    }
    if (key == Key::escape)
    {
        clear();
        return true;
    }
    if (key == Key::a &&
        (modifiers & (KeyModifiers::ctrl | KeyModifiers::meta)) !=
            KeyModifiers::none)
    {
        selectAll();
        return true;
    }
    return false;
}
#endif
