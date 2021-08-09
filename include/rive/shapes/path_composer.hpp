#ifndef _RIVE_PATH_COMPOSER_HPP_
#define _RIVE_PATH_COMPOSER_HPP_
#include "rive/component.hpp"
namespace rive
{
	class Shape;
	class CommandPath;
	class RenderPath;
	class PathComposer : public Component
	{
	private:
		Shape* m_Shape;
		CommandPath* m_LocalPath = nullptr;
		CommandPath* m_WorldPath = nullptr;

	public:
		PathComposer(Shape* shape);
		~PathComposer();
		Shape* shape() const { return m_Shape; }
		void buildDependencies() override;
		void update(ComponentDirt value) override;

		CommandPath* localPath() const { return m_LocalPath; }
		CommandPath* worldPath() const { return m_WorldPath; }
	};
} // namespace rive
#endif