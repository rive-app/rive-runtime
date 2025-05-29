#include "rive/text/text_style_paint.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_variation_helper.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

TextStylePaint::TextStylePaint() : m_path(true, FillRule::clockwise) {}

void TextStylePaint::rewindPath()
{
    m_path.rewind();
    m_hasContents = false;
    m_opacityPaths.clear();
}

bool TextStylePaint::addPath(const RawPath& rawPath, float opacity)
{
    bool hadContents = m_hasContents;
    m_hasContents = true;
    if (opacity > 0.0f)
    {

        // m_path contains everything, so inner feather bounds can work.
        m_path.addPathClockwise(rawPath);

        // Bucket by opacity
        auto itr = m_opacityPaths.find(opacity);
        ShapePaintPath* shapePaintPath = nullptr;
        if (itr != m_opacityPaths.end())
        {
            ShapePaintPath& path = itr->second;
            shapePaintPath = &path;
        }
        else
        {
            m_opacityPaths[opacity] = ShapePaintPath(true, FillRule::clockwise);
            shapePaintPath = &m_opacityPaths.at(opacity);
        }
        shapePaintPath->addPathClockwise(rawPath);
    }

    return !hadContents;
}

void TextStylePaint::draw(Renderer* renderer, const Mat2D& worldTransform)
{
    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->shouldDraw())
        {
            continue;
        }
        shapePaint->blendMode(parent()->as<Text>()->blendMode());

        // For blend modes to work, opaque paths render first
        auto itr = m_opacityPaths.find(1.0f);
        if (itr != m_opacityPaths.end())
        {
            ShapePaintPath& path = itr->second;
            shapePaint->draw(renderer, &path, worldTransform, true);
        }

        if (m_paintPool.size() < m_opacityPaths.size())
        {
            m_paintPool.reserve(m_opacityPaths.size());
            Factory* factory = artboard()->factory();
            while (m_paintPool.size() < m_opacityPaths.size())
            {
                m_paintPool.emplace_back(factory->makeRenderPaint());
            }
        }

        uint32_t paintIndex = 0;
        for (itr = m_opacityPaths.begin(); itr != m_opacityPaths.end(); itr++)
        {
            // Don't render opaque paths twice
            if (itr->first == 1.0f)
            {
                continue;
            }
            RenderPaint* renderPaint = m_paintPool[paintIndex++].get();
            shapePaint->applyTo(renderPaint, itr->first);
            if (auto feather = shapePaint->feather())
            {
                renderPaint->feather(feather->strength());
            }
            ShapePaintPath& path = itr->second;
            shapePaint->draw(renderer,
                             &path,
                             worldTransform,
                             true,
                             renderPaint);
        }
    }
}

const Mat2D& TextStylePaint::shapeWorldTransform() const
{
    return parent()->as<Text>()->shapeWorldTransform();
}

Component* TextStylePaint::pathBuilder() { return parent(); }

Core* TextStylePaint::clone() const
{
    TextStylePaint* twin = TextStylePaintBase::clone()->as<TextStylePaint>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }

    return twin;
}
