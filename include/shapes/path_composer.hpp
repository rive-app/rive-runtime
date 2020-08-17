#ifndef _RIVE_PATH_COMPOSER_HPP_
#define _RIVE_PATH_COMPOSER_HPP_
#include "generated/shapes/path_composer_base.hpp"
namespace rive
{
	class Shape;
	class RenderPath;
	class PathComposer : public PathComposerBase
	{
	private:
		Shape* m_Shape = nullptr;
		RenderPath* m_LocalPath = nullptr;
		RenderPath* m_WorldPath = nullptr;
		RenderPath* m_DifferencePath = nullptr;

	public:
		~PathComposer();
		Shape* shape() const { return m_Shape; }
		void onAddedClean(CoreContext* context) override;
		void buildDependencies() override;
		void update(ComponentDirt value) override;

		RenderPath* localPath() const { return m_LocalPath; }
		RenderPath* worldPath() const { return m_WorldPath; }
		RenderPath* differencePath() const { return m_DifferencePath; }
	};
} // namespace rive
#endif