#ifndef _RIVE_IMAGE_HPP_
#define _RIVE_IMAGE_HPP_

#include "rive/hit_info.hpp"
#include "rive/generated/shapes/image_base.hpp"
#include "rive/assets/file_asset_referencer.hpp"

namespace rive
{
class ImageAsset;
class Mesh;
class Image : public ImageBase, public FileAssetReferencer
{
private:
    Mesh* m_Mesh = nullptr;

public:
    Mesh* mesh() const;
    void setMesh(Mesh* mesh);
    ImageAsset* imageAsset() const { return (ImageAsset*)m_fileAsset; }
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    StatusCode import(ImportStack& importStack) override;
    void setAsset(FileAsset*) override;
    uint32_t assetId() override;
    Core* clone() const override;
};
} // namespace rive

#endif
