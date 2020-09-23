#ifndef _RIVE_PATH_COMPOSER_HPP_
#define _RIVE_PATH_COMPOSER_HPP_
#include "generated/shapes/path_composer_base.hpp"
namespace rive
{
	class Shape;
	class CommandPath;
	class RenderPath;
	class PathComposer : public PathComposerBase
	{
	private:
		Shape* m_Shape = nullptr;
		CommandPath* m_LocalPath = nullptr;
		CommandPath* m_WorldPath = nullptr;

	public:
		~PathComposer();
		Shape* shape() const { return m_Shape; }
		StatusCode onAddedClean(CoreContext* context) override;
		void buildDependencies() override;
		void update(ComponentDirt value) override;

		CommandPath* localPath() const { return m_LocalPath; }
		CommandPath* worldPath() const { return m_WorldPath; }
	};
} // namespace rive
#endif