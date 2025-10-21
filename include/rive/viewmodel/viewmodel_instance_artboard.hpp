#ifndef _RIVE_VIEW_MODEL_INSTANCE_ARTBOARD_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ARTBOARD_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_artboard_base.hpp"
#include "rive/data_bind/data_values/data_value_integer.hpp"
#include "rive/bindable_artboard.hpp"
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
    void asset(rcp<BindableArtboard> value);
    rcp<BindableArtboard> asset() { return m_bindableArtboard; }
    void applyValue(DataValueInteger*);

private:
    rcp<BindableArtboard> m_bindableArtboard = nullptr;
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