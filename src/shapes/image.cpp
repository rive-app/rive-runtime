#include "rive/math/hit_test.hpp"
#include "rive/shapes/image.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/layout.hpp"
#include "rive/layout/n_slicer.hpp"
#include "rive/shapes/mesh_drawable.hpp"
#include "rive/artboard.hpp"
#include "rive/clip_result.hpp"

using namespace rive;

void Image::draw(Renderer* renderer)
{

    rive::ImageAsset* asset = this->imageAsset();

    rive::RenderImage* renderImage = asset->renderImage();
    if (renderImage == nullptr)
    {
        return;
    }
    if (m_needsSaveOperation)
    {

        renderer->save();
    }

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
    if (m_needsSaveOperation)
    {
        renderer->restore();
    }
}

bool Image::willDraw()
{
    return Super::willDraw() && renderOpacity() != 0.0f &&
           this->imageAsset() != nullptr;
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

void Image::setMesh(MeshDrawable* mesh)
{
    if (m_Mesh == mesh)
    {
        return;
    }
    m_Mesh = mesh;
    updateImageScale();
}

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
        return asset->width();
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
        return asset->height();
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

void Image::updateTransform()
{
    Super::updateTransform();
    m_Transform[4] += m_layoutOffsetX;
    m_Transform[5] += m_layoutOffsetY;
}

void Image::updateImageScale()
{
    if (imageAsset() == nullptr)
    {
        if (m_layoutOffsetX != 0.0f || m_layoutOffsetY != 0.0f)
        {
            m_layoutOffsetX = 0.0f;
            m_layoutOffsetY = 0.0f;
            markTransformDirty();
        }
        return;
    }

    float newOffsetX = 0.0f;
    float newOffsetY = 0.0f;
    auto renderImage = imageAsset()->renderImage();
    if (renderImage != nullptr && !std::isnan(m_layoutWidth) &&
        !std::isnan(m_layoutHeight))
    {
        float imgW = (float)renderImage->width();
        float imgH = (float)renderImage->height();
        float newScaleX, newScaleY;
        auto imageFit = static_cast<Fit>(fit());
        switch (imageFit)
        {
            case Fit::fill:
                newScaleX = m_layoutWidth / imgW;
                newScaleY = m_layoutHeight / imgH;
                break;
            case Fit::contain:
            {
                float s =
                    std::fmin(m_layoutWidth / imgW, m_layoutHeight / imgH);
                newScaleX = newScaleY = s;
                break;
            }
            case Fit::cover:
            {
                float s =
                    std::fmax(m_layoutWidth / imgW, m_layoutHeight / imgH);
                newScaleX = newScaleY = s;
                break;
            }
            case Fit::fitWidth:
                newScaleX = newScaleY = m_layoutWidth / imgW;
                break;
            case Fit::fitHeight:
                newScaleX = newScaleY = m_layoutHeight / imgH;
                break;
            case Fit::none:
                newScaleX = newScaleY = 1.0f;
                break;
            case Fit::scaleDown:
            {
                float s =
                    std::fmin(m_layoutWidth / imgW, m_layoutHeight / imgH);
                s = s < 1.0f ? s : 1.0f;
                newScaleX = newScaleY = s;
                break;
            }
            case Fit::layout:
            default:
                newScaleX = m_layoutWidth / imgW;
                newScaleY = m_layoutHeight / imgH;
                break;
        }

        // Compatibility: legacy files assume fill does not apply fit/alignment
        // translation offsets, only scale.
        if (imageFit != Fit::fill)
        {
            float boundsW = imgW;
            float boundsH = imgH;
            float boundsLeft = -imgW * originX();
            float boundsTop = -imgH * originY();
            if (m_Mesh != nullptr && m_Mesh->type() == MeshType::vertex)
            {
                // Keep fit behavior stable while editing vertex meshes.
                boundsLeft = -imgW * 0.5f;
                boundsTop = -imgH * 0.5f;
            }
            Alignment alignment(alignmentX(), alignmentY());
            float xAlign = (alignment.x() + 1.0f) * 0.5f;
            float yAlign = (alignment.y() + 1.0f) * 0.5f;
            float scaledLeft = boundsLeft * newScaleX;
            float scaledTop = boundsTop * newScaleY;
            float widthRemainder = m_layoutWidth - (boundsW * newScaleX);
            float heightRemainder = m_layoutHeight - (boundsH * newScaleY);
            newOffsetX = -scaledLeft + widthRemainder * xAlign;
            newOffsetY = -scaledTop + heightRemainder * yAlign;
        }

        if (newScaleX != scaleX() || newScaleY != scaleY())
        {
            scaleX(newScaleX);
            scaleY(newScaleY);
        }
    }
    if (newOffsetX != m_layoutOffsetX || newOffsetY != m_layoutOffsetY)
    {
        m_layoutOffsetX = newOffsetX;
        m_layoutOffsetY = newOffsetY;
        // Offset is applied in updateTransform(), so changing it must mark the
        // local transform dirty (not just world transform).
        markTransformDirty();
    }
}

AABB Image::localBounds() const
{
    if (imageAsset() == nullptr)
    {
        return AABB();
    }
    return AABB::fromLTWH(-width() * originX(),
                          -height() * originY(),
                          width(),
                          height());
}

ImageAsset* Image::imageAsset() const { return (ImageAsset*)m_fileAsset.get(); }

#ifdef TESTING
#include "rive/shapes/mesh.hpp"
Mesh* Image::mesh() const { return static_cast<Mesh*>(m_Mesh); };
#endif
