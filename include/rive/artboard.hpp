#ifndef _RIVE_ARTBOARD_HPP_
#define _RIVE_ARTBOARD_HPP_
#include "animation/linear_animation.hpp"
#include "animation/state_machine.hpp"
#include "core_context.hpp"
#include "generated/artboard_base.hpp"
#include "math/aabb.hpp"
#include "renderer.hpp"
#include "shapes/shape_paint_container.hpp"
#include <vector>
namespace rive
{
	class File;
	class Drawable;
	class Node;
	class DrawTarget;
	class ArtboardImporter;

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
		unsigned int m_DirtDepth = 0;
		CommandPath* m_BackgroundPath = nullptr;
		CommandPath* m_ClipPath = nullptr;
		Drawable* m_FirstDrawable = nullptr;
		bool m_IsInstance = false;

		void sortDependencies();
		void sortDrawOrder();

#ifdef TESTING
	public:
#endif
		void addObject(Core* object);
		void addAnimation(LinearAnimation* object);
		void addStateMachine(StateMachine* object);

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
	};
} // namespace rive

#endif