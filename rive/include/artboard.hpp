#ifndef _RIVE_ARTBOARD_HPP_
#define _RIVE_ARTBOARD_HPP_
#include "core_context.hpp"
#include "generated/artboard_base.hpp"
#include "shapes/shape_paint_container.hpp"
#include <vector>
namespace rive
{
	class Animation;
	class File;
	class Artboard : public ArtboardBase,
	                 public CoreContext,
	                 public ShapePaintContainer
	{
		friend class File;

	private:
		std::vector<Core*> m_Objects;
		std::vector<Animation*> m_Animations;
		std::vector<Component*> m_DependencyOrder;
		unsigned int m_DirtDepth = 0;

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

		bool advance(double elapsedSeconds);

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
	};
} // namespace rive

#endif