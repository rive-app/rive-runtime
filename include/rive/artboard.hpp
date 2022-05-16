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

#include <queue>
#include <vector>

namespace rive {
    class File;
    class Drawable;
    class Factory;
    class Node;
    class DrawTarget;
    class ArtboardImporter;
    class NestedArtboard;
    class ArtboardInstance;
    class LinearAnimationInstance;
    class StateMachineInstance;

    class Artboard : public ArtboardBase, public CoreContext, public ShapePaintContainer {
        friend class File;
        friend class ArtboardImporter;
        friend class Component;

    private:
        std::vector<Core*> m_Objects;
        std::vector<LinearAnimation*> m_Animations;
        std::vector<StateMachine*> m_StateMachines;
        std::vector<Component*> m_DependencyOrder;
        std::vector<Drawable*> m_Drawables;
        std::vector<DrawTarget*> m_DrawTargets;
        std::vector<NestedArtboard*> m_NestedArtboards;

        unsigned int m_DirtDepth = 0;
        std::unique_ptr<CommandPath> m_BackgroundPath;
        std::unique_ptr<CommandPath> m_ClipPath;
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
        void addNestedArtboard(NestedArtboard* object);

    public:
        Artboard() {}
        ~Artboard();
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

        enum class DrawOption {
            kNormal,
            kHideBG,
            kHideFG,
        };
        void draw(Renderer* renderer, DrawOption = DrawOption::kNormal);

        CommandPath* clipPath() const { return m_ClipPath.get(); }
        CommandPath* backgroundPath() const { return m_BackgroundPath.get(); }

        const std::vector<Core*>& objects() const { return m_Objects; }

        AABB bounds() const;

        // Can we hide these from the public? (they use playable)
        bool isTranslucent(const LinearAnimation*) const;
        bool isTranslucent(const LinearAnimationInstance*) const;

        template <typename T = Component> T* find(const std::string& name) {
            for (auto object : m_Objects) {
                if (object != nullptr && object->is<T>() && object->as<T>()->name() == name) {
                    return reinterpret_cast<T*>(object);
                }
            }
            return nullptr;
        }

        size_t animationCount() const { return m_Animations.size(); }
        std::string animationNameAt(size_t index) const;

        size_t stateMachineCount() const { return m_StateMachines.size(); }
        std::string stateMachineNameAt(size_t index) const;

        LinearAnimation* firstAnimation() const;
        LinearAnimation* animation(const std::string& name) const;
        LinearAnimation* animation(size_t index) const;

        StateMachine* firstStateMachine() const;
        StateMachine* stateMachine(const std::string& name) const;
        StateMachine* stateMachine(size_t index) const;

        /// When provided, the designer has specified that this artboard should
        /// always autoplay this StateMachine.
        StateMachine* defaultStateMachine() const;

        /// Make an instance of this artboard, must be explictly deleted when no
        /// longer needed.
        // Deprecated...
        std::unique_ptr<ArtboardInstance> instance() const;

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

    class ArtboardInstance : public Artboard {
    public:
        ArtboardInstance() {}

        std::unique_ptr<LinearAnimationInstance> animationAt(size_t index);
        std::unique_ptr<LinearAnimationInstance> animationNamed(const std::string& name);

        std::unique_ptr<StateMachineInstance> stateMachineAt(size_t index);
        std::unique_ptr<StateMachineInstance> stateMachineNamed(const std::string& name);
    };
} // namespace rive

#endif
