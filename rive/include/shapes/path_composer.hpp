#ifndef _RIVE_PATH_COMPOSER_HPP_
#define _RIVE_PATH_COMPOSER_HPP_
#include "generated/shapes/path_composer_base.hpp"
namespace rive
{
	class Shape;
	class PathComposer : public PathComposerBase
	{
	private:
		Shape* m_Shape;

	public:
		Shape* shape() const { return m_Shape; }
		void onAddedClean(CoreContext* context);
		void buildDependencies();
	};
} // namespace rive
#endif