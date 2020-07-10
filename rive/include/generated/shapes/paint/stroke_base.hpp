#ifndef _RIVE_STROKE_BASE_HPP_
#define _RIVE_STROKE_BASE_HPP_
#include "shapes/paint/shape_paint.hpp"
namespace rive
{
	class StrokeBase : public ShapePaint
	{
	private:
		double m_Thickness = 1;
		int m_Cap = 0;
		int m_Join = 0;
		bool m_TransformAffectsStroke = true;
	public:
		double thickness() const { return m_Thickness; }
		void thickness(double value) { m_Thickness = value; }

		int cap() const { return m_Cap; }
		void cap(int value) { m_Cap = value; }

		int join() const { return m_Join; }
		void join(int value) { m_Join = value; }

		bool transformAffectsStroke() const { return m_TransformAffectsStroke; }
		void transformAffectsStroke(bool value) { m_TransformAffectsStroke = value; }
	};
} // namespace rive

#endif