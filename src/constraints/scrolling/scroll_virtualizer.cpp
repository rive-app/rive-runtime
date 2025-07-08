#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/constraints/scrolling/scroll_virtualizer.hpp"

using namespace rive;

ScrollVirtualizer::~ScrollVirtualizer() { reset(); }

void ScrollVirtualizer::reset() { m_visibleIndexStart = m_visibleIndexEnd = 0; }

bool ScrollVirtualizer::constrain(ScrollConstraint* scroll,
                                  std::vector<LayoutNodeProvider*>& children,
                                  float offset,
                                  VirtualizedDirection direction)
{
    bool isHorz = direction == VirtualizedDirection::horizontal;
    double contentSize =
        isHorz ? scroll->contentWidth() : scroll->contentHeight();
    if (contentSize > 0.0f)
    {
        float normalizedOffset = -offset;
        m_direction = direction;
        m_viewportSize =
            isHorz ? scroll->viewportWidth() : scroll->viewportHeight();
        m_infinite = scroll->infinite();
        if (offset > 0.0f)
        {
            if (m_infinite)
            {
                int offsetMultiplier =
                    static_cast<int>(std::floor(offset / contentSize)) + 1;
                m_offset = -1.0f * (offset - (offsetMultiplier * contentSize));
            }
            else
            {
                m_offset = -offset;
            }
        }
        else
        {
            int offsetMultiplier =
                static_cast<int>(std::floor(normalizedOffset / contentSize));
            m_offset = offsetMultiplier > 0
                           ? std::fmod(normalizedOffset,
                                       offsetMultiplier * contentSize)
                           : normalizedOffset;
        }
        virtualize(scroll, children);
    }
    return true;
}

void ScrollVirtualizer::virtualize(ScrollConstraint* scroll,
                                   std::vector<LayoutNodeProvider*>& children)
{
    int totalItemCount = 0;
    for (auto child : children)
    {
        totalItemCount += child->numLayoutNodes();
    }

    // All changes in this function are intended to compare the
    // ranges of the previous render to the ranges of the upcoming list. This is
    // removing the carousel overflow.
    // normalizing the two values to the actual indexes of available children
    int lastVisibleIndexStart = m_infinite && totalItemCount > 0
                                    ? m_visibleIndexStart % totalItemCount
                                    : m_visibleIndexStart;
    int lastVisibleIndexEnd = m_infinite && totalItemCount > 0
                                  ? m_visibleIndexEnd % totalItemCount
                                  : m_visibleIndexEnd;

    m_visibleIndexStart = 0;
    m_visibleIndexEnd = totalItemCount - 1;
    float runningSize = 0.0f;
    float runningOffset = 0.0f;
    int runningIndex = 0;
    int childIndex = 0;
    int currentChildIndex = 0;
    bool isHorz = m_direction == VirtualizedDirection::horizontal;
    float gap = isHorz ? scroll->gap().x : scroll->gap().y;

    for (int i = 0; i < children.size(); i++)
    {
        auto child = children[i];
        for (int j = 0; j < child->numLayoutNodes(); j++)
        {
            auto size = getItemSize(child, j, isHorz);
            if (runningSize + size > m_offset)
            {
                runningOffset = runningSize - m_offset;
                m_visibleIndexStart = runningIndex;
                if (currentChildIndex == children.size() - 1)
                {
                    childIndex++;
                    currentChildIndex = 0;
                }
                else
                {
                    currentChildIndex++;
                }
                goto findVisibleEnd;
            }
            runningSize += size;
            currentChildIndex = j;
            runningIndex++;
            if (runningSize + gap > m_offset)
            {
                if (runningIndex == totalItemCount)
                {
                    runningIndex = 0;
                }
                if (currentChildIndex == children.size() - 1)
                {
                    childIndex++;
                    currentChildIndex = 0;
                }
                else
                {
                    currentChildIndex++;
                }
                runningSize += gap;
                runningOffset = runningSize - m_offset;
                m_visibleIndexStart = runningIndex;
                goto findVisibleEnd;
            }
            runningSize += gap;
        }
        childIndex++;
    }

findVisibleEnd:
    childIndex = childIndex % children.size();
    int i = m_visibleIndexStart;
    bool wrapped = false;
    int cycleCount = 0;
    while (i < totalItemCount && cycleCount < 2)
    {
        auto child = children[childIndex];
        for (int j = currentChildIndex; j < child->numLayoutNodes(); j++)
        {
            auto size = getItemSize(child, j, isHorz);
            if (runningSize + size + gap >= m_offset + m_viewportSize)
            {
                m_visibleIndexEnd =
                    m_infinite ? (wrapped ? i + totalItemCount : i) : i;
                goto recycle;
            }
            runningSize += size + gap;
            runningIndex++;
            if (m_infinite && i == totalItemCount - 1)
            {
                wrapped = true;
                i = -1; // will become 0 after increment
                cycleCount++;
            }
            i++;
        }
        currentChildIndex = 0;
    }

recycle:
    std::vector<int> indicesToRecycle;
    int actualStart = m_infinite && totalItemCount > 0
                          ? m_visibleIndexStart % totalItemCount
                          : m_visibleIndexStart;
    int actualEnd = m_infinite && totalItemCount > 0
                        ? m_visibleIndexEnd % totalItemCount
                        : m_visibleIndexEnd;
    std::unordered_map<int, bool> usedIndexes = {};
    // If start < end it means that the range is not going over
    // the end of the list, so we know we can add the full range to the used
    // items.
    if (actualStart <= actualEnd)
    {
        for (int i = actualStart; i <= actualEnd; i++)
        {
            usedIndexes[i] = true;
        }
    }
    // If end > start, we know that the range wraps, so we
    // actually need to add two ranges, from [start to totalIItems] and from [0
    // to end]
    else
    {
        for (int i = actualStart; i < totalItemCount; i++)
        {
            usedIndexes[i] = true;
        }
        for (int i = 0; i <= actualEnd; i++)
        {
            usedIndexes[i] = true;
        }
    }
    // Similarly, we check the previous ranges and check which
    // ones overlap with the new range and which ones can be recycled.
    if (lastVisibleIndexStart <= lastVisibleIndexEnd)
    {

        for (int i = lastVisibleIndexStart; i <= lastVisibleIndexEnd; i++)
        {
            if (usedIndexes.find(i) == usedIndexes.end())
            {
                indicesToRecycle.push_back(i);
            }
        }
    }
    else
    {

        for (int i = lastVisibleIndexStart; i < totalItemCount; i++)
        {
            if (usedIndexes.find(i) == usedIndexes.end())
            {
                indicesToRecycle.push_back(i);
            }
        }
        for (int i = 0; i <= lastVisibleIndexEnd; i++)
        {
            if (usedIndexes.find(i) == usedIndexes.end())
            {
                indicesToRecycle.push_back(i);
            }
        }
    }
    recycleItems(indicesToRecycle, children, totalItemCount);

    for (int i = m_visibleIndexStart; i <= m_visibleIndexEnd; ++i)
    {
        int actualIndex = m_infinite ? i % totalItemCount : i;
        int runningTotal = 0;
        for (int i = 0; i < children.size(); i++)
        {
            auto child = children[i];
            int start = runningTotal;
            int end = start + (int)child->numLayoutNodes();
            auto component = child->transformComponent();
            if (component != nullptr)
            {
                auto virt = VirtualizingComponent::from(component);
                if (virt != nullptr && start < end)
                {
                    if (actualIndex < end && actualIndex >= start)
                    {
                        int childIndex = actualIndex - start;
                        auto item = virt->item(childIndex);
                        if (item == nullptr)
                        {
                            virt->addVirtualizable(childIndex);
                        }

                        auto size = getItemSize(child, childIndex, isHorz);
                        auto virtualizable = virt->item(childIndex);
                        if (virtualizable != nullptr)
                        {
                            auto virtualizableComponent =
                                virtualizable->virtualizableComponent();
                            if (virtualizableComponent != nullptr &&
                                virtualizableComponent->is<ArtboardInstance>())
                            {
                                auto artboardInstance =
                                    virtualizableComponent
                                        ->as<ArtboardInstance>();
                                auto parentWorld = component->worldTransform();
                                Mat2D inverse;
                                if (!parentWorld.invert(&inverse))
                                {
                                    continue;
                                }
                                auto location =
                                    isHorz ? Vec2D(runningOffset,
                                                   artboardInstance->layoutY())
                                           : Vec2D(artboardInstance->layoutX(),
                                                   runningOffset);
                                auto transform =
                                    inverse * Mat2D::fromTranslation(location);
                                artboardInstance->mutableWorldTransform() =
                                    transform;
                                artboardInstance->markWorldTransformDirty();
                            }
                        }

                        runningOffset += size + gap;
                        break;
                    }
                }
            }
            runningTotal = end;
        }
    }
}

void ScrollVirtualizer::recycleItems(std::vector<int> indices,
                                     std::vector<LayoutNodeProvider*>& children,
                                     int totalItemCount)
{
    if (totalItemCount == 0)
    {
        return;
    }
    std::sort(indices.begin(), indices.end());
    for (auto globalIndex : indices)
    {
        auto actualIndex =
            m_infinite ? globalIndex % totalItemCount : globalIndex;
        int runningTotal = 0;
        for (int i = 0; i < children.size(); i++)
        {
            auto child = children[i];
            int start = runningTotal;
            int end = start + (int)child->numLayoutNodes();
            auto component = child->transformComponent();
            if (component != nullptr)
            {
                auto virt = VirtualizingComponent::from(component);
                if (virt != nullptr && start < end)
                {
                    if (actualIndex < end && actualIndex >= start)
                    {
                        int childIndex = actualIndex - start;
                        virt->removeVirtualizable(childIndex);
                        break;
                    }
                }
            }
            runningTotal = end;
        }
    }
}

float ScrollVirtualizer::getItemSize(LayoutNodeProvider* child,
                                     int index,
                                     bool isHorizontal)
{
    auto component = child->transformComponent();
    if (component != nullptr)
    {
        auto virt = VirtualizingComponent::from(component);
        if (virt != nullptr)
        {
            auto size = virt->itemSize(index);
            return isHorizontal ? size.x : size.y;
        }
    }
    auto bounds = child->layoutBounds();
    return isHorizontal ? bounds.width() : bounds.height();
}
