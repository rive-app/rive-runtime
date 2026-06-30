#ifndef _RIVE_IMAGE_HPP_
#define _RIVE_IMAGE_HPP_

#include "rive/hit_info.hpp"
#include "rive/generated/shapes/image_base.hpp"
#include "rive/assets/file_asset_referencer.hpp"

namespace rive
{

enum class ImageFit : unsigned char
{
    resize,
    contain,
    cover,
    fitWidth,
    fitHeight,
    none,
    scaleDown,
    fill,
};

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
    float m_layoutOffsetX = 0.0f;
    float m_layoutOffsetY = 0.0f;
    // The layout-driven fit scale is stored separately (rather than written
    // into scaleX/scaleY) so the user-facing scale stays free to be
    // edited/animated. It is composed with the local transform in
    // updateTransform().
    float m_layoutScaleX = 1.0f;
    float m_layoutScaleY = 1.0f;
    // Whether the file applies the layout fit as a separate scale (7.2+).
    // Legacy files overwrite scaleX/scaleY with the fit instead. Set from the
    // file version at import; defaults to the modern behavior for
    // runtime-created images.
    bool m_layoutScaleSeparate = true;
    void updateImageScale();

public:
    void setMesh(MeshDrawable* mesh);
    ImageAsset* imageAsset() const;
    void draw(Renderer* renderer) override;
    bool willDraw() override;
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
    // Effective render scale: the user-facing scaleX/scaleY composed with the
    // layout fit scale. Equals the user scale when not in a layout.
    float renderScaleX() const { return scaleX() * m_layoutScaleX; }
    float renderScaleY() const { return scaleY() * m_layoutScaleY; }
    void assetUpdated() override;
    AABB localBounds() const override;
    void updateTransform() override;

#ifdef TESTING
    Mesh* mesh() const;
#endif
};
} // namespace rive

#endif
