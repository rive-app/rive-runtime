#ifndef _RIVE_CORE_REGISTRY_HPP_
#define _RIVE_CORE_REGISTRY_HPP_
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
#include "bones/bone.hpp"
#include "bones/cubic_weight.hpp"
#include "bones/root_bone.hpp"
#include "bones/skeletal_component.hpp"
#include "bones/skin.hpp"
#include "bones/tendon.hpp"
#include "bones/weight.hpp"
#include "component.hpp"
#include "container_component.hpp"
#include "drawable.hpp"
#include "node.hpp"
#include "shapes/clipping_shape.hpp"
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
#include "transform_component.hpp"
namespace rive
{
	class CoreRegistry
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
				case ClippingShapeBase::typeKey:
					return new ClippingShape();
				case PathComposerBase::typeKey:
					return new PathComposer();
				case CubicDetachedVertexBase::typeKey:
					return new CubicDetachedVertex();
				case ArtboardBase::typeKey:
					return new Artboard();
				case BackboardBase::typeKey:
					return new Backboard();
				case WeightBase::typeKey:
					return new Weight();
				case BoneBase::typeKey:
					return new Bone();
				case RootBoneBase::typeKey:
					return new RootBone();
				case SkinBase::typeKey:
					return new Skin();
				case TendonBase::typeKey:
					return new Tendon();
				case CubicWeightBase::typeKey:
					return new CubicWeight();
			}
			return nullptr;
		}
		static void setUint(Core* object, int propertyKey, int value)
		{
			switch (propertyKey)
			{
				case KeyedObjectBase::objectIdPropertyKey:
					object->as<KeyedObjectBase>()->objectId(value);
					break;
				case KeyedPropertyBase::propertyKeyPropertyKey:
					object->as<KeyedPropertyBase>()->propertyKey(value);
					break;
				case KeyFrameBase::framePropertyKey:
					object->as<KeyFrameBase>()->frame(value);
					break;
				case KeyFrameBase::interpolationTypePropertyKey:
					object->as<KeyFrameBase>()->interpolationType(value);
					break;
				case KeyFrameBase::interpolatorIdPropertyKey:
					object->as<KeyFrameBase>()->interpolatorId(value);
					break;
				case LinearAnimationBase::fpsPropertyKey:
					object->as<LinearAnimationBase>()->fps(value);
					break;
				case LinearAnimationBase::durationPropertyKey:
					object->as<LinearAnimationBase>()->duration(value);
					break;
				case LinearAnimationBase::loopValuePropertyKey:
					object->as<LinearAnimationBase>()->loopValue(value);
					break;
				case LinearAnimationBase::workStartPropertyKey:
					object->as<LinearAnimationBase>()->workStart(value);
					break;
				case LinearAnimationBase::workEndPropertyKey:
					object->as<LinearAnimationBase>()->workEnd(value);
					break;
				case KeyFrameDrawOrderValueBase::drawableIdPropertyKey:
					object->as<KeyFrameDrawOrderValueBase>()->drawableId(value);
					break;
				case KeyFrameDrawOrderValueBase::valuePropertyKey:
					object->as<KeyFrameDrawOrderValueBase>()->value(value);
					break;
				case ComponentBase::parentIdPropertyKey:
					object->as<ComponentBase>()->parentId(value);
					break;
				case StrokeBase::capPropertyKey:
					object->as<StrokeBase>()->cap(value);
					break;
				case StrokeBase::joinPropertyKey:
					object->as<StrokeBase>()->join(value);
					break;
				case FillBase::fillRulePropertyKey:
					object->as<FillBase>()->fillRule(value);
					break;
				case DrawableBase::drawOrderPropertyKey:
					object->as<DrawableBase>()->drawOrder(value);
					break;
				case DrawableBase::blendModeValuePropertyKey:
					object->as<DrawableBase>()->blendModeValue(value);
					break;
				case ClippingShapeBase::shapeIdPropertyKey:
					object->as<ClippingShapeBase>()->shapeId(value);
					break;
				case ClippingShapeBase::clipOpValuePropertyKey:
					object->as<ClippingShapeBase>()->clipOpValue(value);
					break;
				case WeightBase::valuesPropertyKey:
					object->as<WeightBase>()->values(value);
					break;
				case WeightBase::indicesPropertyKey:
					object->as<WeightBase>()->indices(value);
					break;
				case TendonBase::boneIdPropertyKey:
					object->as<TendonBase>()->boneId(value);
					break;
				case CubicWeightBase::inValuesPropertyKey:
					object->as<CubicWeightBase>()->inValues(value);
					break;
				case CubicWeightBase::inIndicesPropertyKey:
					object->as<CubicWeightBase>()->inIndices(value);
					break;
				case CubicWeightBase::outValuesPropertyKey:
					object->as<CubicWeightBase>()->outValues(value);
					break;
				case CubicWeightBase::outIndicesPropertyKey:
					object->as<CubicWeightBase>()->outIndices(value);
					break;
			}
		}
		static void setString(Core* object, int propertyKey, std::string value)
		{
			switch (propertyKey)
			{
				case AnimationBase::namePropertyKey:
					object->as<AnimationBase>()->name(value);
					break;
				case ComponentBase::namePropertyKey:
					object->as<ComponentBase>()->name(value);
					break;
			}
		}
		static void setDouble(Core* object, int propertyKey, float value)
		{
			switch (propertyKey)
			{
				case CubicInterpolatorBase::x1PropertyKey:
					object->as<CubicInterpolatorBase>()->x1(value);
					break;
				case CubicInterpolatorBase::y1PropertyKey:
					object->as<CubicInterpolatorBase>()->y1(value);
					break;
				case CubicInterpolatorBase::x2PropertyKey:
					object->as<CubicInterpolatorBase>()->x2(value);
					break;
				case CubicInterpolatorBase::y2PropertyKey:
					object->as<CubicInterpolatorBase>()->y2(value);
					break;
				case KeyFrameDoubleBase::valuePropertyKey:
					object->as<KeyFrameDoubleBase>()->value(value);
					break;
				case LinearAnimationBase::speedPropertyKey:
					object->as<LinearAnimationBase>()->speed(value);
					break;
				case LinearGradientBase::startXPropertyKey:
					object->as<LinearGradientBase>()->startX(value);
					break;
				case LinearGradientBase::startYPropertyKey:
					object->as<LinearGradientBase>()->startY(value);
					break;
				case LinearGradientBase::endXPropertyKey:
					object->as<LinearGradientBase>()->endX(value);
					break;
				case LinearGradientBase::endYPropertyKey:
					object->as<LinearGradientBase>()->endY(value);
					break;
				case LinearGradientBase::opacityPropertyKey:
					object->as<LinearGradientBase>()->opacity(value);
					break;
				case StrokeBase::thicknessPropertyKey:
					object->as<StrokeBase>()->thickness(value);
					break;
				case GradientStopBase::positionPropertyKey:
					object->as<GradientStopBase>()->position(value);
					break;
				case TransformComponentBase::rotationPropertyKey:
					object->as<TransformComponentBase>()->rotation(value);
					break;
				case TransformComponentBase::scaleXPropertyKey:
					object->as<TransformComponentBase>()->scaleX(value);
					break;
				case TransformComponentBase::scaleYPropertyKey:
					object->as<TransformComponentBase>()->scaleY(value);
					break;
				case TransformComponentBase::opacityPropertyKey:
					object->as<TransformComponentBase>()->opacity(value);
					break;
				case NodeBase::xPropertyKey:
					object->as<NodeBase>()->x(value);
					break;
				case NodeBase::yPropertyKey:
					object->as<NodeBase>()->y(value);
					break;
				case PathVertexBase::xPropertyKey:
					object->as<PathVertexBase>()->x(value);
					break;
				case PathVertexBase::yPropertyKey:
					object->as<PathVertexBase>()->y(value);
					break;
				case StraightVertexBase::radiusPropertyKey:
					object->as<StraightVertexBase>()->radius(value);
					break;
				case CubicAsymmetricVertexBase::rotationPropertyKey:
					object->as<CubicAsymmetricVertexBase>()->rotation(value);
					break;
				case CubicAsymmetricVertexBase::inDistancePropertyKey:
					object->as<CubicAsymmetricVertexBase>()->inDistance(value);
					break;
				case CubicAsymmetricVertexBase::outDistancePropertyKey:
					object->as<CubicAsymmetricVertexBase>()->outDistance(value);
					break;
				case ParametricPathBase::widthPropertyKey:
					object->as<ParametricPathBase>()->width(value);
					break;
				case ParametricPathBase::heightPropertyKey:
					object->as<ParametricPathBase>()->height(value);
					break;
				case RectangleBase::cornerRadiusPropertyKey:
					object->as<RectangleBase>()->cornerRadius(value);
					break;
				case CubicMirroredVertexBase::rotationPropertyKey:
					object->as<CubicMirroredVertexBase>()->rotation(value);
					break;
				case CubicMirroredVertexBase::distancePropertyKey:
					object->as<CubicMirroredVertexBase>()->distance(value);
					break;
				case CubicDetachedVertexBase::inRotationPropertyKey:
					object->as<CubicDetachedVertexBase>()->inRotation(value);
					break;
				case CubicDetachedVertexBase::inDistancePropertyKey:
					object->as<CubicDetachedVertexBase>()->inDistance(value);
					break;
				case CubicDetachedVertexBase::outRotationPropertyKey:
					object->as<CubicDetachedVertexBase>()->outRotation(value);
					break;
				case CubicDetachedVertexBase::outDistancePropertyKey:
					object->as<CubicDetachedVertexBase>()->outDistance(value);
					break;
				case ArtboardBase::widthPropertyKey:
					object->as<ArtboardBase>()->width(value);
					break;
				case ArtboardBase::heightPropertyKey:
					object->as<ArtboardBase>()->height(value);
					break;
				case ArtboardBase::xPropertyKey:
					object->as<ArtboardBase>()->x(value);
					break;
				case ArtboardBase::yPropertyKey:
					object->as<ArtboardBase>()->y(value);
					break;
				case ArtboardBase::originXPropertyKey:
					object->as<ArtboardBase>()->originX(value);
					break;
				case ArtboardBase::originYPropertyKey:
					object->as<ArtboardBase>()->originY(value);
					break;
				case BoneBase::lengthPropertyKey:
					object->as<BoneBase>()->length(value);
					break;
				case RootBoneBase::xPropertyKey:
					object->as<RootBoneBase>()->x(value);
					break;
				case RootBoneBase::yPropertyKey:
					object->as<RootBoneBase>()->y(value);
					break;
				case SkinBase::xxPropertyKey:
					object->as<SkinBase>()->xx(value);
					break;
				case SkinBase::yxPropertyKey:
					object->as<SkinBase>()->yx(value);
					break;
				case SkinBase::xyPropertyKey:
					object->as<SkinBase>()->xy(value);
					break;
				case SkinBase::yyPropertyKey:
					object->as<SkinBase>()->yy(value);
					break;
				case SkinBase::txPropertyKey:
					object->as<SkinBase>()->tx(value);
					break;
				case SkinBase::tyPropertyKey:
					object->as<SkinBase>()->ty(value);
					break;
				case TendonBase::xxPropertyKey:
					object->as<TendonBase>()->xx(value);
					break;
				case TendonBase::yxPropertyKey:
					object->as<TendonBase>()->yx(value);
					break;
				case TendonBase::xyPropertyKey:
					object->as<TendonBase>()->xy(value);
					break;
				case TendonBase::yyPropertyKey:
					object->as<TendonBase>()->yy(value);
					break;
				case TendonBase::txPropertyKey:
					object->as<TendonBase>()->tx(value);
					break;
				case TendonBase::tyPropertyKey:
					object->as<TendonBase>()->ty(value);
					break;
			}
		}
		static void setColor(Core* object, int propertyKey, int value)
		{
			switch (propertyKey)
			{
				case KeyFrameColorBase::valuePropertyKey:
					object->as<KeyFrameColorBase>()->value(value);
					break;
				case SolidColorBase::colorValuePropertyKey:
					object->as<SolidColorBase>()->colorValue(value);
					break;
				case GradientStopBase::colorValuePropertyKey:
					object->as<GradientStopBase>()->colorValue(value);
					break;
			}
		}
		static void setBool(Core* object, int propertyKey, bool value)
		{
			switch (propertyKey)
			{
				case LinearAnimationBase::enableWorkAreaPropertyKey:
					object->as<LinearAnimationBase>()->enableWorkArea(value);
					break;
				case ShapePaintBase::isVisiblePropertyKey:
					object->as<ShapePaintBase>()->isVisible(value);
					break;
				case StrokeBase::transformAffectsStrokePropertyKey:
					object->as<StrokeBase>()->transformAffectsStroke(value);
					break;
				case PointsPathBase::isClosedPropertyKey:
					object->as<PointsPathBase>()->isClosed(value);
					break;
				case ClippingShapeBase::isVisiblePropertyKey:
					object->as<ClippingShapeBase>()->isVisible(value);
					break;
			}
		}
		static int getUint(Core* object, int propertyKey)
		{
			switch (propertyKey)
			{
				case KeyedObjectBase::objectIdPropertyKey:
					return object->as<KeyedObjectBase>()->objectId();
				case KeyedPropertyBase::propertyKeyPropertyKey:
					return object->as<KeyedPropertyBase>()->propertyKey();
				case KeyFrameBase::framePropertyKey:
					return object->as<KeyFrameBase>()->frame();
				case KeyFrameBase::interpolationTypePropertyKey:
					return object->as<KeyFrameBase>()->interpolationType();
				case KeyFrameBase::interpolatorIdPropertyKey:
					return object->as<KeyFrameBase>()->interpolatorId();
				case LinearAnimationBase::fpsPropertyKey:
					return object->as<LinearAnimationBase>()->fps();
				case LinearAnimationBase::durationPropertyKey:
					return object->as<LinearAnimationBase>()->duration();
				case LinearAnimationBase::loopValuePropertyKey:
					return object->as<LinearAnimationBase>()->loopValue();
				case LinearAnimationBase::workStartPropertyKey:
					return object->as<LinearAnimationBase>()->workStart();
				case LinearAnimationBase::workEndPropertyKey:
					return object->as<LinearAnimationBase>()->workEnd();
				case KeyFrameDrawOrderValueBase::drawableIdPropertyKey:
					return object->as<KeyFrameDrawOrderValueBase>()
					    ->drawableId();
				case KeyFrameDrawOrderValueBase::valuePropertyKey:
					return object->as<KeyFrameDrawOrderValueBase>()->value();
				case ComponentBase::parentIdPropertyKey:
					return object->as<ComponentBase>()->parentId();
				case StrokeBase::capPropertyKey:
					return object->as<StrokeBase>()->cap();
				case StrokeBase::joinPropertyKey:
					return object->as<StrokeBase>()->join();
				case FillBase::fillRulePropertyKey:
					return object->as<FillBase>()->fillRule();
				case DrawableBase::drawOrderPropertyKey:
					return object->as<DrawableBase>()->drawOrder();
				case DrawableBase::blendModeValuePropertyKey:
					return object->as<DrawableBase>()->blendModeValue();
				case ClippingShapeBase::shapeIdPropertyKey:
					return object->as<ClippingShapeBase>()->shapeId();
				case ClippingShapeBase::clipOpValuePropertyKey:
					return object->as<ClippingShapeBase>()->clipOpValue();
				case WeightBase::valuesPropertyKey:
					return object->as<WeightBase>()->values();
				case WeightBase::indicesPropertyKey:
					return object->as<WeightBase>()->indices();
				case TendonBase::boneIdPropertyKey:
					return object->as<TendonBase>()->boneId();
				case CubicWeightBase::inValuesPropertyKey:
					return object->as<CubicWeightBase>()->inValues();
				case CubicWeightBase::inIndicesPropertyKey:
					return object->as<CubicWeightBase>()->inIndices();
				case CubicWeightBase::outValuesPropertyKey:
					return object->as<CubicWeightBase>()->outValues();
				case CubicWeightBase::outIndicesPropertyKey:
					return object->as<CubicWeightBase>()->outIndices();
			}
			return 0;
		}
		static std::string getString(Core* object, int propertyKey)
		{
			switch (propertyKey)
			{
				case AnimationBase::namePropertyKey:
					return object->as<AnimationBase>()->name();
				case ComponentBase::namePropertyKey:
					return object->as<ComponentBase>()->name();
			}
			return "";
		}
		static float getDouble(Core* object, int propertyKey)
		{
			switch (propertyKey)
			{
				case CubicInterpolatorBase::x1PropertyKey:
					return object->as<CubicInterpolatorBase>()->x1();
				case CubicInterpolatorBase::y1PropertyKey:
					return object->as<CubicInterpolatorBase>()->y1();
				case CubicInterpolatorBase::x2PropertyKey:
					return object->as<CubicInterpolatorBase>()->x2();
				case CubicInterpolatorBase::y2PropertyKey:
					return object->as<CubicInterpolatorBase>()->y2();
				case KeyFrameDoubleBase::valuePropertyKey:
					return object->as<KeyFrameDoubleBase>()->value();
				case LinearAnimationBase::speedPropertyKey:
					return object->as<LinearAnimationBase>()->speed();
				case LinearGradientBase::startXPropertyKey:
					return object->as<LinearGradientBase>()->startX();
				case LinearGradientBase::startYPropertyKey:
					return object->as<LinearGradientBase>()->startY();
				case LinearGradientBase::endXPropertyKey:
					return object->as<LinearGradientBase>()->endX();
				case LinearGradientBase::endYPropertyKey:
					return object->as<LinearGradientBase>()->endY();
				case LinearGradientBase::opacityPropertyKey:
					return object->as<LinearGradientBase>()->opacity();
				case StrokeBase::thicknessPropertyKey:
					return object->as<StrokeBase>()->thickness();
				case GradientStopBase::positionPropertyKey:
					return object->as<GradientStopBase>()->position();
				case TransformComponentBase::rotationPropertyKey:
					return object->as<TransformComponentBase>()->rotation();
				case TransformComponentBase::scaleXPropertyKey:
					return object->as<TransformComponentBase>()->scaleX();
				case TransformComponentBase::scaleYPropertyKey:
					return object->as<TransformComponentBase>()->scaleY();
				case TransformComponentBase::opacityPropertyKey:
					return object->as<TransformComponentBase>()->opacity();
				case NodeBase::xPropertyKey:
					return object->as<NodeBase>()->x();
				case NodeBase::yPropertyKey:
					return object->as<NodeBase>()->y();
				case PathVertexBase::xPropertyKey:
					return object->as<PathVertexBase>()->x();
				case PathVertexBase::yPropertyKey:
					return object->as<PathVertexBase>()->y();
				case StraightVertexBase::radiusPropertyKey:
					return object->as<StraightVertexBase>()->radius();
				case CubicAsymmetricVertexBase::rotationPropertyKey:
					return object->as<CubicAsymmetricVertexBase>()->rotation();
				case CubicAsymmetricVertexBase::inDistancePropertyKey:
					return object->as<CubicAsymmetricVertexBase>()
					    ->inDistance();
				case CubicAsymmetricVertexBase::outDistancePropertyKey:
					return object->as<CubicAsymmetricVertexBase>()
					    ->outDistance();
				case ParametricPathBase::widthPropertyKey:
					return object->as<ParametricPathBase>()->width();
				case ParametricPathBase::heightPropertyKey:
					return object->as<ParametricPathBase>()->height();
				case RectangleBase::cornerRadiusPropertyKey:
					return object->as<RectangleBase>()->cornerRadius();
				case CubicMirroredVertexBase::rotationPropertyKey:
					return object->as<CubicMirroredVertexBase>()->rotation();
				case CubicMirroredVertexBase::distancePropertyKey:
					return object->as<CubicMirroredVertexBase>()->distance();
				case CubicDetachedVertexBase::inRotationPropertyKey:
					return object->as<CubicDetachedVertexBase>()->inRotation();
				case CubicDetachedVertexBase::inDistancePropertyKey:
					return object->as<CubicDetachedVertexBase>()->inDistance();
				case CubicDetachedVertexBase::outRotationPropertyKey:
					return object->as<CubicDetachedVertexBase>()->outRotation();
				case CubicDetachedVertexBase::outDistancePropertyKey:
					return object->as<CubicDetachedVertexBase>()->outDistance();
				case ArtboardBase::widthPropertyKey:
					return object->as<ArtboardBase>()->width();
				case ArtboardBase::heightPropertyKey:
					return object->as<ArtboardBase>()->height();
				case ArtboardBase::xPropertyKey:
					return object->as<ArtboardBase>()->x();
				case ArtboardBase::yPropertyKey:
					return object->as<ArtboardBase>()->y();
				case ArtboardBase::originXPropertyKey:
					return object->as<ArtboardBase>()->originX();
				case ArtboardBase::originYPropertyKey:
					return object->as<ArtboardBase>()->originY();
				case BoneBase::lengthPropertyKey:
					return object->as<BoneBase>()->length();
				case RootBoneBase::xPropertyKey:
					return object->as<RootBoneBase>()->x();
				case RootBoneBase::yPropertyKey:
					return object->as<RootBoneBase>()->y();
				case SkinBase::xxPropertyKey:
					return object->as<SkinBase>()->xx();
				case SkinBase::yxPropertyKey:
					return object->as<SkinBase>()->yx();
				case SkinBase::xyPropertyKey:
					return object->as<SkinBase>()->xy();
				case SkinBase::yyPropertyKey:
					return object->as<SkinBase>()->yy();
				case SkinBase::txPropertyKey:
					return object->as<SkinBase>()->tx();
				case SkinBase::tyPropertyKey:
					return object->as<SkinBase>()->ty();
				case TendonBase::xxPropertyKey:
					return object->as<TendonBase>()->xx();
				case TendonBase::yxPropertyKey:
					return object->as<TendonBase>()->yx();
				case TendonBase::xyPropertyKey:
					return object->as<TendonBase>()->xy();
				case TendonBase::yyPropertyKey:
					return object->as<TendonBase>()->yy();
				case TendonBase::txPropertyKey:
					return object->as<TendonBase>()->tx();
				case TendonBase::tyPropertyKey:
					return object->as<TendonBase>()->ty();
			}
			return 0.0f;
		}
		static int getColor(Core* object, int propertyKey)
		{
			switch (propertyKey)
			{
				case KeyFrameColorBase::valuePropertyKey:
					return object->as<KeyFrameColorBase>()->value();
				case SolidColorBase::colorValuePropertyKey:
					return object->as<SolidColorBase>()->colorValue();
				case GradientStopBase::colorValuePropertyKey:
					return object->as<GradientStopBase>()->colorValue();
			}
			return 0;
		}
		static bool getBool(Core* object, int propertyKey)
		{
			switch (propertyKey)
			{
				case LinearAnimationBase::enableWorkAreaPropertyKey:
					return object->as<LinearAnimationBase>()->enableWorkArea();
				case ShapePaintBase::isVisiblePropertyKey:
					return object->as<ShapePaintBase>()->isVisible();
				case StrokeBase::transformAffectsStrokePropertyKey:
					return object->as<StrokeBase>()->transformAffectsStroke();
				case PointsPathBase::isClosedPropertyKey:
					return object->as<PointsPathBase>()->isClosed();
				case ClippingShapeBase::isVisiblePropertyKey:
					return object->as<ClippingShapeBase>()->isVisible();
			}
			return false;
		}
	};
} // namespace rive

#endif