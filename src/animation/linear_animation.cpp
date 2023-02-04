#include "rive/animation/linear_animation.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/import_stack.hpp"

using namespace rive;

#ifdef TESTING
int LinearAnimation::deleteCount = 0;
#endif

LinearAnimation::LinearAnimation() {}

LinearAnimation::~LinearAnimation()
{
#ifdef TESTING
    deleteCount++;
#endif
}

StatusCode LinearAnimation::onAddedDirty(CoreContext* context)
{
    StatusCode code;
    for (const auto& object : m_KeyedObjects)
    {
        if ((code = object->onAddedDirty(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
    return StatusCode::Ok;
}

StatusCode LinearAnimation::onAddedClean(CoreContext* context)
{
    StatusCode code;
    for (const auto& object : m_KeyedObjects)
    {
        if ((code = object->onAddedClean(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
    return StatusCode::Ok;
}

void LinearAnimation::addKeyedObject(std::unique_ptr<KeyedObject> object)
{
    m_KeyedObjects.push_back(std::move(object));
}

void LinearAnimation::apply(Artboard* artboard, float time, float mix) const
{
    for (const auto& object : m_KeyedObjects)
    {
        object->apply(artboard, time, mix);
    }
}

StatusCode LinearAnimation::import(ImportStack& importStack)
{
    auto artboardImporter = importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    artboardImporter->addAnimation(this);
    return Super::import(importStack);
}

float LinearAnimation::startSeconds() const
{
    return (enableWorkArea() ? workStart() : 0) / (float)fps();
}
float LinearAnimation::endSeconds() const
{
    return (enableWorkArea() ? workEnd() : duration()) / (float)fps();
}
float LinearAnimation::durationSeconds() const { return endSeconds() - startSeconds(); }

// Matches Dart modulus: https://api.dart.dev/stable/2.19.0/dart-core/double/operator_modulo.html
static float positiveMod(float value, float range)
{
    assert(range > 0.0f);
    float v = fmodf(value, range);
    if (v < 0.0f)
    {
        v += range;
    }
    return v;
}

float LinearAnimation::globalToLocalSeconds(float seconds) const
{
    switch (loop())
    {
        case Loop::oneShot:
            return seconds + startSeconds();
        case Loop::loop:
            return positiveMod(seconds, (endSeconds() - startSeconds())) + startSeconds();
        case Loop::pingPong:
            float localTime = positiveMod(seconds, (endSeconds() - startSeconds()));
            int direction = ((int)(seconds / (endSeconds() - startSeconds()))) % 2;
            return direction == 0 ? localTime + startSeconds() : endSeconds() - localTime;
    }
    RIVE_UNREACHABLE();
}