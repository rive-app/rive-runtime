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

std::string FileAsset::uniqueFilename()
{
	// remove final extension
	std::string uniqueFilename = name();
	std::size_t finalDot = uniqueFilename.rfind('.');

	if (finalDot != std::string::npos)
	{
		uniqueFilename = uniqueFilename.substr(0, finalDot);
	}
	return uniqueFilename + "-" + std::to_string(assetId()) + "." +
	       fileExtension();
}
