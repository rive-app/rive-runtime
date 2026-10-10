#ifndef _RIVE_IMAGE_HPP_
#define _RIVE_IMAGE_HPP_

#include "rive/hit_info.hpp"
#include "rive/generated/shapes/image_base.hpp"
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/shapes/mesh_host.hpp"

namespace rive
{
class LayoutNodeStyle;
class LayoutParticipant;

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
class Image : public ImageBase, public FileAssetReferencer, public MeshHost
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
    void setMesh(MeshDrawable* mesh) override;
    MeshDrawable* hostedMesh() const override { return m_Mesh; }
    float meshHostWidth() const override { return width(); }
    float meshHostHeight() const override { return height(); }
    Vec2D meshHostOrigin() const override
    {
        return Vec2D(originX(), originY());
    }
    float meshHostScaleX() const override { return renderScaleX(); }
    float meshHostScaleY() const override { return renderScaleY(); }
    Mat2D meshHostWorldTransform() const override { return worldTransform(); }
    bool meshHostReady() const override { return imageAsset() != nullptr; }
    Mat2D meshHostUVTransform() const override;
#ifdef WITH_RIVE_EDITOR
    /// Mesh / NSlicer call this from `editorParentChanged(_, this)`
    /// to register on transition to. Both share `m_Mesh` — the
    /// runtime invariant is one Mesh OR one NSlicer per Image, so
    /// the second one overrides.
    void setMeshForEditor(MeshDrawable* mesh) override { m_Mesh = mesh; }
    void clearMeshIfForEditor(MeshDrawable* expected) override
    {
        if (m_Mesh == expected)
        {
            m_Mesh = nullptr;
        }
    }
#endif
    ImageAsset* imageAsset() const;
    ImageSampler imageSampler() const;
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

    // Participation via an optional LayoutParticipant child. Origin
    // is 0 (unlike Text): the image composes its origin + fit into the render
    // separately, so the slot base is just the slot top-left.
    void composeWorldTransform() override;

protected:
    void updateConstraints() override;

public:
    Vec2D layoutBaseTranslation(LayoutParticipant* participant) const;
    LayoutParticipant* layoutParticipant() const;
    bool isParticipatingInLayout() const;

    float width() const;
    float height() const;
    // Effective render scale: the user-facing scaleX/scaleY composed with the
    // layout fit scale. Equals the user scale when not in a layout.
    float renderScaleX() const { return scaleX() * m_layoutScaleX; }
    float renderScaleY() const { return scaleY() * m_layoutScaleY; }
    float computedWidth() override { return width() * renderScaleX(); }
    float computedHeight() override { return height() * renderScaleY(); }
    void assetUpdated() override;
    BoundsFidelity paintedWorldBounds(AABB* out) override;
    AABB localBounds() const override;
    void updateTransform() override;

#ifdef TESTING
    Mesh* mesh() const;
#endif
};
} // namespace rive

#endif
