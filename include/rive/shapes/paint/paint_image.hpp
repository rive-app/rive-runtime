#ifndef _RIVE_PAINT_IMAGE_HPP_
#define _RIVE_PAINT_IMAGE_HPP_
#include "rive/generated/shapes/paint/paint_image_base.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/math/aabb.hpp"

namespace rive
{
class RenderPaint;
class ImageAsset;

/// Optional child of a ShapePaint that modulates (multiplies) the parent
/// paint's color/gradient with an image when painting the path. Discovered by
/// the parent via ContainerComponent::firstChild<PaintImage>() (no back-pointer
/// is stored on the ShapePaint).
class PaintImage : public PaintImageBase, public FileAssetReferencer
{
public:
    StatusCode import(ImportStack& importStack) override;
    bool validate(CoreContext* context) override;
    Core* clone() const override;

    // FileAssetReferencer: the image used to modulate the parent paint.
    void setAsset(rcp<FileAsset>) override;
    uint32_t assetId() override;
    void assetUpdated() override;
    ImageAsset* imageAsset() const;
    ImageSampler imageSampler() const;

    /// Modulate [paint] with this image, auto-fit to [bounds] (the shape's
    /// local bounds) and adjusted by the image transform (scale/offset/rotation
    /// about the bounds center). Returns false (leaving [paint] untouched) when
    /// no image is resolved -- the caller owns clearing any stale image, as it
    /// also has to handle this component being removed entirely.
    bool applyTo(RenderPaint* paint, const AABB& bounds);

private:
    void invalidatePaint();
};
} // namespace rive

#endif
