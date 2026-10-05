#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint_proxy.hpp"
#include "rive/constraints/scrolling/scroll_virtualizer.hpp"
#include "rive/constraints/scrolling/virtual_layout.hpp"
#include "rive/constraints/transform_constraint.hpp"
#include "rive/core_context.hpp"
#include "rive/layout/grid_track.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/transform_component.hpp"
#include "rive/virtualizing_component.hpp"
#include "rive/math/mat2d.hpp"

using namespace rive;

// A wheel reports no end, so its gesture closes by going quiet.
static const float scrollIdleSeconds = 0.1f;

ScrollConstraint::~ScrollConstraint()
{
    if (m_virtualizer != nullptr)
    {
        delete m_virtualizer;
        m_virtualizer = nullptr;
    }
    delete m_virtualLayout;
    m_virtualLayout = nullptr;
    m_layoutChildren.clear();
    delete m_physics;
}

// Virtualized content along the scroll axis. Matches the non-virtualized
// layout size; an infinite carousel's cycle length stays padding free.
float ScrollConstraint::virtualContentExtent(float padding)
{
    auto layout = ensureVirtualLayout();
    if (layout == nullptr)
    {
        return 0.0f;
    }
    return infinite() ? layout->cycleExtent() : layout->extent() + padding;
}

float ScrollConstraint::contentWidth()
{
    if (virtualize() && !virtualAxisIsColumn())
    {
        return virtualContentExtent(content()->paddingLeft() +
                                    content()->paddingRight());
    }
    if (loopsX())
    {
        // A grid's columns cycle on from their copy one cycle along.
        auto layout = ensureVirtualLayout();
        return layout != nullptr ? layout->columnCycleExtent() : 0.0f;
    }
    return content()->layoutWidth();
}

float ScrollConstraint::contentHeight()
{
    if (virtualize() && virtualAxisIsColumn())
    {
        return virtualContentExtent(content()->paddingTop() +
                                    content()->paddingBottom());
    }
    return content()->layoutHeight();
}

float ScrollConstraint::viewportWidth()
{
    return direction() == DraggableConstraintDirection::vertical
               ? viewport()->layoutWidth()
               : std::max(0.0f,
                          viewport()->layoutWidth() - content()->layoutX());
}
float ScrollConstraint::viewportHeight()
{
    return direction() == DraggableConstraintDirection::horizontal
               ? viewport()->layoutHeight()
               : std::max(0.0f,
                          viewport()->layoutHeight() - content()->layoutY());
}
float ScrollConstraint::visibleWidthRatio()
{
    if (contentWidth() == 0)
    {
        return 1;
    }
    return std::min(1.0f, viewportWidth() / contentWidth());
}
float ScrollConstraint::visibleHeightRatio()
{
    if (contentHeight() == 0)
    {
        return 1;
    }
    return std::min(1.0f, viewportHeight() / contentHeight());
}
float ScrollConstraint::minOffsetX()
{
    if (loopsX())
    {
        return std::numeric_limits<float>::infinity();
    }
    return 0;
}
float ScrollConstraint::minOffsetY()
{
    if (loopsY())
    {
        return std::numeric_limits<float>::infinity();
    }
    return 0;
}
float ScrollConstraint::maxOffsetX()
{
    if (loopsX())
    {
        return -std::numeric_limits<float>::infinity();
    }
    return std::min(0.0f,
                    viewportWidth() - contentWidth() -
                        viewport()->paddingRight());
}
float ScrollConstraint::maxOffsetY()
{
    if (loopsY())
    {
        return -std::numeric_limits<float>::infinity();
    }
    return std::min(0.0f,
                    viewportHeight() - contentHeight() -
                        viewport()->paddingBottom());
}
float ScrollConstraint::clampedOffsetX()
{
    if (loopsX())
    {
        return offsetX();
    }
    if (maxOffsetX() > 0)
    {
        return 0;
    }
    if (m_physics != nullptr && m_physics->enabled())
    {
        return m_physics
            ->clamp(Vec2D(maxOffsetX(), maxOffsetY()),
                    Vec2D(minOffsetX(), minOffsetY()),
                    Vec2D(m_offsetX, m_offsetY))
            .x;
    }
    return math::clamp(m_offsetX, maxOffsetX(), 0);
}
float ScrollConstraint::clampedOffsetY()
{
    if (loopsY())
    {
        return offsetY();
    }
    if (maxOffsetY() > 0)
    {
        return 0;
    }
    if (m_physics != nullptr && m_physics->enabled())
    {
        return m_physics
            ->clamp(Vec2D(maxOffsetX(), maxOffsetY()),
                    Vec2D(minOffsetX(), minOffsetY()),
                    Vec2D(m_offsetX, m_offsetY))
            .y;
    }
    return math::clamp(m_offsetY, maxOffsetY(), 0);
}

void ScrollConstraint::offsetX(float value)
{
    if (m_offsetX == value)
    {
        return;
    }
    m_offsetX = value;
    content()->markWorldTransformDirty();
}
void ScrollConstraint::offsetY(float value)
{
    if (m_offsetY == value)
    {
        return;
    }
    m_offsetY = value;
    content()->markWorldTransformDirty();
}

bool ScrollConstraint::mainAxisIsColumn()
{
    return content() != nullptr && content()->mainAxisIsColumn();
}

bool ScrollConstraint::virtualizesGrid()
{
    if (!virtualize() || content() == nullptr ||
        !content()->isGridContainer() ||
        direction() == DraggableConstraintDirection::horizontal)
    {
        return false;
    }
    // Anything but a list keeps its cell while its row is unrealized, which
    // would shift realized items off their columns.
    return scrollChildrenAreLists();
}

bool ScrollConstraint::virtualizesGridColumns()
{
    // Columns past the viewport's sides are windowed too, however it scrolls.
    // A carousel only windows them when it cycles them, scrolled both ways.
    return virtualizesGrid() &&
           (!infinite() || direction() == DraggableConstraintDirection::all);
}

bool ScrollConstraint::indexesGridCells()
{
    return direction() == DraggableConstraintDirection::all &&
           virtualizesGridColumns();
}

bool ScrollConstraint::indexesHorizontally()
{
    // Scrolled both ways, only the virtualized axis has an item order.
    if (virtualize() && direction() == DraggableConstraintDirection::all)
    {
        return !virtualAxisIsColumn();
    }
    return constrainsHorizontal();
}

// A carousel cycles its virtualized axis, and a grid's columns when it
// scrolls both ways; any other axis it scrolls is bounded.
bool ScrollConstraint::loopsX()
{
    return infinite() && (!virtualAxisIsColumn() || virtualizesGridColumns());
}

bool ScrollConstraint::loopsY() { return infinite() && virtualAxisIsColumn(); }

bool ScrollConstraint::scrollChildrenAreLists()
{
    for (auto child : scrollChildren())
    {
        if (VirtualizingComponent::fromScrollChild(child) == nullptr)
        {
            return false;
        }
    }
    return true;
}

static float innerSize(LayoutComponent* layout, bool vertical)
{
    return vertical ? layout->layoutHeight() - layout->paddingTop() -
                          layout->paddingBottom()
                    : layout->layoutWidth() - layout->paddingLeft() -
                          layout->paddingRight();
}

// The content's margins along its lines' flow axis: points, or percents of
// its parent's inner width, as layout resolves them.
static float flowMargins(LayoutComponent* content,
                         LayoutComponent* parent,
                         bool flowIsVertical)
{
    auto style = content->style();
    if (style == nullptr)
    {
        return 0.0f;
    }
    float reference = innerSize(parent, false);
    auto resolve = [&](float value, uint8_t unit) {
        const uint8_t point = 1, percent = 2;
        return unit == point     ? value
               : unit == percent ? value / 100.0f * reference
                                 : 0.0f;
    };
    return flowIsVertical
               ? resolve(style->marginTop(), style->marginTopUnitsValue()) +
                     resolve(style->marginBottom(),
                             style->marginBottomUnitsValue())
               : resolve(style->marginLeft(), style->marginLeftUnitsValue()) +
                     resolve(style->marginRight(),
                             style->marginRightUnitsValue());
}

// A row track as the virtual layout sizes it.
static VirtualGridTrack virtualTrack(GridTrack* track)
{
    auto min = (VirtualTrackSizing)track->trackType();
    if (track->trackMaxType() == 0)
    {
        return {min, track->trackValue(), min, track->trackValue()};
    }
    return {min,
            track->trackValue(),
            (VirtualTrackSizing)(track->trackMaxType() - 1),
            track->trackMaxValue()};
}

bool ScrollConstraint::virtualAxisIsColumn()
{
    // Grid rows stack down the block axis.
    if (virtualizesGrid())
    {
        return true;
    }
    // Wrapped lines stack along the cross axis, so that's what scrolls.
    bool isColumn = mainAxisIsColumn();
    return virtualize() && content() != nullptr && content()->wrapsLines()
               ? !isColumn
               : isColumn;
}

void ScrollConstraint::constrain(TransformComponent* component)
{
    resolveScrollIntents();
    m_scrollTransform =
        Mat2D::fromTranslate(constrainsHorizontal() ? clampedOffsetX() : 0,
                             constrainsVertical() ? clampedOffsetY() : 0);
    m_childConstraintAppliedCount = 0;
}

void ScrollConstraint::constrainChild(LayoutNodeProvider* child)
{
    auto component = child->transformComponent();
    if (component == nullptr)
    {
        return;
    }
    auto targetTransform = offsetInParentFrame(component, m_scrollTransform);
    TransformConstraint::constrainWorld(component,
                                        component->worldTransform(),
                                        m_componentsA,
                                        targetTransform,
                                        m_componentsB,
                                        strength());
    m_childConstraintAppliedCount += 1;
    constrainVirtualized();
}

void ScrollConstraint::constrainVirtualized(bool force)
{
    if (virtualize() && m_virtualizer != nullptr)
    {
        // By reference: this runs for every constrained child, and the list
        // only changes in buildDependencies.
        auto& children = scrollChildren();
        if (m_childConstraintAppliedCount < children.size() && !force)
        {
            return;
        }
        refreshVirtualLayout();
        auto isColumn = virtualAxisIsColumn();
        anchorScroll(m_virtualizer->anchorMoved(*m_virtualLayout, children),
                     isColumn);
        auto virtualDirection = isColumn ? VirtualizedDirection::vertical
                                         : VirtualizedDirection::horizontal;
        auto offset = isColumn ? clampedOffsetY() : clampedOffsetX();
        // Items move along their lines when the scroll moves that axis.
        bool scrollsFlow =
            isColumn ? constrainsHorizontal() : constrainsVertical();
        float flowOffset =
            scrollsFlow ? -(isColumn ? clampedOffsetX() : clampedOffsetY())
                        : 0.0f;
        // A grid windows the columns the viewport shows, in content space.
        bool columns = virtualizesGridColumns();
        m_virtualizer->constrain(this,
                                 children,
                                 offset,
                                 virtualDirection,
                                 flowOffset,
                                 flowOffset - content()->layoutX(),
                                 columns ? viewport()->layoutWidth() : 0.0f,
                                 columns);
        syncVirtualGridTracks();
    }
}

// Size of a scroll child's item.
static Vec2D virtualItemSize(LayoutNodeProvider* child, int index)
{
    auto virt = VirtualizingComponent::fromScrollChild(child);
    if (virt != nullptr)
    {
        return virt->itemSize(index);
    }
    auto bounds = child->layoutBounds();
    return Vec2D(bounds.width(), bounds.height());
}

void ScrollConstraint::refreshVirtualLayout()
{
    if (m_virtualLayout == nullptr)
    {
        return;
    }
    bool isHorizontal = !virtualAxisIsColumn();
    auto contentGap = gap();
    auto layout = content();
    if (virtualizesGrid())
    {
        // Layout only ever sees the realized rows, so tracks are sized here
        // from every item; column starts come back from layout once it uses
        // them.
        int columns = 0;
        for (auto child : layout->children())
        {
            if (child->is<GridTrack>() &&
                child->as<GridTrack>()->gridCollection() ==
                    GridTrackCollection::templateColumns)
            {
                columns++;
            }
        }
        m_virtualLayout->beginGrid(contentGap.y,
                                   columns,
                                   layout->childAlignment().cross,
                                   contentGap.x);
        layout->gridColumnLineOffsets(m_virtualLayout->gridColumnStarts());

        auto& rows = m_virtualLayout->gridRows();
        auto& gridColumns = m_virtualLayout->gridColumns();
        for (auto child : layout->children())
        {
            if (!child->is<GridTrack>())
            {
                continue;
            }
            auto track = child->as<GridTrack>();
            std::vector<VirtualGridTrack>* tracks = nullptr;
            switch (track->gridCollection())
            {
                case GridTrackCollection::templateRows:
                    tracks = &rows.templates;
                    break;
                case GridTrackCollection::autoRows:
                    tracks = &rows.autos;
                    break;
                case GridTrackCollection::templateColumns:
                    tracks = &gridColumns.templates;
                    break;
                case GridTrackCollection::autoColumns:
                    tracks = &gridColumns.autos;
                    break;
            }
            if (tracks != nullptr)
            {
                tracks->push_back(virtualTrack(track));
            }
        }
        // A grid of fixed or filling size sizes its tracks in it; one that
        // hugs them sizes them first (its own size only reflects realized
        // rows), and again in the size they give it along a flex parent's
        // main axis.
        auto style = layout->style();
        const auto hug = (uint8_t)LayoutScaleType::hug;
        if (style != nullptr && style->layoutHeightScaleType() != hug)
        {
            rows.space = std::max(0.0f, innerSize(layout, true));
        }
        if (style != nullptr && style->layoutWidthScaleType() != hug)
        {
            gridColumns.space = std::max(0.0f, innerSize(layout, false));
        }
        auto parent = viewport();
        if (parent->style() != nullptr &&
            parent->style()->layoutTypeValue() == 0)
        {
            rows.resize = parent->mainAxisIsColumn();
            gridColumns.resize = !rows.resize;
        }
    }
    else if (layout->wrapsLines())
    {
        // Lines run along the flow axis, inside the content's padding. Hugging
        // content breaks them at the space its parent offers instead (its own
        // size only reflects the realized lines), less its margins.
        auto alignment = layout->childAlignment();
        bool hugs = layout->hugsLines();
        auto bounds = hugs ? viewport() : layout;
        float flowStart =
            isHorizontal ? layout->paddingTop() : layout->paddingLeft();
        float flowExtent = innerSize(bounds, isHorizontal);
        if (hugs)
        {
            flowExtent -= isHorizontal
                              ? layout->paddingTop() + layout->paddingBottom()
                              : layout->paddingLeft() + layout->paddingRight();
            flowExtent -= flowMargins(layout, viewport(), isHorizontal);
        }
        m_virtualLayout->beginWrap(isHorizontal ? contentGap.x : contentGap.y,
                                   isHorizontal ? contentGap.y : contentGap.x,
                                   flowStart,
                                   flowExtent,
                                   alignment.main,
                                   alignment.cross,
                                   hugs);
        // Layout positions items along a line exactly (grow, margins) when it
        // sees whole lines from a line start, as we realize them. A non-list
        // child keeps its slot while its line is unrealized, a carousel adds
        // its tail after its head, and hugging content justifies within its
        // realized lines only; those positions are ours.
        m_virtualLayout->flowFromLayout(!hugs && !infinite() &&
                                        scrollChildrenAreLists());
    }
    else
    {
        m_virtualLayout->beginLinear(isHorizontal ? contentGap.x
                                                  : contentGap.y);
    }
    for (auto child : scrollChildren())
    {
        // Every child gets a segment, so segments index like scrollChildren.
        m_virtualLayout->beginSegment();
        if (child == nullptr)
        {
            continue;
        }
        int count = (int)child->numLayoutNodes();
        for (int j = 0; j < count; j++)
        {
            auto size = virtualItemSize(child, j);
            m_virtualLayout->addItem(isHorizontal ? size.x : size.y,
                                     isHorizontal ? size.y : size.x);
        }
    }
    m_virtualLayout->end();
}

void ScrollConstraint::anchorScroll(float moved, bool isColumn)
{
    // Content before the first item on screen changed size (it measured
    // differently from its authored size): move with it so what's on screen
    // stays put. At the start nothing is above to anchor against, so growth
    // shows below instead.
    // The live offset: direct writes (scrollToPosition, intents, tests) move it
    // without the stored scroll offset.
    float offset = isColumn ? offsetY() : offsetX();
    if (moved == 0.0f || offset >= 0.0f)
    {
        return;
    }
    if (isColumn)
    {
        scrollOffsetY(offset - moved);
        offsetY(offset - moved);
        m_lastFrameOffsetY -= moved;
    }
    else
    {
        scrollOffsetX(offset - moved);
        offsetX(offset - moved);
        m_lastFrameOffsetX -= moved;
    }
    if (m_physics != nullptr)
    {
        m_physics->shift(isColumn ? Vec2D(0.0f, -moved) : Vec2D(-moved, 0.0f));
    }
}

void ScrollConstraint::syncVirtualGridTracks()
{
    static const std::vector<float> none;
    auto layout = content();
    if (layout == nullptr)
    {
        return;
    }
    if (!virtualizesGrid() || m_virtualLayout == nullptr ||
        !m_virtualLayout->isGrid())
    {
        layout->virtualGridTracks(none, none);
        return;
    }
    // Every column takes the size the layout sized from all of its items; a
    // grid without template columns has one implicit column.
    auto& columns = m_virtualLayout->gridColumnSizes();
    columns.clear();
    for (int column = 0; column < m_virtualLayout->columnCount(); column++)
    {
        columns.push_back(m_virtualLayout->gridColumnSize(column));
    }
    // Realized rows reach layout in item order. A window follows it, except a
    // carousel's, which can wrap from the tail to the head; pinned cells take
    // their rows in window order instead.
    auto& rows = m_virtualLayout->gridRowSizes();
    rows.clear();
    int lineCount = m_virtualLayout->lineCount();
    int start = m_virtualizer->realizedLineStart();
    int end = m_virtualizer->realizedLineEnd();
    auto pushRows = [&](int from, int to) {
        for (int line = from; line <= to; line++)
        {
            rows.push_back(m_virtualLayout->lineExtent(line));
        }
    };
    if (lineCount > 0 && end >= start)
    {
        // A carousel window wrapping past the tail takes the head first.
        int first = m_virtualLayout->wrapLine(start);
        int span = std::min(end - start + 1, lineCount);
        if (!infinite() || virtualizesGridColumns())
        {
            pushRows(start, end);
        }
        else if (first + span <= lineCount)
        {
            pushRows(first, first + span - 1);
        }
        else
        {
            pushRows(0, first + span - 1 - lineCount);
            pushRows(first, lineCount - 1);
        }
    }
    layout->virtualGridTracks(columns, rows);
}

const VirtualLayout* ScrollConstraint::ensureVirtualLayout()
{
    // Asked before the first constrain: build on demand.
    if (m_virtualLayout != nullptr &&
        m_virtualLayout->segmentCount() != (int)scrollChildren().size())
    {
        refreshVirtualLayout();
    }
    return m_virtualLayout;
}

Vec2D ScrollConstraint::virtualItemPosition(LayoutNodeProvider* child,
                                            int index)
{
    if (m_virtualLayout == nullptr)
    {
        return Vec2D();
    }
    auto& children = scrollChildren();
    auto it = std::find(children.begin(), children.end(), child);
    if (it == children.end())
    {
        return Vec2D();
    }
    int segment = (int)(it - children.begin());
    ensureVirtualLayout();
    // Asked before a grown list's constrain: build on demand.
    if (m_virtualLayout->segmentStart(segment) + index >=
        m_virtualLayout->itemCount())
    {
        refreshVirtualLayout();
    }
    int item = m_virtualLayout->segmentStart(segment) + index;
    if (item >= m_virtualLayout->itemCount())
    {
        return Vec2D();
    }
    float scroll =
        m_virtualLayout->lineStart(m_virtualLayout->lineOfItem(item)) +
        m_virtualLayout->itemLineOffset(item);
    float flow = m_virtualLayout->itemFlowOffset(item);
    return virtualAxisIsColumn() ? Vec2D(flow, scroll) : Vec2D(scroll, flow);
}

void ScrollConstraint::addLayoutChild(LayoutNodeProvider* child)
{
    m_layoutChildren.push_back(child);
}

void ScrollConstraint::dragView(Vec2D delta,
                                float timeStamp,
                                bool trackVelocity)
{
    // Scale once so the inertial release phase matches the drag feel.
    Vec2D scaledDelta(delta.x * dragMultiplier(), delta.y * dragMultiplier());
    if (m_physics != nullptr)
    {
        if (trackVelocity)
        {
            m_physics->accumulate(scaledDelta, timeStamp);
        }
        scrollOffsetX(offsetX() + scaledDelta.x);
        scrollOffsetY(offsetY() + scaledDelta.y);
        return;
    }
    // Without physics nothing ever pulls the stored offset back into range,
    // so overscroll would accumulate out of view and eat the next drag.
    float x = offsetX() + scaledDelta.x;
    float y = offsetY() + scaledDelta.y;
    if (!loopsX())
    {
        x = maxOffsetX() > 0 ? 0 : math::clamp(x, maxOffsetX(), 0);
    }
    if (!loopsY())
    {
        y = maxOffsetY() > 0 ? 0 : math::clamp(y, maxOffsetY(), 0);
    }
    scrollOffsetX(x);
    scrollOffsetY(y);
}

// dragMultiplier is bindable and unclamped, so interest has to be judged on
// the delta as applied: zero moves nothing, negative moves the other way.
Vec2D ScrollConstraint::scaledDelta(Vec2D delta)
{
    return Vec2D(delta.x * dragMultiplier(), delta.y * dragMultiplier());
}

// dragView writes unclamped, so only the elastic clamp hides the excess.
bool ScrollConstraint::isOverscrolled()
{
    return (constrainsHorizontal() && !loopsX() &&
            offsetX() != clampResolvedOffset(offsetX(), true)) ||
           (constrainsVertical() && !loopsY() &&
            offsetY() != clampResolvedOffset(offsetY(), false));
}

// An elastic view stretches at its edge instead of chaining, so this tests
// range on the delta's axes rather than available movement.
bool ScrollConstraint::canStretch(Vec2D rawDelta)
{
    Vec2D delta = scaledDelta(rawDelta);
    if (!wheelEnabled() || !hasLayoutParent() ||
        parent()->parent() == nullptr ||
        !parent()->parent()->is<LayoutComponent>() || m_physics == nullptr ||
        physicsType() != ScrollPhysicsType::elastic)
    {
        return false;
    }
    bool wantsX = constrainsHorizontal() && delta.x != 0;
    bool wantsY = constrainsVertical() && delta.y != 0;
    return (wantsX && (loopsX() || maxOffsetX() < 0)) ||
           (wantsY && (loopsY() || maxOffsetY() < 0));
}

bool ScrollConstraint::canConsume(Vec2D rawDelta)
{
    Vec2D delta = scaledDelta(rawDelta);
    if (!wheelEnabled() || !hasLayoutParent() ||
        parent()->parent() == nullptr ||
        !parent()->parent()->is<LayoutComponent>())
    {
        return false;
    }
    bool wantsX = constrainsHorizontal() && delta.x != 0;
    bool wantsY = constrainsVertical() && delta.y != 0;
    // A looping axis wraps forever, so any delta on it moves.
    bool movesX = loopsX() || clampResolvedOffset(offsetX() + delta.x, true) !=
                                  clampResolvedOffset(offsetX(), true);
    bool movesY = loopsY() || clampResolvedOffset(offsetY() + delta.y, false) !=
                                  clampResolvedOffset(offsetY(), false);
    return (wantsX && movesX) || (wantsY && movesY);
}

void ScrollConstraint::scrollBy(Vec2D delta)
{
    // Shares dragMultiplier with the drag path: it is the view's sensitivity.
    Vec2D scaled(delta.x * dragMultiplier(), delta.y * dragMultiplier());
    if (constrainsHorizontal())
    {
        scrollOffsetX(clampResolvedOffset(offsetX() + scaled.x, true));
    }
    if (constrainsVertical())
    {
        scrollOffsetY(clampResolvedOffset(offsetY() + scaled.y, false));
    }
}

bool ScrollConstraint::beginScrollGesture()
{
    markScrollActivity();
    if (m_isScrolling)
    {
        return false;
    }
    m_isScrolling = true;
    // User interaction supersedes held intents.
    clearScrollIntents();
    m_lastFrameOffsetX = scrollOffsetX();
    m_lastFrameOffsetY = scrollOffsetY();
    return true;
}

void ScrollConstraint::endScrollGesture()
{
    m_isScrolling = false;
    m_scrollIdleSeconds = 0;
    // m_isDragging belongs to the pointer path; a pointer drag can overlap
    // this gesture and must outlive it.
}

std::vector<Vec2D> ScrollConstraint::collectSnapPoints()
{
    std::vector<Vec2D> snappingPoints;
    if (content() == nullptr)
    {
        return snappingPoints;
    }
    for (auto child : scrollChildren())
    {
        if (child == nullptr)
        {
            continue;
        }
        int nodeCount = (int)child->numLayoutNodes();
        for (int j = 0; j < nodeCount; j++)
        {
            auto bounds = child->layoutBoundsForNode(j);
            if (isBoundsCollapsed(bounds))
            {
                continue;
            }
            snappingPoints.push_back(Vec2D(bounds.left(), bounds.top()));
        }
    }
    return snappingPoints;
}

void ScrollConstraint::runPhysics()
{
    m_isDragging = false;
    startPhysics();
}

void ScrollConstraint::startPhysics()
{
    std::vector<Vec2D> snappingPoints =
        snap() ? collectSnapPoints() : std::vector<Vec2D>();
    if (m_physics != nullptr)
    {
        m_physics->run(Vec2D(maxOffsetX(), maxOffsetY()),
                       Vec2D(minOffsetX(), minOffsetY()),
                       Vec2D(offsetX(), offsetY()),
                       snappingPoints,
                       Vec2D(contentWidth(), contentHeight()),
                       Vec2D(viewportWidth(), viewportHeight()));
    }
}

bool ScrollConstraint::advanceComponent(float elapsedSeconds,
                                        AdvanceFlags flags)
{
    if (isCollapsed())
    {
        // offsetX(0);
        // offsetY(0);
        // A collapsed view never reaches the idle timer below, so a gesture
        // left on it would hold the state machine's latch forever.
        if (m_isScrolling)
        {
            stopPhysics();
            clearVelocity();
            endScrollGesture();
        }
        return false;
    }
    if ((flags & AdvanceFlags::AdvanceNested) != AdvanceFlags::AdvanceNested)
    {
        return false;
    }
    if (m_isScrolling &&
        (flags & AdvanceFlags::NewFrame) == AdvanceFlags::NewFrame)
    {
        m_scrollIdleSeconds += elapsedSeconds;
        if (m_scrollIdleSeconds >= scrollIdleSeconds)
        {
            // A gesture that reports no end settles here instead.
            if (snap() || isOverscrolled())
            {
                // The wheel path never primed, so the helpers may not exist.
                // prepare also zeroes velocity, so nothing flings from here.
                primePhysics();
                startPhysics();
            }
            endScrollGesture();
        }
    }
    if (m_physics == nullptr)
    {
        return m_isScrolling;
    }
    if (m_physics->isRunning())
    {
        auto offset = m_physics->advance(elapsedSeconds);
        scrollOffsetX(offset.x);
        scrollOffsetY(offset.y);
    }
    // Detect a paused pointer during a drag, or paused fingers during a
    // trackpad gesture: if scrollOffset hasn't changed since the last
    // new-frame advance, accumulate() didn't fire and speed is stale.
    if ((flags & AdvanceFlags::NewFrame) == AdvanceFlags::NewFrame)
    {
        bool hasMoved = scrollOffsetX() != m_lastFrameOffsetX ||
                        scrollOffsetY() != m_lastFrameOffsetY;
        if ((m_isScrollBarDragging || m_isDragging || m_isScrolling) &&
            !hasMoved)
        {
            clearVelocity();
        }
        m_lastFrameOffsetX = scrollOffsetX();
        m_lastFrameOffsetY = scrollOffsetY();
    }
    // Keep advancing while a drag is active so the next new-frame advance can
    // detect a paused pointer and clear velocity when the pointer holds still.
    // A live scroll gesture keeps frames coming for its idle timer.
    return m_physics->enabled() || m_isScrollBarDragging || m_isDragging ||
           m_isScrolling;
}

std::vector<DraggableProxy*> ScrollConstraint::draggables()
{
    std::vector<DraggableProxy*> items;
    items.push_back(new ViewportDraggableProxy(this, viewport()->proxy()));
    return items;
}

void ScrollConstraint::buildDependencies()
{
    Super::buildDependencies();
    // draggables() hands the viewport's proxy out as the drag hit target, so it
    // must be injected into the draw order even if the viewport never paints or
    // clips. Stamp it here, before the artboard's one-time proxy injection.
    if (parent() != nullptr && parent()->parent() != nullptr &&
        parent()->parent()->is<LayoutComponent>())
    {
        viewport()->markInteractionTarget();
    }
    m_hasListChildren = false;
    for (auto child : content()->children())
    {
        auto layout = LayoutNodeProvider::from(child);
        if (layout != nullptr)
        {
            addDependent(child);
            layout->addLayoutConstraint(static_cast<LayoutConstraint*>(this));
        }
        if (child->is<ArtboardComponentList>())
        {
            m_hasListChildren = true;
        }
    }
}

Core* ScrollConstraint::clone() const
{
    auto cloned = ScrollConstraintBase::clone();
    if (physics() != nullptr)
    {
        auto constraint = cloned->as<ScrollConstraint>();
        auto clonedPhysics = physics()->clone()->as<ScrollPhysics>();
        constraint->physics(clonedPhysics);
    }
    return cloned;
}

StatusCode ScrollConstraint::import(ImportStack& importStack)
{
    auto backboardImporter =
        importStack.latest<BackboardImporter>(BackboardBase::typeKey);
    if (backboardImporter != nullptr)
    {
        std::vector<ScrollPhysics*> physicsObjects =
            backboardImporter->physics();
        if (physicsId() != -1 && physicsId() < physicsObjects.size())
        {
            auto phys = physicsObjects[physicsId()];
            if (phys != nullptr)
            {
                auto cloned = phys->clone()->as<ScrollPhysics>();
                physics(cloned);
            }
        }
    }
    else
    {
        return StatusCode::MissingObject;
    }
    return Super::import(importStack);
}

void ScrollConstraint::virtualizeChanged()
{
    if (virtualize())
    {
        // The next pass recycles whatever the lists hold off screen.
        if (m_virtualizer == nullptr)
        {
            m_virtualizer = new ScrollVirtualizer();
        }
        if (m_virtualLayout == nullptr)
        {
            m_virtualLayout = new VirtualLayout();
        }
        markConstraintDirty();
        return;
    }
    delete m_virtualizer;
    m_virtualizer = nullptr;
    delete m_virtualLayout;
    m_virtualLayout = nullptr;
    if (!hasLayoutParent())
    {
        return;
    }
    // Layout takes every item from here: undo the rows and cells we gave it,
    // and realize what the window left out.
    syncVirtualGridTracks();
    std::vector<int> held;
    for (auto child : scrollChildren())
    {
        auto virt = VirtualizingComponent::fromScrollChild(child);
        if (virt == nullptr)
        {
            continue;
        }
        held.clear();
        virt->realizedIndices(held);
        for (int index : held)
        {
            virt->setVirtualizableCell(index, -1, -1);
        }
        virt->clearVirtualWindow();
        for (int index = 0; index < virt->itemCount(); index++)
        {
            if (virt->item(index) == nullptr)
            {
                virt->addVirtualizable(index);
            }
        }
        virt->virtualizableChanged();
    }
    markConstraintDirty();
}

StatusCode ScrollConstraint::onAddedDirty(CoreContext* context)
{
    StatusCode result = Super::onAddedDirty(context);
    if (virtualize())
    {
        m_virtualizer = new ScrollVirtualizer();
        m_virtualLayout = new VirtualLayout();
    }
    offsetX(scrollOffsetX());
    offsetY(scrollOffsetY());
    return result;
}

// A finished settle calls reset(), which destroys the elastic helpers, so a
// live gesture can lose them mid-flight. Without them the elastic clamp is
// gone and the view renders pinned at its edge while dragView still
// accumulates -- the stretch reappears in one jump on the next run().
// Asked of the physics, not of enabled(): for clamped physics enabled() is
// just isRunning(), which would re-prime on every event and never let
// velocity accumulate.
bool ScrollConstraint::ensurePhysicsPrimed()
{
    if (m_physics != nullptr && !m_physics->isPrimed())
    {
        primePhysics();
        return true;
    }
    return false;
}

void ScrollConstraint::initPhysics()
{
    m_isDragging = true;
    primePhysics();
}

void ScrollConstraint::primePhysics()
{
    // User interaction supersedes held intents.
    clearScrollIntents();
    m_lastFrameOffsetX = scrollOffsetX();
    m_lastFrameOffsetY = scrollOffsetY();
    if (m_physics != nullptr)
    {
        m_physics->prepare(direction());
    }
}

void ScrollConstraint::stopPhysics()
{
    if (m_physics != nullptr)
    {
        m_physics->reset();
    }
}

void ScrollConstraint::clearVelocity()
{
    if (m_physics != nullptr)
    {
        m_physics->clearVelocity();
    }
}

float ScrollConstraint::maxOffsetXForPercent()
{
    return loopsX() ? contentWidth() : maxOffsetX();
}

float ScrollConstraint::maxOffsetYForPercent()
{
    return loopsY() ? contentHeight() : maxOffsetY();
}

float ScrollConstraint::velocityX()
{
    return m_physics != nullptr ? m_physics->speed().x : 0.0f;
}

float ScrollConstraint::velocityY()
{
    return m_physics != nullptr ? m_physics->speed().y : 0.0f;
}

bool ScrollConstraint::scrollActive()
{
    return m_isDragging || m_isScrollBarDragging || m_isScrolling ||
           (m_physics != nullptr && m_physics->isRunning());
}

float ScrollConstraint::scrollPercentX()
{
    if (m_intentX.space == ScrollSpace::percent)
    {
        return m_intentX.value;
    }
    return maxOffsetX() != 0 ? scrollOffsetX() / maxOffsetXForPercent() : 0;
}

float ScrollConstraint::scrollPercentY()
{
    if (m_intentY.space == ScrollSpace::percent)
    {
        return m_intentY.value;
    }
    return maxOffsetY() != 0 ? scrollOffsetY() / maxOffsetYForPercent() : 0;
}

float ScrollConstraint::scrollIndex()
{
    const ScrollAxisIntent& intent =
        indexesHorizontally() ? m_intentX : m_intentY;
    if (intent.space == ScrollSpace::index)
    {
        return intent.value;
    }
    return indexAtPosition(Vec2D(scrollOffsetX(), scrollOffsetY()));
}

void ScrollConstraint::setScrollPercentX(float value)
{
    if (m_isDragging)
    {
        return;
    }
    stopPhysics();
    setIntentX({ScrollSpace::percent, value});
}

void ScrollConstraint::setScrollPercentY(float value)
{
    if (m_isDragging)
    {
        return;
    }
    stopPhysics();
    setIntentY({ScrollSpace::percent, value});
}

void ScrollConstraint::setScrollIndex(float value)
{
    if (m_isDragging)
    {
        return;
    }
    stopPhysics();
    if (constrainsHorizontal())
    {
        setIntentX({ScrollSpace::index, value});
    }
    if (constrainsVertical())
    {
        setIntentY({ScrollSpace::index, value});
    }
}

// False while hidden or before the first layout pass on the given axis.
bool ScrollConstraint::scrollLayoutResolvable(bool isX)
{
    auto vp = viewport();
    if (vp == nullptr)
    {
        return false;
    }
    return isX ? vp->layoutWidth() > 0 : vp->layoutHeight() > 0;
}

// Resolved offsets clamp to the scrollable range.
float ScrollConstraint::clampResolvedOffset(float value, bool isX)
{
    if (isX ? loopsX() : loopsY())
    {
        return value;
    }
    return math::clamp(value, isX ? maxOffsetX() : maxOffsetY(), 0.0f);
}

// Convert an intent to an offset; false if layout can't resolve yet.
bool ScrollConstraint::resolveIntent(const ScrollAxisIntent& intent,
                                     bool isX,
                                     float& outOffset)
{
    // Non-finite indices resolve to the origin without needing layout.
    if (intent.space == ScrollSpace::index &&
        (std::isnan(intent.value) ||
         (infinite() && !std::isfinite(intent.value))))
    {
        outOffset = 0;
        return true;
    }
    // Gate both spaces: virtualized lists compute item bounds from authored
    // sizes, which resolve even while the viewport layout is collapsed.
    if (!scrollLayoutResolvable(isX))
    {
        return false;
    }
    switch (intent.space)
    {
        case ScrollSpace::percent:
        {
            float contentSize = isX ? contentWidth() : contentHeight();
            if (contentSize <= 0)
            {
                // No measured content — can't resolve.
                return false;
            }
            float maxForPercent =
                isX ? maxOffsetXForPercent() : maxOffsetYForPercent();
            outOffset = clampResolvedOffset(intent.value * maxForPercent, isX);
            return true;
        }
        case ScrollSpace::index:
        {
            Vec2D to;
            if (!positionAtIndex(intent.value, to))
            {
                return false;
            }
            outOffset = clampResolvedOffset(isX ? to.x : to.y, isX);
            return true;
        }
        case ScrollSpace::none:
            break;
    }
    return false;
}

// Resolve eagerly; unresolved intents are held and retried in constrain().
void ScrollConstraint::setIntentX(ScrollAxisIntent intent)
{
    float resolved;
    if (!resolveIntent(intent, true, resolved))
    {
        m_intentX = intent;
        return;
    }
    m_intentX.space = ScrollSpace::none;
    scrollOffsetX(resolved);
}

void ScrollConstraint::setIntentY(ScrollAxisIntent intent)
{
    float resolved;
    if (!resolveIntent(intent, false, resolved))
    {
        m_intentY = intent;
        return;
    }
    m_intentY.space = ScrollSpace::none;
    scrollOffsetY(resolved);
}

// Retry held intents; called from constrain() where layout is fresh.
void ScrollConstraint::resolveScrollIntents()
{
    if (m_intentX.space != ScrollSpace::none)
    {
        setIntentX(m_intentX);
    }
    if (m_intentY.space != ScrollSpace::none)
    {
        setIntentY(m_intentY);
    }
}

bool ScrollConstraint::positionAtIndex(float index, Vec2D& outPosition)
{
    if (std::isnan(index) || (infinite() && !std::isfinite(index)))
    {
        outPosition = Vec2D();
        return true;
    }
    auto count = scrollItemCount();
    if (content() == nullptr || count == 0)
    {
        return false;
    }
    Vec2D contentGap = gap();
    float normalizedIndex;
    if (infinite())
    {
        normalizedIndex = std::fmod(index, (float)count);
        if (normalizedIndex < 0)
        {
            normalizedIndex += (float)count;
        }
    }
    else
    {
        normalizedIndex = std::max(index, 0.0f);
        // Past the last item: target the far end (clamps to max scroll).
        if (normalizedIndex >= (float)count)
        {
            if (contentWidth() <= 0 && contentHeight() <= 0)
            {
                // No measured content — can't resolve.
                return false;
            }
            outPosition = Vec2D(-contentWidth(), -contentHeight());
            return true;
        }
    }
    float floorIndex = std::floor(normalizedIndex);
    float mod = normalizedIndex - floorIndex;
    uint32_t targetIndex = (uint32_t)floorIndex;

    // No list children: O(1) direct index into scrollChildren.
    if (!m_hasListChildren)
    {
        // Target is visible — return its position with fractional offset.
        auto bounds = boundsForFlatIndex(targetIndex);
        if (!isBoundsCollapsed(bounds))
        {
            outPosition =
                Vec2D(-bounds.left() - (bounds.width() + contentGap.x) * mod,
                      -bounds.top() - (bounds.height() + contentGap.y) * mod);
            return true;
        }
        // Target is collapsed — walk forward to next visible item.
        for (uint32_t k = targetIndex + 1; k < (uint32_t)count; k++)
        {
            auto b = boundsForFlatIndex(k);
            if (!isBoundsCollapsed(b))
            {
                outPosition = Vec2D(-b.left(), -b.top());
                return true;
            }
        }
        if (infinite())
        {
            // Carousel: wrap around from the beginning.
            for (uint32_t k = 0; k < targetIndex; k++)
            {
                auto b = boundsForFlatIndex(k);
                if (!isBoundsCollapsed(b))
                {
                    outPosition = Vec2D(-b.left(), -b.top());
                    return true;
                }
            }
        }
        else
        {
            // End of list: walk backward to last visible item.
            for (int k = (int)targetIndex - 1; k >= 0; k--)
            {
                auto b = boundsForFlatIndex(k);
                if (!isBoundsCollapsed(b))
                {
                    outPosition = Vec2D(-b.left(), -b.top());
                    return true;
                }
            }
        }
        // Nothing visible — can't resolve.
        return false;
    }

    // Has list children: single-pass through nested child→node structure.
    auto& children = scrollChildren();
    uint32_t flatIndex = 0;
    Vec2D lastVisibleBeforeTarget;
    bool hasVisibleBeforeTarget = false;
    bool reachedTarget = false;

    for (auto child : children)
    {
        if (child == nullptr)
        {
            continue;
        }
        int nodeCount = (int)child->numLayoutNodes();
        for (int j = 0; j < nodeCount; j++, flatIndex++)
        {
            auto bounds = child->layoutBoundsForNode(j);
            // Before target: track last visible for backward fallback.
            if (flatIndex < targetIndex)
            {
                if (!isBoundsCollapsed(bounds))
                {
                    lastVisibleBeforeTarget =
                        Vec2D(-bounds.left(), -bounds.top());
                    hasVisibleBeforeTarget = true;
                }
                continue;
            }
            // At target: return position if visible, otherwise keep walking.
            if (flatIndex == targetIndex)
            {
                reachedTarget = true;
                if (!isBoundsCollapsed(bounds))
                {
                    outPosition = Vec2D(
                        -bounds.left() - (bounds.width() + contentGap.x) * mod,
                        -bounds.top() - (bounds.height() + contentGap.y) * mod);
                    return true;
                }
                continue;
            }
            // Past target: return first visible item found.
            if (!isBoundsCollapsed(bounds))
            {
                outPosition = Vec2D(-bounds.left(), -bounds.top());
                return true;
            }
        }
    }

    if (!reachedTarget)
    {
        return false;
    }

    // No visible item after target.
    if (infinite())
    {
        // Carousel: wrap around from the beginning.
        flatIndex = 0;
        for (auto child : children)
        {
            if (child == nullptr)
            {
                continue;
            }
            int nodeCount = (int)child->numLayoutNodes();
            for (int j = 0; j < nodeCount; j++, flatIndex++)
            {
                if (flatIndex >= targetIndex)
                {
                    // Nothing visible — can't resolve.
                    return false;
                }
                auto bounds = child->layoutBoundsForNode(j);
                if (!isBoundsCollapsed(bounds))
                {
                    outPosition = Vec2D(-bounds.left(), -bounds.top());
                    return true;
                }
            }
        }
    }
    else if (hasVisibleBeforeTarget)
    {
        // End of list: fall back to last visible item before target.
        outPosition = lastVisibleBeforeTarget;
        return true;
    }

    // Nothing visible — can't resolve.
    return false;
}

float ScrollConstraint::indexAtPosition(Vec2D pos)
{
    if (content() == nullptr || content()->children().size() == 0)
    {
        return 0;
    }
    // A grid scrolled both ways: the row at y, then the column at x.
    if (indexesGridCells() && m_virtualLayout != nullptr)
    {
        ensureVirtualLayout();
        if (m_virtualLayout->lineCount() == 0)
        {
            return 0;
        }
        // A carousel's position wraps into its first cycle.
        float y = -pos.y;
        float cycle = m_virtualLayout->cycleExtent();
        if (loopsY() && cycle > 0.0f)
        {
            y -= std::floor(y / cycle) * cycle;
        }
        int row = m_virtualLayout->window(y, 0.0f, loopsY(), 0).visibleStart;
        int column = m_virtualLayout->wrapColumn(
            m_virtualLayout->columnWindow(-pos.x, 0.0f, 0, loopsX())
                .visibleStart);
        return (float)std::min(m_virtualLayout->lineFirstItem(row) + column,
                               m_virtualLayout->lineLastItem(row));
    }
    Vec2D contentGap = gap();
    if (!m_hasListChildren)
    {
        size_t count = scrollChildren().size();
        if (indexesHorizontally())
        {
            for (size_t i = 0; i < count; i++)
            {
                auto bounds = scrollChildren()[i]->layoutBoundsForNode(0);
                float step = bounds.width() + contentGap.x;
                if (pos.x > -bounds.left() - step)
                {
                    return step != 0
                               ? (float)i + (-pos.x - bounds.left()) / step
                               : (float)i;
                }
            }
            return (float)count;
        }
        else if (constrainsVertical())
        {
            for (size_t i = 0; i < count; i++)
            {
                auto bounds = scrollChildren()[i]->layoutBoundsForNode(0);
                float step = bounds.height() + contentGap.y;
                if (pos.y > -bounds.top() - step)
                {
                    return step != 0 ? (float)i + (-pos.y - bounds.top()) / step
                                     : (float)i;
                }
            }
            return (float)count;
        }
        return 0;
    }
    // Has list children: nested iteration visiting each node once.
    float flatIndex = 0.0f;
    if (indexesHorizontally())
    {
        for (auto child : scrollChildren())
        {
            if (child == nullptr)
            {
                continue;
            }
            int nodeCount = (int)child->numLayoutNodes();
            for (int j = 0; j < nodeCount; j++)
            {
                auto bounds = child->layoutBoundsForNode(j);
                float step = bounds.width() + contentGap.x;
                if (pos.x > -bounds.left() - step)
                {
                    return step != 0 ? (flatIndex + j) +
                                           (-pos.x - bounds.left()) / step
                                     : (flatIndex + j);
                }
            }
            flatIndex += nodeCount;
        }
        return flatIndex;
    }
    else if (constrainsVertical())
    {
        for (auto child : scrollChildren())
        {
            if (child == nullptr)
            {
                continue;
            }
            int nodeCount = (int)child->numLayoutNodes();
            for (int j = 0; j < nodeCount; j++)
            {
                auto bounds = child->layoutBoundsForNode(j);
                float step = bounds.height() + contentGap.y;
                if (pos.y > -bounds.top() - step)
                {
                    return step != 0 ? (flatIndex + j) +
                                           (-pos.y - bounds.top()) / step
                                     : (flatIndex + j);
                    (bounds.height() + contentGap.y);
                }
            }
            flatIndex += nodeCount;
        }
        return flatIndex;
    }
    return 0;
}

bool ScrollConstraint::isBoundsCollapsed(AABB bounds)
{
    return (constrainsHorizontal() && bounds.width() <= 0) ||
           (constrainsVertical() && bounds.height() <= 0);
}

size_t ScrollConstraint::scrollItemCount()
{
    if (!m_hasListChildren)
    {
        return scrollChildren().size();
    }
    size_t count = 0;
    for (auto child : scrollChildren())
    {
        if (child == nullptr)
        {
            continue;
        }
        count += child->numLayoutNodes();
    }
    return count;
}

AABB ScrollConstraint::boundsForFlatIndex(size_t index)
{
    auto& children = scrollChildren();
    if (!m_hasListChildren)
    {
        if (index < children.size() && children[index] != nullptr)
        {
            return children[index]->layoutBoundsForNode(0);
        }
        return AABB();
    }
    size_t flatIndex = 0;
    for (auto child : children)
    {
        if (child == nullptr)
        {
            continue;
        }
        size_t nodeCount = child->numLayoutNodes();
        if (index < flatIndex + nodeCount)
        {
            return child->layoutBoundsForNode((int)(index - flatIndex));
        }
        flatIndex += nodeCount;
    }
    return AABB();
}

Vec2D ScrollConstraint::gap()
{
    if (content() == nullptr)
    {
        return Vec2D();
    }
    return Vec2D(content()->gapHorizontal(), content()->gapVertical());
}

void ScrollConstraint::scrollToPosition(float targetX, float targetY)
{
    // Direct offset writes supersede held intents.
    clearScrollIntents();
    if (m_physics == nullptr)
    {
        // No physics, just set offset directly
        scrollOffsetX(targetX);
        scrollOffsetY(targetY);
        return;
    }

    Vec2D current(m_offsetX, m_offsetY);
    Vec2D target(targetX, targetY);
    Vec2D rangeMin(maxOffsetX(), maxOffsetY());
    Vec2D rangeMax(0.0f, 0.0f);

    m_physics->scrollToPosition(current,
                                target,
                                rangeMin,
                                rangeMax,
                                constrainsHorizontal(),
                                constrainsVertical());
}

static float nearestSnapInDirection(float current,
                                    float target,
                                    const std::vector<Vec2D>& snapPoints,
                                    bool useX)
{
    if (current == target)
    {
        return target;
    }
    bool scrollingNegative = target < current;
    float best = target;
    bool found = false;
    float bestDist = 0.0f;
    for (const auto& p : snapPoints)
    {
        float c = useX ? -p.x : -p.y;
        if (scrollingNegative ? c > target : c < target)
        {
            continue;
        }
        float d = scrollingNegative ? target - c : c - target;
        if (!found || d < bestDist)
        {
            bestDist = d;
            best = c;
            found = true;
        }
    }
    return found ? best : target;
}

Vec2D ScrollConstraint::nearestSnapOffsetInDirection(Vec2D current,
                                                     Vec2D target)
{
    if (!snap())
    {
        return target;
    }
    auto snapPoints = collectSnapPoints();
    if (snapPoints.empty())
    {
        return target;
    }
    float snappedX =
        constrainsHorizontal()
            ? nearestSnapInDirection(current.x, target.x, snapPoints, true)
            : target.x;
    float snappedY =
        constrainsVertical()
            ? nearestSnapInDirection(current.y, target.y, snapPoints, false)
            : target.y;
    return Vec2D(snappedX, snappedY);
}

float ScrollConstraint::effectiveScrollOffsetX()
{
    if (m_physics != nullptr && m_physics->isRunning() &&
        m_physics->hasTargetX())
    {
        return m_physics->targetX();
    }
    return scrollOffsetX();
}

float ScrollConstraint::effectiveScrollOffsetY()
{
    if (m_physics != nullptr && m_physics->isRunning() &&
        m_physics->hasTargetY())
    {
        return m_physics->targetY();
    }
    return scrollOffsetY();
}