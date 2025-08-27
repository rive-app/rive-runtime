#include "rive/math/hit_test.hpp"
#include "rive/shapes/image.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/layout/n_slicer.hpp"
#include "rive/shapes/mesh_drawable.hpp"
#include "rive/artboard.hpp"
#include "rive/clip_result.hpp"

using namespace rive;

void Image::draw(Renderer* renderer)
{
    rive::ImageAsset* asset = this->imageAsset();
    if (asset == nullptr || renderOpacity() == 0.0f)
    {
        return;
    }

    rive::RenderImage* renderImage = asset->renderImage();
    if (renderImage == nullptr)
    {
        return;
    }

    ClipResult clipResult = applyClip(renderer);

    if (clipResult == ClipResult::noClip)
    {
        // We didn't clip, so make sure to save as we'll be doing some
        // transformations.
        renderer->save();
    }

    if (clipResult != ClipResult::emptyClip)
    {
        float width = (float)renderImage->width();
        float height = (float)renderImage->height();

        // until image loading and saving is done, use default sampling for
        // image assets
        if (m_Mesh != nullptr)
        {
            m_Mesh->draw(renderer,
                         renderImage,
                         rive::ImageSampler::LinearClamp(),
                         blendMode(),
                         renderOpacity());
        }
        else
        {
            renderer->transform(worldTransform());
            renderer->translate(-width * originX(), -height * originY());
            renderer->drawImage(renderImage,
                                rive::ImageSampler::LinearClamp(),
                                blendMode(),
                                renderOpacity());
        }
    }

    renderer->restore();
}

Core* Image::hitTest(HitInfo* hinfo, const Mat2D& xform)
{
    // TODO: handle clip?

    auto renderImage = imageAsset()->renderImage();
    float width = (float)renderImage->width();
    float height = (float)renderImage->height();

    if (m_Mesh)
    {
        printf("Missing mesh\n");
        // TODO: hittest mesh
    }
    else
    {
        auto mx = xform * worldTransform() *
                  Mat2D::fromTranslate(-width * originX(), -height * originY());
        HitTester tester(hinfo->area);
        tester.addRect(AABB(0, 0, (float)width, (float)height), mx);
        if (tester.test())
        {
            return this;
        }
    }
    return nullptr;
}

StatusCode Image::import(ImportStack& importStack)
{
    auto result = registerReferencer(importStack);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    return Super::import(importStack);
}

// Question: thoughts on this? it looks a bit odd to me,
// maybe there's a trick i'm missing here .. (could also implement
// getAssetId...)
uint32_t Image::assetId() { return ImageBase::assetId(); }

void Image::setAsset(rcp<FileAsset> asset)
{
    if (asset != nullptr && asset->is<ImageAsset>())
    {
        FileAssetReferencer::setAsset(asset);

        // If we have a mesh and we're in the source artboard, let's initialize
        // the mesh buffers.
        if (m_Mesh != nullptr && !artboard()->isInstance())
        {
            m_Mesh->onAssetLoaded(imageAsset()->renderImage());
        }
        updateImageScale();
    }
}

void Image::assetUpdated()
{
    updateImageScale();
    markWorldTransformDirty();
}

Core* Image::clone() const
{
    Image* twin = ImageBase::clone()->as<Image>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}

void Image::setMesh(MeshDrawable* mesh) { m_Mesh = mesh; }

float Image::width() const
{
    rive::ImageAsset* asset = this->imageAsset();
    if (asset == nullptr)
    {
        return 0.0f;
    }

    rive::RenderImage* renderImage = asset->renderImage();
    if (renderImage == nullptr)
    {
        return 0.0f;
    }
    return (float)renderImage->width();
}

float Image::height() const
{
    rive::ImageAsset* asset = this->imageAsset();
    if (asset == nullptr)
    {
        return 0.0f;
    }

    rive::RenderImage* renderImage = asset->renderImage();
    if (renderImage == nullptr)
    {
        return 0.0f;
    }
    return (float)renderImage->height();
}

Vec2D Image::measureLayout(float width,
                           LayoutMeasureMode widthMode,
                           float height,
                           LayoutMeasureMode heightMode)
{
    float measuredWidth, measuredHeight;
    switch (widthMode)
    {
        case LayoutMeasureMode::atMost:
            measuredWidth = std::max(Image::width(), width);
            break;
        case LayoutMeasureMode::exactly:
            measuredWidth = width;
            break;
        case LayoutMeasureMode::undefined:
            measuredWidth = Image::width();
            break;
    }
    switch (heightMode)
    {
        case LayoutMeasureMode::atMost:
            measuredHeight = std::max(Image::height(), height);
            break;
        case LayoutMeasureMode::exactly:
            measuredHeight = height;
            break;
        case LayoutMeasureMode::undefined:
            measuredHeight = Image::height();
            break;
    }
    return Vec2D(measuredWidth, measuredHeight);
}

void Image::controlSize(Vec2D size,
                        LayoutScaleType widthScaleType,
                        LayoutScaleType heightScaleType,
                        LayoutDirection direction)
{
    // We store layout width/height because the image asset may not be available
    // yet (referenced images) and we have defer controlling its size
    if (m_layoutWidth != size.x || m_layoutHeight != size.y)
    {
        m_layoutWidth = size.x;
        m_layoutHeight = size.y;

        updateImageScale();
    }
}

void Image::updateImageScale()
{
    // User-created meshes are not affected by scale
    if ((m_Mesh != nullptr && m_Mesh->type() == MeshType::vertex) ||
        imageAsset() == nullptr)
    {
        return;
    }
    auto renderImage = imageAsset()->renderImage();
    if (renderImage != nullptr && !std::isnan(m_layoutWidth) &&
        !std::isnan(m_layoutHeight))
    {
        float newScaleX = m_layoutWidth / (float)renderImage->width();
        float newScaleY = m_layoutHeight / (float)renderImage->height();
        if (newScaleX != scaleX() || newScaleY != scaleY())
        {
            scaleX(newScaleX);
            scaleY(newScaleY);
            addDirt(ComponentDirt::WorldTransform, false);
        }
    }
}

ImageAsset* Image::imageAsset() const { return (ImageAsset*)m_fileAsset.get(); }

#ifdef TESTING
#include "rive/shapes/mesh.hpp"
Mesh* Image::mesh() const { return static_cast<Mesh*>(m_Mesh); };
#endif
