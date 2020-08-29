#ifndef _RIVE_BACKBOARD_HPP_
#define _RIVE_BACKBOARD_HPP_
#include "generated/backboard_base.hpp"
namespace rive
{
	class Backboard : public BackboardBase
	{
	public:
		StatusCode onAddedDirty(CoreContext* context) { return StatusCode::Ok; }
		StatusCode onAddedClean(CoreContext* context) { return StatusCode::Ok; }
	};
} // namespace rive

#endif