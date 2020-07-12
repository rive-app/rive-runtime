#ifndef _RIVE_CORE_CONTEXT_HPP_
#define _RIVE_CORE_CONTEXT_HPP_
#include "animation/animation.hpp"
#include "animation/cubic_interpolator.hpp"
#include "animation/keyed_object.hpp"
#include "animation/keyed_property.hpp"
#include "animation/keyframe.hpp"
#include "animation/keyframe_color.hpp"
#include "animation/keyframe_double.hpp"
#include "animation/keyframe_draw_order.hpp"
#include "animation/keyframe_draw_order_value.hpp"
#include "animation/linear_animation.hpp"
#include "artboard.hpp"
#include "backboard.hpp"
#include "component.hpp"
#include "container_component.hpp"
#include "drawable.hpp"
#include "node.hpp"
#include "shapes/cubic_asymmetric_vertex.hpp"
#include "shapes/cubic_detached_vertex.hpp"
#include "shapes/cubic_mirrored_vertex.hpp"
#include "shapes/cubic_vertex.hpp"
#include "shapes/ellipse.hpp"
#include "shapes/paint/fill.hpp"
#include "shapes/paint/gradient_stop.hpp"
#include "shapes/paint/linear_gradient.hpp"
#include "shapes/paint/radial_gradient.hpp"
#include "shapes/paint/shape_paint.hpp"
#include "shapes/paint/solid_color.hpp"
#include "shapes/paint/stroke.hpp"
#include "shapes/parametric_path.hpp"
#include "shapes/path.hpp"
#include "shapes/path_composer.hpp"
#include "shapes/path_vertex.hpp"
#include "shapes/points_path.hpp"
#include "shapes/rectangle.hpp"
#include "shapes/shape.hpp"
#include "shapes/straight_vertex.hpp"
#include "shapes/triangle.hpp"
namespace rive
{
	class CoreContext
	{
	public:
		static Core* makeCoreInstance(int typeKey)
		{
			switch (typeKey)
			{
				case KeyedObjectBase::typeKey:
					return new KeyedObject();
				case KeyedPropertyBase::typeKey:
					return new KeyedProperty();
				case AnimationBase::typeKey:
					return new Animation();
				case CubicInterpolatorBase::typeKey:
					return new CubicInterpolator();
				case KeyFrameDoubleBase::typeKey:
					return new KeyFrameDouble();
				case KeyFrameColorBase::typeKey:
					return new KeyFrameColor();
				case LinearAnimationBase::typeKey:
					return new LinearAnimation();
				case KeyFrameDrawOrderBase::typeKey:
					return new KeyFrameDrawOrder();
				case KeyFrameDrawOrderValueBase::typeKey:
					return new KeyFrameDrawOrderValue();
				case LinearGradientBase::typeKey:
					return new LinearGradient();
				case RadialGradientBase::typeKey:
					return new RadialGradient();
				case StrokeBase::typeKey:
					return new Stroke();
				case SolidColorBase::typeKey:
					return new SolidColor();
				case GradientStopBase::typeKey:
					return new GradientStop();
				case FillBase::typeKey:
					return new Fill();
				case NodeBase::typeKey:
					return new Node();
				case ShapeBase::typeKey:
					return new Shape();
				case StraightVertexBase::typeKey:
					return new StraightVertex();
				case CubicAsymmetricVertexBase::typeKey:
					return new CubicAsymmetricVertex();
				case PointsPathBase::typeKey:
					return new PointsPath();
				case RectangleBase::typeKey:
					return new Rectangle();
				case CubicMirroredVertexBase::typeKey:
					return new CubicMirroredVertex();
				case TriangleBase::typeKey:
					return new Triangle();
				case EllipseBase::typeKey:
					return new Ellipse();
				case PathComposerBase::typeKey:
					return new PathComposer();
				case CubicDetachedVertexBase::typeKey:
					return new CubicDetachedVertex();
				case ArtboardBase::typeKey:
					return new Artboard();
				case BackboardBase::typeKey:
					return new Backboard();
			}
			return nullptr;
		}
	};
} // namespace rive

#endif