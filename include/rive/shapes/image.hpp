#ifndef _RIVE_IMAGE_HPP_
#define _RIVE_IMAGE_HPP_

#include "rive/hit_info.hpp"
#include "rive/generated/shapes/image_base.hpp"
#include "rive/assets/file_asset_referencer.hpp"

namespace rive
{
class ImageAsset;
class MeshDrawable;
#ifdef TESTING
class Mesh;
#endif
class Image : public ImageBase, public FileAssetReferencer
{
private:
    MeshDrawable* m_Mesh = nullptr;
    // Since layouts only pass down width/height we store those
    // and use the image width/height to compute the proper scale
    float m_layoutWidth = NAN;
    float m_layoutHeight = NAN;
    void updateImageScale();

public:
    void setMesh(MeshDrawable* mesh);
    ImageAsset* imageAsset() const;
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    StatusCode import(ImportStack& importStack) override;
    void setAsset(rcp<FileAsset>) override;
    uint32_t assetId() override;
    Core* clone() const override;
    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size,
                     LayoutScaleType widthScaleType,
                     LayoutScaleType heightScaleType,
                     LayoutDirection direction) override;
    float width() const;
    float height() const;
    void assetUpdated() override;
    AABB localBounds() const override;

#ifdef TESTING
    Mesh* mesh() const;
#endif
};
} // namespace rive

#endif
