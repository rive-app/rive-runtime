#include "rive/core_context.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/artboard.hpp"
#include "rive/hittest_command_path.hpp"
#include "rive/importers/artboard_importer.hpp"

using namespace rive;

void TextValueRun::textChanged()
{
    m_length = -1;
    parent()->as<Text>()->markShapeDirty();
}

Text* TextValueRun::textComponent() const { return parent()->as<Text>(); }

StatusCode TextValueRun::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    if (parent() != nullptr && parent()->is<Text>())
    {
        parent()->as<Text>()->addRun(this);
        return StatusCode::Ok;
    }

    return StatusCode::MissingObject;
}

StatusCode TextValueRun::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    auto coreObject = context->resolve(styleId());
    if (coreObject == nullptr || !coreObject->is<TextStylePaint>())
    {
        return StatusCode::MissingObject;
    }

    m_style = static_cast<TextStylePaint*>(coreObject);

    return StatusCode::Ok;
}

void TextValueRun::styleIdChanged()
{
    auto coreObject = artboard()->resolve(styleId());
    if (coreObject != nullptr && coreObject->is<TextStylePaint>())
    {
        m_style = static_cast<TextStylePaint*>(coreObject);
        parent()->as<Text>()->markShapeDirty();
    }
}

uint32_t TextValueRun::offset() const
{
#ifdef WITH_RIVE_TEXT
    Text* text = parent()->as<Text>();
    uint32_t offset = 0;

    for (TextValueRun* run : text->runs())
    {
        if (run == this)
        {
            break;
        }
        offset += run->length();
    }
    return offset;
#else
    return 0;
#endif
}

bool TextValueRun::canHitTest() const
{
    return (m_isHitTarget && textComponent() != nullptr &&
            !m_localBounds.isEmptyOrNaN());
}

void TextValueRun::resetHitTest()
{
    m_glyphHitRects.clear();
    m_localBounds = AABB::forExpansion();
}

void TextValueRun::addHitRect(const AABB& rect)
{
    AABB::expandTo(m_localBounds, rect.min());
    AABB::expandTo(m_localBounds, rect.max());
    m_glyphHitRects.push_back(rect);
}

void TextValueRun::computeHitContours()
{
    if (!m_rectanglesToContour)
    {
        m_rectanglesToContour = rivestd::make_unique<RectanglesToContour>();
    }
    else
    {
        m_rectanglesToContour->reset();
    }
    for (const AABB& rect : m_glyphHitRects)
    {
        m_rectanglesToContour->addRect(rect);
    }
    m_rectanglesToContour->computeContours();
}

bool TextValueRun::hitTestAABB(const Vec2D& position)
{
    if (!canHitTest())
    {
        return false;
    }

    if (textComponent()->overflow() != TextOverflow::visible)
    {
        Mat2D inverseWorld;
        if (!textComponent()->worldTransform().invert(&inverseWorld))
        {
            return false;
        }

        if (!textComponent()->localBounds().contains(inverseWorld * position))
        {
            return false;
        }
    }

    Mat2D inverseWorld;
    Mat2D worldTransform =
        textComponent()->worldTransform() * textComponent()->m_transform;
    if (worldTransform.invert(&inverseWorld))
    {
        auto localWorld = inverseWorld * position;
        return m_localBounds.contains(localWorld);
    }

    return false;
}

bool TextValueRun::hitTestHiFi(const Vec2D& position, float hitRadius)
{
    if (!canHitTest())
    {
        return false;
    }

    auto hitArea = AABB(position.x - hitRadius,
                        position.y - hitRadius,
                        position.x + hitRadius,
                        position.y + hitRadius)
                       .round();
    HitTestCommandPath tester(hitArea);
    tester.setXform(textComponent()->worldTransform() *
                    textComponent()->m_transform);

    assert(m_rectanglesToContour != nullptr);
    for (auto contour : *m_rectanglesToContour)
    {
        assert(contour.begin() != contour.end());
        auto pointItr = contour.begin();
        auto end = contour.end();

        Vec2D firstPoint = *pointItr;
        tester.moveTo(firstPoint.x, firstPoint.y);

        while (++pointItr != end)
        {
            Vec2D point = *pointItr;
            tester.lineTo(point.x, point.y);
        }

        tester.close();
    }

    return tester.wasHit();
}

void TextValueRun::isHitTarget(bool value) { m_isHitTarget = value; }

bool TextValueRun::hitTestPoint(const Vec2D& position,
                                bool skipOnUnclipped,
                                bool isPrimaryHit)
{
    if (hitTestAABB(position) &&
        Component::hitTestPoint(position, skipOnUnclipped, isPrimaryHit))
    {
        return hitTestHiFi(position, 2);
    }
    return false;
}