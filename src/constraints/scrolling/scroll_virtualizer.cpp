#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/constraints/scrolling/scroll_virtualizer.hpp"
#include <algorithm>
#include <set>

using namespace rive;

ScrollVirtualizer::~ScrollVirtualizer() { reset(); }

void ScrollVirtualizer::reset()
{
    m_realizedIndexStart = m_realizedIndexEnd = 0;
}

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
    int lastRealizedIndexStart = m_infinite && totalItemCount > 0
                                     ? m_realizedIndexStart % totalItemCount
                                     : m_realizedIndexStart;
    int lastRealizedIndexEnd = m_infinite && totalItemCount > 0
                                   ? m_realizedIndexEnd % totalItemCount
                                   : m_realizedIndexEnd;

    m_realizedIndexStart = 0;
    m_realizedIndexEnd = totalItemCount - 1;
    float runningSize = 0.0f;
    float runningOffset = 0.0f;
    int runningIndex = 0;
    int childIndex = 0;
    int currentChildIndex = 0;
    bool isHorz = m_direction == VirtualizedDirection::horizontal;
    float gap = isHorz ? scroll->gap().x : scroll->gap().y;
    std::set<VirtualizingComponent*> changedVirtualizingComponents;

    for (int i = 0; i < children.size(); i++)
    {
        auto child = children[i];
        auto component = child->transformComponent();
        if (component != nullptr)
        {
            auto virt = VirtualizingComponent::from(component);
            if (virt != nullptr)
            {
                virt->setVisibleIndices(-1, -1);
                virt->setRealizedIndices(-1, -1);
            }
        }
    }

    for (int i = 0; i < children.size(); i++)
    {
        auto child = children[i];
        for (int j = 0; j < child->numLayoutNodes(); j++)
        {
            auto size = getItemSize(child, j, isHorz);
            if (runningSize + size > m_offset)
            {
                runningOffset = runningSize - m_offset;
                m_realizedIndexStart = runningIndex;
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
                m_realizedIndexStart = runningIndex;
                goto findVisibleEnd;
            }
            runningSize += gap;
        }
        childIndex++;
    }

findVisibleEnd:
    childIndex = childIndex % children.size();
    int i = m_realizedIndexStart;
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
                m_realizedIndexEnd =
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
    // Keep `virtualizeBuffer` lines realized on each side of the visible range
    // so items are mounted and advancing before they scroll in. Buffered items
    // are drawn (clipped away by a normal viewport), but stay out of the
    // visible range, which is what reports measured sizes back to us.
    int visibleIndexStart = m_realizedIndexStart;
    int visibleIndexEnd = m_realizedIndexEnd;
    int buffer =
        std::min(static_cast<int>(scroll->virtualizeBuffer()), totalItemCount);
    if (buffer > 0 && totalItemCount > 0)
    {
        int visibleSpan = m_realizedIndexEnd - m_realizedIndexStart + 1;
        int maxExtra = std::max(0, totalItemCount - visibleSpan);
        int before =
            std::min(buffer, m_infinite ? maxExtra : m_realizedIndexStart);
        int after =
            std::min(buffer,
                     m_infinite ? maxExtra - before
                                : totalItemCount - 1 - m_realizedIndexEnd);
        before = std::max(0, before);
        after = std::max(0, after);
        for (int k = 1; k <= before; k++)
        {
            runningOffset -= getItemSizeAt(m_realizedIndexStart - k,
                                           children,
                                           totalItemCount,
                                           isHorz) +
                             gap;
        }
        m_realizedIndexStart -= before;
        m_realizedIndexEnd += after;
        if (m_infinite)
        {
            // Indices are modular when infinite, so bias the widened range into
            // positive space and keep the visible bounds in the same frame.
            m_realizedIndexStart += totalItemCount;
            m_realizedIndexEnd += totalItemCount;
            visibleIndexStart += totalItemCount;
            visibleIndexEnd += totalItemCount;
        }
    }

    std::vector<int> indicesToRecycle;
    int actualStart = m_infinite && totalItemCount > 0
                          ? m_realizedIndexStart % totalItemCount
                          : m_realizedIndexStart;
    int actualEnd = m_infinite && totalItemCount > 0
                        ? m_realizedIndexEnd % totalItemCount
                        : m_realizedIndexEnd;
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
    if (lastRealizedIndexStart <= lastRealizedIndexEnd)
    {

        for (int i = lastRealizedIndexStart; i <= lastRealizedIndexEnd; i++)
        {
            if (usedIndexes.find(i) == usedIndexes.end())
            {
                indicesToRecycle.push_back(i);
            }
        }
    }
    else
    {

        for (int i = lastRealizedIndexStart; i < totalItemCount; i++)
        {
            if (usedIndexes.find(i) == usedIndexes.end())
            {
                indicesToRecycle.push_back(i);
            }
        }
        for (int i = 0; i <= lastRealizedIndexEnd; i++)
        {
            if (usedIndexes.find(i) == usedIndexes.end())
            {
                indicesToRecycle.push_back(i);
            }
        }
    }
    recycleItems(indicesToRecycle, children, totalItemCount);

    std::vector<Vec2D> visibleIndices(children.size(), Vec2D(-1, -1));
    std::vector<Vec2D> realizedIndices(children.size(), Vec2D(-1, -1));

    for (int i = m_realizedIndexStart; i <= m_realizedIndexEnd; ++i)
    {
        int actualIndex = m_infinite ? i % totalItemCount : i;
        // Buffered items are realized and drawn, but only on screen items
        // report their measured size back.
        bool isVisible = i >= visibleIndexStart && i <= visibleIndexEnd;
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
                        auto& realizedInd = realizedIndices[i];
                        if (realizedInd.x == -1)
                        {
                            realizedInd.x = childIndex;
                        }
                        realizedInd.y = childIndex;
                        if (isVisible)
                        {
                            auto& visibleInd = visibleIndices[i];
                            if (visibleInd.x == -1)
                            {
                                visibleInd.x = childIndex;
                            }
                            visibleInd.y = childIndex;
                        }
                        auto item = virt->item(childIndex);
                        if (item == nullptr)
                        {
                            virt->addVirtualizable(childIndex);
                            changedVirtualizingComponents.emplace(virt);
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
                                virt->setVirtualizablePosition(childIndex,
                                                               location);
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

    for (int i = 0; i < children.size(); i++)
    {
        auto child = children[i];
        auto visible = visibleIndices[i];
        auto component = child->transformComponent();
        if (component != nullptr)
        {
            auto virt = VirtualizingComponent::from(component);
            if (virt != nullptr)
            {
                virt->setVisibleIndices(visible.x, visible.y);
                auto realized = realizedIndices[i];
                virt->setRealizedIndices(realized.x, realized.y);
            }
        }
    }
    for (auto& virtualizingComponent : changedVirtualizingComponents)
    {
        virtualizingComponent->virtualizableChanged();
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

float ScrollVirtualizer::getItemSizeAt(
    int globalIndex,
    std::vector<LayoutNodeProvider*>& children,
    int totalItemCount,
    bool isHorizontal)
{
    if (totalItemCount <= 0)
    {
        return 0.0f;
    }
    int index = globalIndex % totalItemCount;
    if (index < 0)
    {
        index += totalItemCount;
    }
    int runningTotal = 0;
    for (auto child : children)
    {
        int end = runningTotal + (int)child->numLayoutNodes();
        if (index < end)
        {
            return getItemSize(child, index - runningTotal, isHorizontal);
        }
        runningTotal = end;
    }
    return 0.0f;
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
