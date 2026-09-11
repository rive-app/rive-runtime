#include "rive/shapes/paint/paint_image.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/renderer.hpp"
#include "rive/core_context.hpp"

using namespace rive;

StatusCode PaintImage::import(ImportStack& importStack)
{
    // The image is optional, so registration is best-effort: unlike the Image
    // node we must not fail the import when there's no backboard importer. The
    // backboard importer safely skips out-of-range indices during resolve.
    registerReferencer(importStack);
    return Super::import(importStack);
}

bool PaintImage::validate(CoreContext* context)
{
    if (!Super::validate(context))
    {
        return false;
    }
    auto coreObject = context->resolve(parentId());
    // we know it's not nullptr from Super::validate
    assert(coreObject != nullptr);
    return coreObject->is<ShapePaint>();
}

Core* PaintImage::clone() const
{
    PaintImage* twin = PaintImageBase::clone()->as<PaintImage>();
    // The generated clone copies the imageAssetId property but not the resolved
    // asset, so carry it over (mirrors Image::clone). Without this a nested
    // artboard instance loses its paint image.
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}

uint32_t PaintImage::assetId() { return imageAssetId(); }

void PaintImage::invalidatePaint()
{
    if (parent() != nullptr && parent()->is<ShapePaint>())
    {
        parent()->as<ShapePaint>()->invalidateRendering();
    }
}

void PaintImage::setAsset(rcp<FileAsset> asset)
{
    if (asset != nullptr && asset->is<ImageAsset>())
    {
        FileAssetReferencer::setAsset(asset);
        invalidatePaint();
    }
}

// The texture usually lands after setAsset -- decoding is async, and in the
// editor the bytes arrive from the CDN later still. ImageAsset publishes it by
// calling assetUpdated() on its referencers, so re-invalidate here; otherwise a
// paint that recorded "no image" on its first draw keeps that cleared state
// until some unrelated redraw happens to dirty it.
void PaintImage::assetUpdated() { invalidatePaint(); }

ImageAsset* PaintImage::imageAsset() const
{
    return static_cast<ImageAsset*>(m_fileAsset.get());
}

ImageSampler PaintImage::imageSampler() const
{
    // Clamp file values so the key stays inside the backends' sampler tables.
    auto filterValue = [](uint32_t value) {
        return value <= (uint32_t)ImageFilter::nearest
                   ? static_cast<ImageFilter>(value)
                   : ImageFilter::bilinear;
    };
    auto wrapValue = [](uint32_t value) {
        return value <= (uint32_t)ImageWrap::mirror
                   ? static_cast<ImageWrap>(value)
                   : ImageWrap::clamp;
    };
    ImageSampler sampler = ImageSampler::LinearClamp();
    if (ImageAsset* asset = imageAsset())
    {
        sampler.filter = filterValue(asset->samplerFilter());
        sampler.wrapX = wrapValue(asset->samplerWrapX());
        sampler.wrapY = wrapValue(asset->samplerWrapY());
    }
    // Paint values are offset by one, zero means inherit from the asset.
    if (imageSamplerFilter() != 0)
    {
        sampler.filter = filterValue(imageSamplerFilter() - 1);
    }
    if (imageSamplerWrapX() != 0)
    {
        sampler.wrapX = wrapValue(imageSamplerWrapX() - 1);
    }
    if (imageSamplerWrapY() != 0)
    {
        sampler.wrapY = wrapValue(imageSamplerWrapY() - 1);
    }
    return sampler;
}

bool PaintImage::applyTo(RenderPaint* paint, const AABB& bounds)
{
    ImageAsset* asset = imageAsset();
    RenderImage* renderImage =
        asset != nullptr ? asset->renderImage() : nullptr;
    if (renderImage == nullptr)
    {
        // The asset was cleared (or replaced by one that hasn't decoded yet).
        // The caller clears the stale texture off the (persistent) paint.
        return false;
    }
    // Maps the image's normalized texture space (0..1) into the path's
    // coordinate space. The base fits the image across the bounds once; the
    // user transform (scale/offset/rotation about the bounds center) modifies
    // it. At defaults it reproduces the plain fit; with scale < 1 and
    // wrap=repeat/mirror it tiles. Base tile size: shape bounds (fill) or the
    // image's native pixel size (original). 0 = fill, 1 = original.
    bool original = imageSizeMode() == 1;
    float baseW = original ? (float)renderImage->width() : bounds.width();
    float baseH = original ? (float)renderImage->height() : bounds.height();
    float cx = bounds.left() + bounds.width() * 0.5f;
    float cy = bounds.top() + bounds.height() * 0.5f;
    Mat2D imageTransform =
        Mat2D::fromTranslate(cx, cy) * Mat2D::fromRotation(imageRotation()) *
        Mat2D::fromTranslate(imageOffsetX() * baseW, imageOffsetY() * baseH) *
        Mat2D::fromScale(baseW * imageScaleX(), baseH * imageScaleY()) *
        Mat2D::fromTranslate(-0.5f, -0.5f);
    paint->modulatedImage(renderImage, imageSampler(), imageTransform);
    return true;
}
