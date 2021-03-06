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

	class Artboard : public ArtboardBase,
	                 public CoreContext,
	                 public ShapePaintContainer
	{
		friend class File;

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

		void sortDependencies();
		void sortDrawOrder();

	public:
		~Artboard();
		StatusCode initialize();
		void addObject(Core* object);
		void addAnimation(LinearAnimation* object);
		void addStateMachine(StateMachine* object);

		Core* resolve(int id) const override;

		StatusCode onAddedClean(CoreContext* context) override
		{
			return StatusCode::Ok;
		}
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
				if (object->is<T>() && object->as<T>()->name() == name)
				{
					return reinterpret_cast<T*>(object);
				}
			}
			return nullptr;
		}

		LinearAnimation* firstAnimation();
		LinearAnimation* animation(std::string name);
		LinearAnimation* animation(size_t index);
		size_t animationCount() { return m_Animations.size(); }

		StateMachine* firstStateMachine();
		StateMachine* stateMachine(std::string name);
		StateMachine* stateMachine(size_t index);
		size_t stateMachineCount() { return m_StateMachines.size(); }
	};
} // namespace rive

#endif