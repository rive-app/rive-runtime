#ifndef _RIVE_CUBIC_VERTEX_BASE_HPP_
#define _RIVE_CUBIC_VERTEX_BASE_HPP_
#include "shapes/path_vertex.hpp"
namespace rive
{
	class CubicVertexBase : public PathVertex
	{
	public:
		static const int typeKey = 36;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case CubicVertexBase::typeKey:
				case PathVertexBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }

	protected:
	};
} // namespace rive

#endif