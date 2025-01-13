#include "rive/core_context.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style.hpp"
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
    if (coreObject == nullptr || !coreObject->is<TextStyle>())
    {
        return StatusCode::MissingObject;
    }

    m_style = static_cast<TextStyle*>(coreObject);

    return StatusCode::Ok;
}

void TextValueRun::styleIdChanged()
{
    auto coreObject = artboard()->resolve(styleId());
    if (coreObject != nullptr && coreObject->is<TextStyle>())
    {
        m_style = static_cast<TextStyle*>(coreObject);
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
    m_contours.clear();
    m_localBounds = AABB::forExpansion();
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
    for (const std::vector<Vec2D>& contour : m_contours)
    {
        tester.moveTo(contour[0].x, contour[0].y);
        for (auto i = 1; i < contour.size(); i++)
        {
            tester.lineTo(contour[i].x, contour[i].y);
        }
        tester.close();
    }
    return tester.wasHit();
}
