#ifndef _RIVE_ARTBOARD_HPP_
#define _RIVE_ARTBOARD_HPP_

#include "rive/animation/linear_animation.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/core_context.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/hit_info.hpp"
#include "rive/math/aabb.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/event.hpp"

#include <queue>
#include <vector>

namespace rive
{
class File;
class Drawable;
class Factory;
class Node;
class DrawTarget;
class ArtboardImporter;
class NestedArtboard;
class ArtboardInstance;
class LinearAnimationInstance;
class Scene;
class StateMachineInstance;
class Joystick;
class TextValueRun;
class Event;

class Artboard : public ArtboardBase, public CoreContext, public ShapePaintContainer
{
    friend class File;
    friend class ArtboardImporter;
    friend class Component;

private:
    std::vector<Core*> m_Objects;
    std::vector<LinearAnimation*> m_Animations;
    std::vector<StateMachine*> m_StateMachines;
    std::vector<TextValueRun*> m_TextValueRuns;
    std::vector<Event*> m_Events;
    std::vector<Component*> m_DependencyOrder;
    std::vector<Drawable*> m_Drawables;
    std::vector<DrawTarget*> m_DrawTargets;
    std::vector<NestedArtboard*> m_NestedArtboards;
    std::vector<Joystick*> m_Joysticks;
    bool m_JoysticksApplyBeforeUpdate = true;

    unsigned int m_DirtDepth = 0;
    rcp<RenderPath> m_BackgroundPath;
    rcp<RenderPath> m_ClipPath;
    Factory* m_Factory = nullptr;
    Drawable* m_FirstDrawable = nullptr;
    bool m_IsInstance = false;
    bool m_FrameOrigin = true;

    void sortDependencies();
    void sortDrawOrder();

    Artboard* getArtboard() override { return this; }

#ifdef TESTING
public:
    Artboard(Factory* factory) : m_Factory(factory) {}
#endif
    void addObject(Core* object);
    void addAnimation(LinearAnimation* object);
    void addStateMachine(StateMachine* object);
    void addTextValueRun(TextValueRun* object);
    void addEvent(Event* object);

public:
    Artboard() {}
    ~Artboard() override;
    StatusCode initialize();

    Core* resolve(uint32_t id) const override;

    /// Find the id of a component in the artboard the object in the artboard. The artboard
    /// itself has id 0 so we use that as a flag for not found.
    uint32_t idOf(Core* object) const;

    Factory* factory() const { return m_Factory; }

    // EXPERIMENTAL -- for internal testing only for now.
    // DO NOT RELY ON THIS as it may change/disappear in the future.
    Core* hitTest(HitInfo*, const Mat2D* = nullptr);

    void onComponentDirty(Component* component);

    /// Update components that depend on each other in DAG order.
    bool updateComponents();
    void update(ComponentDirt value) override;
    void onDirty(ComponentDirt dirt) override;

    bool advance(double elapsedSeconds);

    enum class DrawOption
    {
        kNormal,
        kHideBG,
        kHideFG,
    };
    void draw(Renderer* renderer, DrawOption = DrawOption::kNormal);
    void addToRenderPath(RenderPath* path, const Mat2D& transform);

#ifdef TESTING
    RenderPath* clipPath() const { return m_ClipPath.get(); }
    RenderPath* backgroundPath() const { return m_BackgroundPath.get(); }
#endif

    const std::vector<Core*>& objects() const { return m_Objects; }
    const std::vector<NestedArtboard*> nestedArtboards() const { return m_NestedArtboards; }

    AABB bounds() const;

    // Can we hide these from the public? (they use playable)
    bool isTranslucent() const;
    bool isTranslucent(const LinearAnimation*) const;
    bool isTranslucent(const LinearAnimationInstance*) const;

    template <typename T = Component> T* find(const std::string& name)
    {
        for (auto object : m_Objects)
        {
            if (object != nullptr && object->is<T>() && object->as<T>()->name() == name)
            {
                return static_cast<T*>(object);
            }
        }
        return nullptr;
    }

    template <typename T = Component> std::vector<T*> find()
    {
        std::vector<T*> results;
        for (auto object : m_Objects)
        {
            if (object != nullptr && object->is<T>())
            {
                results.push_back(static_cast<T*>(object));
            }
        }
        return results;
    }

    size_t animationCount() const { return m_Animations.size(); }
    std::string animationNameAt(size_t index) const;

    size_t stateMachineCount() const { return m_StateMachines.size(); }
    std::string stateMachineNameAt(size_t index) const;

    size_t textValueRunCount() const { return m_TextValueRuns.size(); }
    TextValueRun* textValueRunAt(size_t index) const;

    size_t eventCount() const { return m_Events.size(); }
    Event* eventAt(size_t index) const;

    LinearAnimation* firstAnimation() const { return animation(0); }
    LinearAnimation* animation(const std::string& name) const;
    LinearAnimation* animation(size_t index) const;

    StateMachine* firstStateMachine() const { return stateMachine(0); }
    StateMachine* stateMachine(const std::string& name) const;
    StateMachine* stateMachine(size_t index) const;

    /// When provided, the designer has specified that this artboard should
    /// always autoplay this StateMachine. Returns -1 if it was not
    // provided.
    int defaultStateMachineIndex() const;

    /// Make an instance of this artboard.
    template <typename T = ArtboardInstance> std::unique_ptr<T> instance() const
    {
        std::unique_ptr<T> artboardClone(new T);
        artboardClone->copy(*this);

        artboardClone->m_Factory = m_Factory;
        artboardClone->m_FrameOrigin = m_FrameOrigin;
        artboardClone->m_IsInstance = true;

        std::vector<Core*>& cloneObjects = artboardClone->m_Objects;
        cloneObjects.push_back(artboardClone.get());

        if (!m_Objects.empty())
        {
            // Skip first object (artboard).
            auto itr = m_Objects.begin();
            while (++itr != m_Objects.end())
            {
                auto object = *itr;
                cloneObjects.push_back(object == nullptr ? nullptr : object->clone());
            }
        }

        for (auto animation : m_Animations)
        {
            artboardClone->m_Animations.push_back(animation);
        }
        for (auto stateMachine : m_StateMachines)
        {
            artboardClone->m_StateMachines.push_back(stateMachine);
        }
        for (auto textRun : m_TextValueRuns)
        {
            artboardClone->m_TextValueRuns.push_back(textRun);
        }
        for (auto event : m_Events)
        {
            artboardClone->m_Events.push_back(event);
        }

        if (artboardClone->initialize() != StatusCode::Ok)
        {
            artboardClone = nullptr;
        }

        assert(artboardClone->isInstance());
        return artboardClone;
    }

    /// Returns true if the artboard is an instance of another
    bool isInstance() const { return m_IsInstance; }

    /// Returns true when the artboard will shift the origin from the top
    /// left to the relative width/height of the artboard itself. This is
    /// what the editor does visually when you change the origin value to
    /// give context as to where the origin lies within the framed bounds.
    bool frameOrigin() const { return m_FrameOrigin; }
    /// When composing multiple artboards together in a common world-space,
    /// it may be desireable to have them share the same space regardless of
    /// origin offset from the bounding artboard. Set frameOrigin to false
    /// to move the bounds relative to the origin instead of the origin
    /// relative to the bounds.
    void frameOrigin(bool value);

    StatusCode import(ImportStack& importStack) override;
};

class ArtboardInstance : public Artboard
{
public:
    ArtboardInstance();
    ~ArtboardInstance() override;

    std::unique_ptr<LinearAnimationInstance> animationAt(size_t index);
    std::unique_ptr<LinearAnimationInstance> animationNamed(const std::string& name);

    std::unique_ptr<StateMachineInstance> stateMachineAt(size_t index);
    std::unique_ptr<StateMachineInstance> stateMachineNamed(const std::string& name);

    /// When provided, the designer has specified that this artboard should
    /// always autoplay this StateMachine instance. If it was not specified,
    /// this returns nullptr.
    std::unique_ptr<StateMachineInstance> defaultStateMachine();

    // This attemps to always return *something*, in this search order:
    // 1. default statemachine instance
    // 2. first statemachine instance
    // 3. first animation instance
    // 4. nullptr
    std::unique_ptr<Scene> defaultScene();
};
} // namespace rive

#endif
