#ifndef _RIVE_ARTBOARD_HPP_
#define _RIVE_ARTBOARD_HPP_

#include "rive/animation/linear_animation.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/core_context.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/hit_info.hpp"
#include "rive/message.hpp"
#include "rive/pointer_event.hpp"
#include "rive/math/aabb.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/shape_paint_container.hpp"

#include <queue>
#include <vector>

namespace rive {
    class File;
    class Drawable;
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
        Drawable* m_FirstDrawable = nullptr;
        bool m_IsInstance = false;
        bool m_FrameOrigin = true;

        std::queue<Message> m_MessageQueue;

        void sortDependencies();
        void sortDrawOrder();

#ifdef TESTING
    public:
#endif
        void addObject(Core* object);
        void addAnimation(LinearAnimation* object);
        void addStateMachine(StateMachine* object);
        void addNestedArtboard(NestedArtboard* object);

        void testing_only_enque_message(const Message&);

    public:
        ~Artboard();
        StatusCode initialize();

        Core* resolve(uint32_t id) const override;

        // EXPERIMENTAL -- for internal testing only for now.
        // DO NOT RELY ON THIS as it may change/disappear in the future.
        Core* hitTest(HitInfo*, const Mat2D* = nullptr);

        void onComponentDirty(Component* component);

        /// Update components that depend on each other in DAG order.
        bool updateComponents();
        void update(ComponentDirt value) override;
        void onDirty(ComponentDirt dirt) override;

        bool advance(double elapsedSeconds);

        // Call this to forward pointer events to the artboard
        // They will be processed when advance() is called.
        //
        void postPointerEvent(const PointerEvent&);

        // Returns true iff calling popMessage() will return true.
        bool hasMessages() const;
        
        // If there are any queued messages...
        //   copies the first message into msg parameter
        //   removes that message from the queue
        //   returns true
        // else
        //   ignores msg parameter
        //   returns false
        //
        bool nextMessage(Message* msg);

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
        bool isTranslucent(const LinearAnimation*) const;
        bool isTranslucent(const LinearAnimationInstance*) const;

        template <typename T = Component> T* find(std::string name) {
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
        LinearAnimation* animation(std::string name) const;
        LinearAnimation* animation(size_t index) const;

        StateMachine* firstStateMachine() const;
        StateMachine* stateMachine(std::string name) const;
        StateMachine* stateMachine(size_t index) const;

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
        std::unique_ptr<LinearAnimationInstance> animationAt(size_t index);
        std::unique_ptr<LinearAnimationInstance> animationNamed(std::string name);

        std::unique_ptr<StateMachineInstance> stateMachineAt(size_t index);
        std::unique_ptr<StateMachineInstance> stateMachineNamed(std::string name);
    };
} // namespace rive

#endif
