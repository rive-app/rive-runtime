#ifndef _RIVE_SCRIPT_INPUT_ARTBOARD_HPP_
#define _RIVE_SCRIPT_INPUT_ARTBOARD_HPP_
#include "rive/artboard_referencer.hpp"
#include "rive/generated/script_input_artboard_base.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include <stdio.h>
namespace rive
{
class Artboard;

class ScriptInputArtboard : public ScriptInputArtboardBase,
                            public ScriptInput,
                            public ArtboardReferencer
{
private:
    File* m_file = nullptr;
    void syncReferencedArtboard();

public:
    ~ScriptInputArtboard();
    void initScriptedValue() override;
    bool validateForScriptInit() override
    {
        return m_referencedArtboard != nullptr;
    }
    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
    StatusCode onAddedClean(CoreContext* context) override;
    void file(File* value) { m_file = value; };
    void artboardIdChanged() override;
    void updateArtboard(
        ViewModelInstanceArtboard* viewModelInstanceArtboard) override;
    int referencedArtboardId() override;
};
} // namespace rive

#endif