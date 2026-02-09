#ifndef _RIVE_SCRIPTED_LAYOUT_HPP_
#define _RIVE_SCRIPTED_LAYOUT_HPP_
#include "rive/layout/layout_enums.hpp"
#include "rive/layout/layout_measure_mode.hpp"
#include "rive/generated/scripted/scripted_layout_base.hpp"
#include <stdio.h>
namespace rive
{
class ScriptedLayout : public ScriptedLayoutBase
{
private:
    Vec2D m_size;
#ifdef WITH_RIVE_SCRIPTING
    void callScriptedResize(Vec2D size);
#endif

public:
#ifdef WITH_RIVE_SCRIPTING
    bool scriptInit(ScriptingVM* vm) override;
#endif
    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;

    void controlSize(Vec2D size,
                     LayoutScaleType widthScaleType,
                     LayoutScaleType heightScaleType,
                     LayoutDirection direction) override;
    void addProperty(CustomProperty* prop) override;
    Core* clone() const override;
    ScriptProtocol scriptProtocol() override { return ScriptProtocol::layout; }
};
} // namespace rive

#endif