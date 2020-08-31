#ifndef _RIVE_STROKE_EFFECT_HPP_
#define _RIVE_STROKE_EFFECT_HPP_
namespace rive
{
	class RenderPath;
	class MetricsPath;

	class StrokeEffect
	{
	public:
		virtual RenderPath* effectPath(MetricsPath* source) = 0;
		virtual void invalidateEffect() = 0;
	};
} // namespace rive
#endif