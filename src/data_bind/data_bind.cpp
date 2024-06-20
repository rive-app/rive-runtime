#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_mode.hpp"
#include "rive/artboard.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

// StatusCode DataBind::onAddedClean(CoreContext* context)
// {
//     return Super::onAddedClean(context);
// }

StatusCode DataBind::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    auto coreObject = context->resolve(targetId());
    if (coreObject == nullptr || !coreObject->is<Component>())
    {
        return StatusCode::MissingObject;
    }

    m_target = static_cast<Component*>(coreObject);

    return StatusCode::Ok;
}

StatusCode DataBind::import(ImportStack& importStack) { return Super::import(importStack); }

void DataBind::buildDependencies()
{
    Super::buildDependencies();
    auto mode = static_cast<DataBindMode>(modeValue());
    if (mode == DataBindMode::oneWayToSource || mode == DataBindMode::twoWay)
    {
        m_target->addDependent(this);
    }
}

void DataBind::updateSourceBinding() {}