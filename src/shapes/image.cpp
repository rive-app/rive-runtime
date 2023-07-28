#include "rive/math/hit_test.hpp"
#include "rive/shapes/image.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/shapes/mesh.hpp"
#include "rive/artboard.hpp"

using namespace rive;

void Image::draw(Renderer* renderer)
{
    rive::RenderImage* renderImage;
    if (m_ImageAsset == nullptr || renderOpacity() == 0.0f ||
        (renderImage = m_ImageAsset->renderImage()) == nullptr)
    {
        return;
    }

    if (!clip(renderer))
    {
        // We didn't clip, so make sure to save as we'll be doing some
        // transformations.
        renderer->save();
    }

    auto width = renderImage->width();
    auto height = renderImage->height();

    if (m_Mesh != nullptr)
    {
        m_Mesh->draw(renderer, renderImage, blendMode(), renderOpacity());
    }
    else
    {
        renderer->transform(worldTransform());
        renderer->translate(-width * originX(), -height * originY());
        renderer->drawImage(renderImage, blendMode(), renderOpacity());
    }

    renderer->restore();
}

Core* Image::hitTest(HitInfo* hinfo, const Mat2D& xform)
{
    // TODO: handle clip?

    auto renderImage = m_ImageAsset->renderImage();
    int width = renderImage->width();
    int height = renderImage->height();

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

void Image::assets(const std::vector<FileAsset*>& assets)
{
    if ((size_t)assetId() >= assets.size())
    {
        return;
    }
    auto asset = assets[assetId()];
    if (asset->is<ImageAsset>())
    {
        m_ImageAsset = asset->as<ImageAsset>();

        // If we have a mesh and we're in the source artboard, let's initialize
        // the mesh buffers.
        if (m_Mesh != nullptr && !artboard()->isInstance())
        {
            m_Mesh->initializeSharedBuffers(m_ImageAsset->renderImage());
        }
    }
}

Core* Image::clone() const
{
    Image* twin = ImageBase::clone()->as<Image>();
    twin->m_ImageAsset = m_ImageAsset;
    return twin;
}

void Image::setMesh(Mesh* mesh) { m_Mesh = mesh; }
Mesh* Image::mesh() const { return m_Mesh; }
