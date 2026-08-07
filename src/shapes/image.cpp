#include "rive/math/hit_test.hpp"
#include "rive/shapes/image.hpp"
#include "rive/node.hpp"
#include "rive/layout/layout_node_style.hpp"
#include "rive/layout/layout_participant.hpp"
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

    if (m_Mesh != nullptr)
    {
        m_Mesh->draw(renderer,
                     renderImage,
                     imageSampler(),
                     blendMode(),
                     renderOpacity());
    }
    else
    {
        renderer->transform(worldTransform());
        renderer->translate(-width * originX(), -height * originY());
        renderer->drawImage(renderImage,
                            imageSampler(),
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
    // Files exported before 7.2 overwrite scaleX/scaleY with the layout fit, so
    // any stored scale on a layout image was ignored. Keep that legacy behavior
    // for those files; newer files compose the fit as a separate scale so the
    // user scale stays editable/animatable.
    int major = importStack.majorVersion();
    int minor = importStack.minorVersion();
    m_layoutScaleSeparate = major > 7 || (major == 7 && minor >= 2);
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
    twin->m_layoutScaleSeparate = m_layoutScaleSeparate;
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
    // Hug to the intrinsic image size. Only an `exactly` constraint overrides
    // the natural size — `atMost` is the available space and must NOT grow a
    // hugging image to fill it.
    float measuredWidth =
        widthMode == LayoutMeasureMode::exactly ? width : Image::width();
    float measuredHeight =
        heightMode == LayoutMeasureMode::exactly ? height : Image::height();
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

void Image::composeWorldTransform()
{
#ifdef WITH_RIVE_LAYOUT
    auto* participant = layoutParticipant();
    if (participant != nullptr && m_ParentTransformComponent != nullptr)
    {
        // Origin 0: the slot base is just the slot top-left (the image composes
        // its origin + fit separately in updateTransform).
        Mat2D base = Mat2D::fromTranslation(
            Vec2D(participant->resolvedLeft(), participant->resolvedTop()));
        m_WorldTransform =
            m_ParentTransformComponent->worldTransform() * base * m_Transform;
        return;
    }
#endif
    Super::composeWorldTransform();
}

LayoutParticipant* Image::layoutParticipant() const
{
    for (auto* child : children())
    {
        if (child->is<LayoutParticipant>())
        {
            return child->as<LayoutParticipant>();
        }
    }
    return nullptr;
}

bool Image::isParticipatingInLayout() const
{
    return layoutParticipant() != nullptr;
}

void Image::updateTransform()
{
    Super::updateTransform();
    // Compose the layout fit scale on top of the user transform (innermost), so
    // the user's scaleX/scaleY (built by Super) remain free to be animated:
    //   M = T(offset) * UserLocal * S(fitScale)
    m_Transform.scaleByValues(m_layoutScaleX, m_layoutScaleY);
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
        auto imageFit = static_cast<ImageFit>(fit());
        switch (imageFit)
        {
            case ImageFit::contain:
            {
                float s =
                    std::fmin(m_layoutWidth / imgW, m_layoutHeight / imgH);
                newScaleX = newScaleY = s;
                break;
            }
            case ImageFit::cover:
            {
                float s =
                    std::fmax(m_layoutWidth / imgW, m_layoutHeight / imgH);
                newScaleX = newScaleY = s;
                break;
            }
            case ImageFit::fitWidth:
                newScaleX = newScaleY = m_layoutWidth / imgW;
                break;
            case ImageFit::fitHeight:
                newScaleX = newScaleY = m_layoutHeight / imgH;
                break;
            case ImageFit::none:
                newScaleX = newScaleY = 1.0f;
                break;
            case ImageFit::scaleDown:
            {
                float s =
                    std::fmin(m_layoutWidth / imgW, m_layoutHeight / imgH);
                s = s < 1.0f ? s : 1.0f;
                newScaleX = newScaleY = s;
                break;
            }
            case ImageFit::fill:
            case ImageFit::resize:
            default:
                newScaleX = m_layoutWidth / imgW;
                newScaleY = m_layoutHeight / imgH;
                break;
        }

        // Compatibility: for a legacy (controlSized) parent, resize does not
        // apply fit/alignment translation offsets (only scale). A participant
        // composes the origin-based offset for resize too, so the image fills
        // its slot positionally as well as in size.
        if (imageFit != ImageFit::resize || isParticipatingInLayout())
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

        if (m_layoutScaleSeparate)
        {
            if (newScaleX != m_layoutScaleX || newScaleY != m_layoutScaleY)
            {
                m_layoutScaleX = newScaleX;
                m_layoutScaleY = newScaleY;
                // Fit scale is composed in updateTransform(), so changing it
                // must mark the local transform dirty (not just the world
                // transform).
                markTransformDirty();
            }
        }
        else
        {
            // Legacy (pre-7.2): the fit overwrites the user scale.
            if (newScaleX != scaleX() || newScaleY != scaleY())
            {
                scaleX(newScaleX);
                scaleY(newScaleY);
            }
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

ImageSampler Image::imageSampler() const
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
    // Node values are offset by one, zero means inherit from the asset.
    if (samplerFilter() != 0)
    {
        sampler.filter = filterValue(samplerFilter() - 1);
    }
    if (samplerWrapX() != 0)
    {
        sampler.wrapX = wrapValue(samplerWrapX() - 1);
    }
    if (samplerWrapY() != 0)
    {
        sampler.wrapY = wrapValue(samplerWrapY() - 1);
    }
    return sampler;
}

#ifdef TESTING
#include "rive/shapes/mesh.hpp"
Mesh* Image::mesh() const { return static_cast<Mesh*>(m_Mesh); };
#endif
