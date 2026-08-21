#include "rive/component_dirt.hpp"
#include "rive/layout_component.hpp"
#include "rive/scripted/scripted_layout.hpp"

using namespace rive;

#ifdef WITH_RIVE_SCRIPTING

void ScriptedLayout::didHydrateScriptInputs()
{
    ScriptedDrawable::didHydrateScriptInputs();
    if (parent() != nullptr && parent()->is<LayoutComponent>())
    {
        parent()->as<LayoutComponent>()->markLayoutNodeDirty(true);
    }
}

void ScriptedLayout::callScriptedResize(Vec2D size)
{
    if (!resizes() || m_vm == nullptr || !m_vm->valid())
    {
        return;
    }
    m_vm->callLayoutResize(this, m_self, size);
}

Vec2D ScriptedLayout::measureLayout(float width,
                                    LayoutMeasureMode widthMode,
                                    float height,
                                    LayoutMeasureMode heightMode)
{
    if (!measures() || m_vm == nullptr || !m_vm->valid())
    {
        return Vec2D(0, 0);
    }
    Vec2D measured(std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max());
    if (!m_vm->callLayoutMeasure(this, m_self, &measured))
    {
        // Assumed for legacy files but not implemented; report no measurement
        // (same as !measures()). Errors keep the unbounded default so the
        // clamp below still applies.
        return Vec2D(0, 0);
    }
    return Vec2D(std::min((widthMode == LayoutMeasureMode::undefined
                               ? std::numeric_limits<float>::max()
                               : width),
                          measured.x),
                 std::min((heightMode == LayoutMeasureMode::undefined
                               ? std::numeric_limits<float>::max()
                               : height),
                          measured.y));
}

void ScriptedLayout::controlSize(Vec2D size,
                                 LayoutScaleType widthScaleType,
                                 LayoutScaleType heightScaleType,
                                 LayoutDirection direction)
{
    m_size = size;
    callScriptedResize(size);
}
#else
Vec2D ScriptedLayout::measureLayout(float width,
                                    LayoutMeasureMode widthMode,
                                    float height,
                                    LayoutMeasureMode heightMode)
{
    return Vec2D((widthMode == LayoutMeasureMode::undefined
                      ? std::numeric_limits<float>::max()
                      : width),
                 (heightMode == LayoutMeasureMode::undefined
                      ? std::numeric_limits<float>::max()
                      : height));
}

void ScriptedLayout::controlSize(Vec2D size,
                                 LayoutScaleType widthScaleType,
                                 LayoutScaleType heightScaleType,
                                 LayoutDirection direction)
{
    m_size = size;
}
#endif

void ScriptedLayout::addProperty(CustomProperty* prop)
{
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
        scriptInput->scriptedObject(this);
    }
    CustomPropertyContainer::addProperty(prop);
}

Core* ScriptedLayout::clone() const
{
    ScriptedLayout* twin = ScriptedLayoutBase::clone()->as<ScriptedLayout>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}