#ifndef _RIVE_ARTBOARD_HPP_
#define _RIVE_ARTBOARD_HPP_
#include "animation/animation.hpp"
#include "core_context.hpp"
#include "generated/artboard_base.hpp"
#include "renderer.hpp"
#include "shapes/shape_paint_container.hpp"
#include <vector>
namespace rive
{
	class File;
	class Drawable;
	class Artboard : public ArtboardBase,
	                 public CoreContext,
	                 public ShapePaintContainer
	{
		friend class File;

	private:
		std::vector<Core*> m_Objects;
		std::vector<Animation*> m_Animations;
		std::vector<Component*> m_DependencyOrder;
		std::vector<Drawable*> m_Drawables;
		unsigned int m_DirtDepth = 0;
		RenderPath* m_RenderPath = nullptr;

		void sortDependencies();

	public:
		~Artboard();
		void initialize();
		void addObject(Core* object);
		void addAnimation(Animation* object);

		Core* resolve(int id) const override;

		void onAddedDirty(CoreContext* context) override;
		void onAddedClean(CoreContext* context) override {}
		void onComponentDirty(Component* component);

		/// Update components that depend on each other in DAG order.
		bool updateComponents();
		void update(ComponentDirt value) override;

		bool advance(double elapsedSeconds);
		void draw(Renderer* renderer);

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

		template <typename T = Animation> T* animation(std::string name)
		{
			for (auto animation : m_Animations)
			{
				if (animation->is<T>() && animation->as<T>()->name() == name)
				{
					return reinterpret_cast<T*>(animation);
				}
			}
			return nullptr;
		}
	};
} // namespace rive

#endif