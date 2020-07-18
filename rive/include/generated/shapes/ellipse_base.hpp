#ifndef _RIVE_ELLIPSE_BASE_HPP_
#define _RIVE_ELLIPSE_BASE_HPP_
#include "shapes/parametric_path.hpp"
namespace rive
{
	class EllipseBase : public ParametricPath
	{
	public:
		static const int typeKey = 4;

		// Helper to quickly determine if a core object extends another without RTTI
		/// at runtime.
		bool isTypeOf(int typeKey) override
		{
			switch (typeKey)
			{
				case EllipseBase::typeKey:
				case ParametricPathBase::typeKey:
				case PathBase::typeKey:
				case NodeBase::typeKey:
				case ContainerComponentBase::typeKey:
				case ComponentBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif