#include "rive/shapes/image.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/image_asset.hpp"

using namespace rive;

void Image::draw(Renderer* renderer)
{
	if (m_ImageAsset == nullptr || renderOpacity() == 0.0f)
	{
		return;
	}

	if (!clip(renderer))
	{
		// We didn't clip, so make sure to save as we'll be doing some
		// transformations.
		renderer->save();
	}

	auto renderImage = m_ImageAsset->renderImage();
	auto width = renderImage->width();
	auto height = renderImage->height();

	const Mat2D& transform = worldTransform();
	renderer->transform(transform);

	Mat2D originTranslation(
	    1.0f, 0.0f, 0.0f, 1.0f, -width / 2.0f, -height / 2.0f);
	renderer->transform(originTranslation);

	renderer->drawImage(renderImage, blendMode(), renderOpacity());

	renderer->restore();
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

Core* Image::clone() const
{
	Image* twin = ImageBase::clone()->as<Image>();
	twin->m_ImageAsset = m_ImageAsset;
	return twin;
}