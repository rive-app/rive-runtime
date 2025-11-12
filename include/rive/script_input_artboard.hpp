#ifndef _RIVE_SCRIPT_INPUT_ARTBOARD_HPP_
#define _RIVE_SCRIPT_INPUT_ARTBOARD_HPP_
#include "rive/generated/script_input_artboard_base.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <stdio.h>
namespace rive
{
class Artboard;

class ScriptInputArtboard : public ScriptInputArtboardBase, public ScriptInput
{
private:
    Artboard* m_artboard;

public:
    void artboard(Artboard* artboard) { m_artboard = artboard; }
    void initScriptedValue() override
    {
        ScriptInput::initScriptedValue();
        if (m_artboard == nullptr)
        {
            return;
        }
        auto obj = scriptedObject();
        if (obj)
        {
            obj->setArtboardInput(name(), m_artboard);
        }
    }
    bool validateForScriptInit() override { return m_artboard != nullptr; }
    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
};
} // namespace rive

#endif