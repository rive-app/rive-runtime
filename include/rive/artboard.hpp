#ifndef _RIVE_ARTBOARD_HPP_
#define _RIVE_ARTBOARD_HPP_
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/core_context.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/math/aabb.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include <vector>
namespace rive
{
    class File;
    class Drawable;
    class Node;
    class DrawTarget;
    class ArtboardImporter;
    class NestedArtboard;

    class Artboard : public ArtboardBase,
                     public CoreContext,
                     public ShapePaintContainer
    {
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
        CommandPath* m_BackgroundPath = nullptr;
        CommandPath* m_ClipPath = nullptr;
        Drawable* m_FirstDrawable = nullptr;
        bool m_IsInstance = false;
        bool m_FrameOrigin = true;

        void sortDependencies();
        void sortDrawOrder();

#ifdef TESTING
    public:
#endif
        void addObject(Core* object);
        void addAnimation(LinearAnimation* object);
        void addStateMachine(StateMachine* object);
        void addNestedArtboard(NestedArtboard* object);

    public:
        ~Artboard();
        StatusCode initialize();

        Core* resolve(int id) const override;

        void onComponentDirty(Component* component);

        /// Update components that depend on each other in DAG order.
        bool updateComponents();
        void update(ComponentDirt value) override;
        void onDirty(ComponentDirt dirt) override;

        bool advance(double elapsedSeconds);
        void draw(Renderer* renderer);

        CommandPath* clipPath() const { return m_ClipPath; }
        CommandPath* backgroundPath() const { return m_BackgroundPath; }

        const std::vector<Core*>& objects() const { return m_Objects; }

        AABB bounds() const;

        template <typename T = Component> T* find(std::string name)
        {
            for (auto object : m_Objects)
            {
                if (object != nullptr && object->is<T>() &&
                    object->as<T>()->name() == name)
                {
                    return reinterpret_cast<T*>(object);
                }
            }
            return nullptr;
        }

        LinearAnimation* firstAnimation() const;
        LinearAnimation* animation(std::string name) const;
        LinearAnimation* animation(size_t index) const;
        size_t animationCount() const { return m_Animations.size(); }

        StateMachine* firstStateMachine() const;
        StateMachine* stateMachine(std::string name) const;
        StateMachine* stateMachine(size_t index) const;
        size_t stateMachineCount() const { return m_StateMachines.size(); }

        /// Make an instance of this artboard, must be explictly deleted when no
        /// longer needed.
        Artboard* instance() const;

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
} // namespace rive

#endif