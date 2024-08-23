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

public:
    void setMesh(MeshDrawable* mesh);
    ImageAsset* imageAsset() const { return (ImageAsset*)m_fileAsset; }
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    StatusCode import(ImportStack& importStack) override;
    void setAsset(FileAsset*) override;
    uint32_t assetId() override;
    Core* clone() const override;
    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size) override;
    float width() const;
    float height() const;

#ifdef TESTING
    Mesh* mesh() const;
#endif
};
} // namespace rive

#endif
