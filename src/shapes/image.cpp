#include "rive/shapes/image.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/image_asset.hpp"

using namespace rive;

void Image::draw(Renderer* renderer)
{
	if (renderOpacity() == 0.0f)
	{
		return;
	}
	renderer->drawImage(
	    m_ImageAsset->renderImage(), blendMode(), renderOpacity());
}

StatusCode Image::import(ImportStack& importStack)
{
	auto backboardImporter =
	    importStack.latest<BackboardImporter>(Backboard::typeKey);
	if (backboardImporter == nullptr)
	{
		return StatusCode::MissingObject;
	}
	backboardImporter->addFileAssetReferencer(this);

	return Super::import(importStack);
}

void Image::assets(const std::vector<FileAsset*>& assets)
{
	if (assetId() < 0 || assetId() >= assets.size())
	{
		return;
	}
	auto asset = assets[assetId()];
	if (asset->is<ImageAsset>())
	{
		m_ImageAsset = asset->as<ImageAsset>();
	}
}