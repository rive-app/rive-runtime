#include "rive/nested_artboard_layout.hpp"
#include "rive/artboard.hpp"

using namespace rive;

Core* NestedArtboardLayout::clone() const
{
    NestedArtboardLayout* nestedArtboard =
        static_cast<NestedArtboardLayout*>(NestedArtboardLayoutBase::clone());
    if (m_Artboard == nullptr)
    {
        return nestedArtboard;
    }
    auto ni = m_Artboard->instance();
    nestedArtboard->nest(ni.release());
    return nestedArtboard;
}

#ifdef WITH_RIVE_LAYOUT
void* NestedArtboardLayout::layoutNode()
{
    if (artboardInstance() == nullptr)
    {
        return nullptr;
    }
    return artboardInstance()->takeLayoutNode();
}
#endif

void NestedArtboardLayout::markNestedLayoutDirty()
{
    if (artboardInstance() != nullptr)
    {
        artboardInstance()->markLayoutNodeDirty();
    }
}

void NestedArtboardLayout::update(ComponentDirt value)
{
    Super::update(value);
    auto artboard = artboardInstance();
    if (hasDirt(value, ComponentDirt::WorldTransform) && artboard != nullptr)
    {
        auto layoutPosition = Vec2D(artboard->layoutX(), artboard->layoutY());

        if (parent()->is<Artboard>())
        {
            auto parentArtboard = parent()->as<Artboard>();
            auto correctedArtboardSpace =
                Mat2D::fromTranslation(parentArtboard->origin() + layoutPosition);
            m_WorldTransform = correctedArtboardSpace * m_WorldTransform;
        }
        else
        {
            m_WorldTransform = Mat2D::fromTranslation(layoutPosition) * m_WorldTransform;
        }
        auto back = Mat2D::fromTranslation(-artboard->origin());
        m_WorldTransform = back * m_WorldTransform;
    }
}