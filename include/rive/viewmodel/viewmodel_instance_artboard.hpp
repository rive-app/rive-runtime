#ifndef _RIVE_VIEW_MODEL_INSTANCE_ARTBOARD_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ARTBOARD_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_artboard_base.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceArtboard;
typedef void (*ViewModelArtboardChanged)(ViewModelInstanceArtboard* vmi,
                                         uint32_t value);
#endif
class ViewModelInstanceArtboard : public ViewModelInstanceArtboardBase
{
protected:
    void propertyValueChanged() override;

public:
    void asset(Artboard* value);
    Artboard* asset() { return m_artboard; }

private:
    Artboard* m_artboard = nullptr;
#ifdef WITH_RIVE_TOOLS
public:
    void onChanged(ViewModelArtboardChanged callback)
    {
        m_changedCallback = callback;
    }
    ViewModelArtboardChanged m_changedCallback = nullptr;
#endif
};
} // namespace rive

#endif