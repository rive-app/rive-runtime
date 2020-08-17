#ifndef _RIVE_BACKBOARD_HPP_
#define _RIVE_BACKBOARD_HPP_
#include "generated/backboard_base.hpp"
namespace rive
{
	class Backboard : public BackboardBase
	{
	public:
		void onAddedDirty(CoreContext* context) {}
		void onAddedClean(CoreContext* context) {}
	};
} // namespace rive

#endif