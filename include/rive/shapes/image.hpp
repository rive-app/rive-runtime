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
    ImageAsset* m_ImageAsset = nullptr;
    Mesh* m_Mesh = nullptr;

public:
    Mesh* mesh() const;
    void setMesh(Mesh* mesh);
    ImageAsset* imageAsset() const { return m_ImageAsset; }
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    StatusCode import(ImportStack& importStack) override;
    void assets(const std::vector<FileAsset*>& assets) override;
    Core* clone() const override;
};
} // namespace rive

#endif
