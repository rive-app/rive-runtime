#include "rive/assets/file_asset.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"

using namespace rive;

StatusCode FileAsset::import(ImportStack& importStack)
{
	auto backboardImporter =
	    importStack.latest<BackboardImporter>(Backboard::typeKey);
	if (backboardImporter == nullptr)
	{
		return StatusCode::MissingObject;
	}
	backboardImporter->addFileAsset(this);

	return Super::import(importStack);
}