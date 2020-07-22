#ifndef _RIVE_CUBIC_DETACHED_VERTEX_HPP_
#define _RIVE_CUBIC_DETACHED_VERTEX_HPP_
#include "generated/shapes/cubic_detached_vertex_base.hpp"
namespace rive
{
	class CubicDetachedVertex : public CubicDetachedVertexBase
	{
	protected:
		void computeIn() override;
		void computeOut() override;
	};
} // namespace rive

#endif