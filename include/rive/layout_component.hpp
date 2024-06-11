#ifndef _RIVE_LAYOUT_COMPONENT_HPP_
#define _RIVE_LAYOUT_COMPONENT_HPP_
#include "rive/generated/layout_component_base.hpp"
#include "rive/layout/layout_component_style.hpp"
#ifdef WITH_RIVE_LAYOUT
#include "yoga/YGNode.h"
#include "yoga/YGStyle.h"
#include "yoga/Yoga.h"
#endif
#include <stdio.h>
namespace rive
{
#ifndef WITH_RIVE_LAYOUT
class YGNodeRef
{
public:
    YGNodeRef() {}
};
class YGStyle
{
public:
    YGStyle() {}
};
#endif
class LayoutComponent : public LayoutComponentBase
{
private:
    LayoutComponentStyle* m_style = nullptr;
    YGNodeRef m_layoutNode;
    YGStyle* m_layoutStyle;
    float m_layoutSizeWidth = 0;
    float m_layoutSizeHeight = 0;
    float m_layoutLocationX = 0;
    float m_layoutLocationY = 0;

#ifdef WITH_RIVE_LAYOUT
private:
    void syncLayoutChildren();
    void propagateSizeToChildren(ContainerComponent* component);
    AABB findMaxIntrinsicSize(ContainerComponent* component, AABB maxIntrinsicSize);

protected:
    void calculateLayout();
#endif

public:
    LayoutComponentStyle* style() { return m_style; }
    void style(LayoutComponentStyle* style) { m_style = style; }

#ifdef WITH_RIVE_LAYOUT
    YGNodeRef layoutNode() { return m_layoutNode; }

    YGStyle* layoutStyle() { return m_layoutStyle; }

    LayoutComponent()
    {
        m_layoutNode = new YGNode();
        m_layoutStyle = new YGStyle();
    }

    ~LayoutComponent()
    {
        YGNodeFreeRecursive(m_layoutNode);
        delete m_layoutStyle;
    }
    void syncStyle();
    void propagateSize();
    void updateLayoutBounds();
    void update(ComponentDirt value) override;
    StatusCode onAddedDirty(CoreContext* context) override;

#else
    LayoutComponent()
    {
        m_layoutNode = YGNodeRef();
        auto s = new YGStyle();
        m_layoutStyle = s;
        m_layoutSizeWidth = 0;
        m_layoutSizeHeight = 0;
        m_layoutLocationX = 0;
        m_layoutLocationY = 0;
    }
#endif
    void buildDependencies() override;

    void markLayoutNodeDirty();
    void clipChanged() override;
    void widthChanged() override;
    void heightChanged() override;
    void styleIdChanged() override;
};
} // namespace rive

#endif