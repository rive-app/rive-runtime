#ifndef _RIVE_SCENE_HPP_
#define _RIVE_SCENE_HPP_

#include "rive/animation/loop.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/animation/keyed_callback_reporter.hpp"
#include "rive/core/field_types/core_callback_type.hpp"
#include "rive/hit_result.hpp"
#include <string>

namespace rive
{
class ArtboardInstance;
class Renderer;

class SMIInput;
class SMIBool;
class SMINumber;
class SMITrigger;

class Scene : public KeyedCallbackReporter, public CallbackContext
{
protected:
    Scene(ArtboardInstance*);

public:
    ~Scene() override {}

    Scene(Scene const& lhs) : m_artboardInstance(lhs.m_artboardInstance) {}

    float width() const;
    float height() const;
    AABB bounds() const { return {0, 0, this->width(), this->height()}; }

    virtual std::string name() const = 0;

    // Returns onShot if this has no looping (e.g. a statemachine)
    virtual Loop loop() const = 0;
    // Returns true iff the Scene is known to not be fully opaque
    virtual bool isTranslucent() const = 0;
    // returns -1 for continuous
    virtual float durationSeconds() const = 0;

    // returns true if draw() should be called
    virtual bool advanceAndApply(float elapsedSeconds) = 0;

    void draw(Renderer*);

    virtual HitResult pointerDown(Vec2D);
    virtual HitResult pointerMove(Vec2D);
    virtual HitResult pointerUp(Vec2D);
    virtual HitResult pointerExit(Vec2D);

    virtual size_t inputCount() const;
    virtual SMIInput* input(size_t index) const;
    virtual SMIBool* getBool(const std::string&) const;
    virtual SMINumber* getNumber(const std::string&) const;
    virtual SMITrigger* getTrigger(const std::string&) const;

    /// Report which time based events have elapsed on a timeline within this
    /// state machine.
    void reportKeyedCallback(uint32_t objectId,
                             uint32_t propertyKey,
                             float elapsedSeconds) override;

protected:
    ArtboardInstance* m_artboardInstance;
};

} // namespace rive

#endif
