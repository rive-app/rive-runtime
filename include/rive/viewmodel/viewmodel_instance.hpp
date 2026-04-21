#ifndef _RIVE_VIEW_MODEL_INSTANCE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_HPP_
#include "rive/generated/viewmodel/viewmodel_instance_base.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/symbol_type.hpp"
#include "rive/data_bind/data_bind_container.hpp"
#include "rive/component.hpp"
#include "rive/refcnt.hpp"
#include <cstdint>
#include <stdio.h>
#include <unordered_map>
#include <vector>
namespace rive
{
class ViewModel;
class ViewModelInstanceViewModel;
class ViewModelInstance : public ViewModelInstanceBase,
                          public RefCnt<ViewModelInstance>
{
private:
    std::vector<rcp<ViewModelInstanceValue>> m_PropertyValues;
    std::vector<ViewModelInstance*> m_parents;
    std::vector<DataBindContainer*> m_dependents;
    std::unordered_map<SymbolType, ViewModelInstanceValue*> m_propertySymbols;
    ViewModel* m_ViewModel;
    void rebindDependents();
    void rebindProperties();

public:
    static uint32_t pointerKey(const ViewModelInstance* instance)
    {
        if (instance == nullptr)
        {
            return -1;
        }
        auto ptr = reinterpret_cast<uint64_t>(instance);
        return static_cast<uint32_t>(ptr ^ (ptr >> 32));
    }

    ~ViewModelInstance();
    void addValue(ViewModelInstanceValue* value);
    ViewModelInstanceValue* propertyValue(const uint32_t id);
    ViewModelInstanceValue* propertyValue(const std::string& name);
    ViewModelInstanceValue* propertyValue(const SymbolType symbolType);
    void propertyValue(const SymbolType symbolType,
                       ViewModelInstanceValue* value);
    bool replaceViewModelByName(const std::string& name,
                                rcp<ViewModelInstance> value);
    bool replaceViewModelByProperty(ViewModelInstanceViewModel*,
                                    rcp<ViewModelInstance> value);
    const std::vector<rcp<ViewModelInstanceValue>>& propertyValues();
    ViewModelInstanceValue* propertyFromPath(std::vector<uint32_t>* path,
                                             size_t index);
    ViewModelInstanceValue* symbol(int coreType);
    void viewModel(ViewModel* value);
    ViewModel* viewModel() const;
    void onComponentDirty(Component* component);
    void setAsRoot(rcp<ViewModelInstance> instance);
    void setRoot(rcp<ViewModelInstance> value);
    Core* clone() const override;
    StatusCode import(ImportStack& importStack) override;
    void advanced();
    void addParent(ViewModelInstance*);
    void removeParent(ViewModelInstance*);
    void addDependent(DataBindContainer*);
    void removeDependent(DataBindContainer*);
#ifdef TESTING
    std::vector<DataBindContainer*> dependents() { return m_dependents; }
    std::vector<ViewModelInstance*> parents() { return m_parents; }
#endif
};
} // namespace rive

#endif