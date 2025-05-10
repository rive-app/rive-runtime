#ifndef _RIVE_VIEW_MODEL_INSTANCE_SYMBOL_LIST_INDEX_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_SYMBOL_LIST_INDEX_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_symbol_list_index_base.hpp"
#include <stdio.h>
namespace rive
{
#ifdef WITH_RIVE_TOOLS
class ViewModelInstanceSymbolListIndex;
typedef void (*ViewModelSymbolListIndexChanged)(
    ViewModelInstanceSymbolListIndex* vmi,
    uint32_t value);
#endif
class ViewModelInstanceSymbolListIndex
    : public ViewModelInstanceSymbolListIndexBase
{
protected:
    void propertyValueChanged() override;
#ifdef WITH_RIVE_TOOLS
public:
    void onChanged(ViewModelSymbolListIndexChanged callback)
    {
        m_changedCallback = callback;
    }
    ViewModelSymbolListIndexChanged m_changedCallback = nullptr;
#endif
};
} // namespace rive

#endif