#ifndef _RIVE_KEY_FRAME_DOUBLE_BASE_HPP_
#define _RIVE_KEY_FRAME_DOUBLE_BASE_HPP_
#include "animation/keyframe.hpp"
namespace rive
{
	class KeyFrameDoubleBase : public KeyFrame
	{
	private:
		double m_Value = 0.0;
	public:
		double value() const { return m_Value; }
		void value(double value) { m_Value = value; }
	};
} // namespace rive

#endif