#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_asset.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/backboard.hpp"
#include "rive/component_dirt.hpp"
#include "rive/refcnt.hpp"

using namespace rive;

StatusCode ViewModelInstanceAsset::import(ImportStack& importStack)
{
    auto backboardImporter =
        importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    auto assets = backboardImporter->assets();
    for (const auto& asset : *assets)
    {
        m_assets.push_back(asset);
    }
    return Super::import(importStack);
}