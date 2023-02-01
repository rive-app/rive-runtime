#include "rive/assets/file_asset_referencer.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"

using namespace rive;

StatusCode FileAssetReferencer::registerReferencer(ImportStack& importStack)
{
    auto backboardImporter = importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    backboardImporter->addFileAssetReferencer(this);

    return StatusCode::Ok;
}