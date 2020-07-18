#ifndef _RIVE_FILL_HPP_
#define _RIVE_FILL_HPP_
#include "generated/shapes/paint/fill_base.hpp"
namespace rive
{
	class Fill : public FillBase
	{
	public:
		void onAddedClean(CoreContext* context) {}
	};
} // namespace rive

#endif