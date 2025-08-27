#ifndef _RIVE_CORE_REGISTRY_HPP_
#define _RIVE_CORE_REGISTRY_HPP_
#include "rive/animation/advanceable_state.hpp"
#include "rive/animation/animation.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/blend_animation.hpp"
#include "rive/animation/blend_animation_1d.hpp"
#include "rive/animation/blend_animation_direct.hpp"
#include "rive/animation/blend_state.hpp"
#include "rive/animation/blend_state_1d.hpp"
#include "rive/animation/blend_state_1d_input.hpp"
#include "rive/animation/blend_state_1d_viewmodel.hpp"
#include "rive/animation/blend_state_direct.hpp"
#include "rive/animation/blend_state_transition.hpp"
#include "rive/animation/cubic_ease_interpolator.hpp"
#include "rive/animation/cubic_interpolator.hpp"
#include "rive/animation/cubic_interpolator_component.hpp"
#include "rive/animation/cubic_value_interpolator.hpp"
#include "rive/animation/elastic_interpolator.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/exit_state.hpp"
#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe.hpp"
#include "rive/animation/keyframe_bool.hpp"
#include "rive/animation/keyframe_callback.hpp"
#include "rive/animation/keyframe_color.hpp"
#include "rive/animation/keyframe_double.hpp"
#include "rive/animation/keyframe_id.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/animation/keyframe_string.hpp"
#include "rive/animation/keyframe_uint.hpp"
#include "rive/animation/layer_state.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/listener_action.hpp"
#include "rive/animation/listener_align_target.hpp"
#include "rive/animation/listener_bool_change.hpp"
#include "rive/animation/listener_fire_event.hpp"
#include "rive/animation/listener_input_change.hpp"
#include "rive/animation/listener_number_change.hpp"
#include "rive/animation/listener_trigger_change.hpp"
#include "rive/animation/listener_viewmodel_change.hpp"
#include "rive/animation/nested_bool.hpp"
#include "rive/animation/nested_input.hpp"
#include "rive/animation/nested_linear_animation.hpp"
#include "rive/animation/nested_number.hpp"
#include "rive/animation/nested_remap_animation.hpp"
#include "rive/animation/nested_simple_animation.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/nested_trigger.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_component.hpp"
#include "rive/animation/state_machine_fire_action.hpp"
#include "rive/animation/state_machine_fire_event.hpp"
#include "rive/animation/state_machine_fire_trigger.hpp"
#include "rive/animation/state_machine_input.hpp"
#include "rive/animation/state_machine_layer.hpp"
#include "rive/animation/state_machine_layer_component.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/transition_artboard_condition.hpp"
#include "rive/animation/transition_bool_condition.hpp"
#include "rive/animation/transition_comparator.hpp"
#include "rive/animation/transition_condition.hpp"
#include "rive/animation/transition_input_condition.hpp"
#include "rive/animation/transition_number_condition.hpp"
#include "rive/animation/transition_property_artboard_comparator.hpp"
#include "rive/animation/transition_property_comparator.hpp"
#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/animation/transition_self_comparator.hpp"
#include "rive/animation/transition_trigger_condition.hpp"
#include "rive/animation/transition_value_asset_comparator.hpp"
#include "rive/animation/transition_value_boolean_comparator.hpp"
#include "rive/animation/transition_value_color_comparator.hpp"
#include "rive/animation/transition_value_comparator.hpp"
#include "rive/animation/transition_value_condition.hpp"
#include "rive/animation/transition_value_enum_comparator.hpp"
#include "rive/animation/transition_value_id_comparator.hpp"
#include "rive/animation/transition_value_number_comparator.hpp"
#include "rive/animation/transition_value_string_comparator.hpp"
#include "rive/animation/transition_value_trigger_comparator.hpp"
#include "rive/animation/transition_viewmodel_condition.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/assets/asset.hpp"
#include "rive/assets/audio_asset.hpp"
#include "rive/assets/drawable_asset.hpp"
#include "rive/assets/export_audio.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/folder.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/audio_event.hpp"
#include "rive/backboard.hpp"
#include "rive/bones/bone.hpp"
#include "rive/bones/cubic_weight.hpp"
#include "rive/bones/root_bone.hpp"
#include "rive/bones/skeletal_component.hpp"
#include "rive/bones/skin.hpp"
#include "rive/bones/tendon.hpp"
#include "rive/bones/weight.hpp"
#include "rive/component.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/constraints/distance_constraint.hpp"
#include "rive/constraints/draggable_constraint.hpp"
#include "rive/constraints/follow_path_constraint.hpp"
#include "rive/constraints/ik_constraint.hpp"
#include "rive/constraints/rotation_constraint.hpp"
#include "rive/constraints/scale_constraint.hpp"
#include "rive/constraints/scrolling/clamped_scroll_physics.hpp"
#include "rive/constraints/scrolling/elastic_scroll_physics.hpp"
#include "rive/constraints/scrolling/scroll_bar_constraint.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/constraints/scrolling/scroll_physics.hpp"
#include "rive/constraints/targeted_constraint.hpp"
#include "rive/constraints/transform_component_constraint.hpp"
#include "rive/constraints/transform_component_constraint_y.hpp"
#include "rive/constraints/transform_constraint.hpp"
#include "rive/constraints/transform_space_constraint.hpp"
#include "rive/constraints/translation_constraint.hpp"
#include "rive/container_component.hpp"
#include "rive/custom_property.hpp"
#include "rive/custom_property_boolean.hpp"
#include "rive/custom_property_color.hpp"
#include "rive/custom_property_enum.hpp"
#include "rive/custom_property_group.hpp"
#include "rive/custom_property_number.hpp"
#include "rive/custom_property_string.hpp"
#include "rive/custom_property_trigger.hpp"
#include "rive/data_bind/bindable_property.hpp"
#include "rive/data_bind/bindable_property_artboard.hpp"
#include "rive/data_bind/bindable_property_asset.hpp"
#include "rive/data_bind/bindable_property_boolean.hpp"
#include "rive/data_bind/bindable_property_color.hpp"
#include "rive/data_bind/bindable_property_enum.hpp"
#include "rive/data_bind/bindable_property_id.hpp"
#include "rive/data_bind/bindable_property_integer.hpp"
#include "rive/data_bind/bindable_property_list.hpp"
#include "rive/data_bind/bindable_property_number.hpp"
#include "rive/data_bind/bindable_property_string.hpp"
#include "rive/data_bind/bindable_property_trigger.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/converters/data_converter_boolean_negate.hpp"
#include "rive/data_bind/converters/data_converter_formula.hpp"
#include "rive/data_bind/converters/data_converter_group.hpp"
#include "rive/data_bind/converters/data_converter_group_item.hpp"
#include "rive/data_bind/converters/data_converter_interpolator.hpp"
#include "rive/data_bind/converters/data_converter_list_to_length.hpp"
#include "rive/data_bind/converters/data_converter_number_to_list.hpp"
#include "rive/data_bind/converters/data_converter_operation.hpp"
#include "rive/data_bind/converters/data_converter_operation_value.hpp"
#include "rive/data_bind/converters/data_converter_operation_viewmodel.hpp"
#include "rive/data_bind/converters/data_converter_range_mapper.hpp"
#include "rive/data_bind/converters/data_converter_rounder.hpp"
#include "rive/data_bind/converters/data_converter_string_pad.hpp"
#include "rive/data_bind/converters/data_converter_string_remove_zeros.hpp"
#include "rive/data_bind/converters/data_converter_string_trim.hpp"
#include "rive/data_bind/converters/data_converter_system_degs_to_rads.hpp"
#include "rive/data_bind/converters/data_converter_system_normalizer.hpp"
#include "rive/data_bind/converters/data_converter_to_number.hpp"
#include "rive/data_bind/converters/data_converter_to_string.hpp"
#include "rive/data_bind/converters/data_converter_trigger.hpp"
#include "rive/data_bind/converters/formula/formula_token.hpp"
#include "rive/data_bind/converters/formula/formula_token_argument_separator.hpp"
#include "rive/data_bind/converters/formula/formula_token_function.hpp"
#include "rive/data_bind/converters/formula/formula_token_input.hpp"
#include "rive/data_bind/converters/formula/formula_token_operation.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis_close.hpp"
#include "rive/data_bind/converters/formula/formula_token_parenthesis_open.hpp"
#include "rive/data_bind/converters/formula/formula_token_value.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_context.hpp"
#include "rive/draw_rules.hpp"
#include "rive/draw_target.hpp"
#include "rive/drawable.hpp"
#include "rive/event.hpp"
#include "rive/foreground_layout_drawable.hpp"
#include "rive/joystick.hpp"
#include "rive/layout/artboard_component_list_override.hpp"
#include "rive/layout/axis.hpp"
#include "rive/layout/axis_x.hpp"
#include "rive/layout/axis_y.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/n_sliced_node.hpp"
#include "rive/layout/n_slicer.hpp"
#include "rive/layout/n_slicer_tile_mode.hpp"
#include "rive/layout_component.hpp"
#include "rive/nested_animation.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/nested_artboard_leaf.hpp"
#include "rive/node.hpp"
#include "rive/open_url_event.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/contour_mesh_vertex.hpp"
#include "rive/shapes/cubic_asymmetric_vertex.hpp"
#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/shapes/cubic_mirrored_vertex.hpp"
#include "rive/shapes/cubic_vertex.hpp"
#include "rive/shapes/ellipse.hpp"
#include "rive/shapes/image.hpp"
#include "rive/shapes/list_path.hpp"
#include "rive/shapes/mesh.hpp"
#include "rive/shapes/mesh_vertex.hpp"
#include "rive/shapes/paint/dash.hpp"
#include "rive/shapes/paint/dash_path.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/gradient_stop.hpp"
#include "rive/shapes/paint/linear_gradient.hpp"
#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/trim_path.hpp"
#include "rive/shapes/parametric_path.hpp"
#include "rive/shapes/path.hpp"
#include "rive/shapes/path_vertex.hpp"
#include "rive/shapes/points_common_path.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/polygon.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/star.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include "rive/shapes/triangle.hpp"
#include "rive/shapes/vertex.hpp"
#include "rive/solo.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_follow_path_modifier.hpp"
#include "rive/text/text_input.hpp"
#include "rive/text/text_input_cursor.hpp"
#include "rive/text/text_input_drawable.hpp"
#include "rive/text/text_input_selected_text.hpp"
#include "rive/text/text_input_selection.hpp"
#include "rive/text/text_input_text.hpp"
#include "rive/text/text_modifier.hpp"
#include "rive/text/text_modifier_group.hpp"
#include "rive/text/text_modifier_range.hpp"
#include "rive/text/text_shape_modifier.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_style_axis.hpp"
#include "rive/text/text_style_feature.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_target_modifier.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text/text_variation_modifier.hpp"
#include "rive/transform_component.hpp"
#include "rive/viewmodel/data_enum.hpp"
#include "rive/viewmodel/data_enum_custom.hpp"
#include "rive/viewmodel/data_enum_system.hpp"
#include "rive/viewmodel/data_enum_value.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_component.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_asset.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_image.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
#include "rive/viewmodel/viewmodel_property_artboard.hpp"
#include "rive/viewmodel/viewmodel_property_asset.hpp"
#include "rive/viewmodel/viewmodel_property_asset_image.hpp"
#include "rive/viewmodel/viewmodel_property_boolean.hpp"
#include "rive/viewmodel/viewmodel_property_color.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"
#include "rive/viewmodel/viewmodel_property_enum_custom.hpp"
#include "rive/viewmodel/viewmodel_property_enum_system.hpp"
#include "rive/viewmodel/viewmodel_property_list.hpp"
#include "rive/viewmodel/viewmodel_property_number.hpp"
#include "rive/viewmodel/viewmodel_property_string.hpp"
#include "rive/viewmodel/viewmodel_property_symbol.hpp"
#include "rive/viewmodel/viewmodel_property_symbol_list_index.hpp"
#include "rive/viewmodel/viewmodel_property_trigger.hpp"
#include "rive/viewmodel/viewmodel_property_viewmodel.hpp"
#include "rive/world_transform_component.hpp"
namespace rive
{
class CoreRegistry
{
public:
    static Core* makeCoreInstance(int typeKey)
    {
        switch (typeKey)
        {
            case ViewModelInstanceListItemBase::typeKey:
                return new ViewModelInstanceListItem();
            case ViewModelComponentBase::typeKey:
                return new ViewModelComponent();
            case ViewModelPropertyBase::typeKey:
                return new ViewModelProperty();
            case ViewModelPropertyArtboardBase::typeKey:
                return new ViewModelPropertyArtboard();
            case ViewModelInstanceColorBase::typeKey:
                return new ViewModelInstanceColor();
            case ViewModelPropertyEnumBase::typeKey:
                return new ViewModelPropertyEnum();
            case ViewModelPropertyEnumCustomBase::typeKey:
                return new ViewModelPropertyEnumCustom();
            case DataEnumBase::typeKey:
                return new DataEnum();
            case DataEnumCustomBase::typeKey:
                return new DataEnumCustom();
            case ViewModelPropertyNumberBase::typeKey:
                return new ViewModelPropertyNumber();
            case ViewModelInstanceEnumBase::typeKey:
                return new ViewModelInstanceEnum();
            case ViewModelPropertySymbolListIndexBase::typeKey:
                return new ViewModelPropertySymbolListIndex();
            case ViewModelInstanceArtboardBase::typeKey:
                return new ViewModelInstanceArtboard();
            case ViewModelInstanceStringBase::typeKey:
                return new ViewModelInstanceString();
            case ViewModelPropertyListBase::typeKey:
                return new ViewModelPropertyList();
            case ViewModelPropertyEnumSystemBase::typeKey:
                return new ViewModelPropertyEnumSystem();
            case ViewModelBase::typeKey:
                return new ViewModel();
            case ViewModelPropertyAssetBase::typeKey:
                return new ViewModelPropertyAsset();
            case DataEnumSystemBase::typeKey:
                return new DataEnumSystem();
            case ViewModelPropertyViewModelBase::typeKey:
                return new ViewModelPropertyViewModel();
            case ViewModelInstanceBase::typeKey:
                return new ViewModelInstance();
            case ViewModelPropertyBooleanBase::typeKey:
                return new ViewModelPropertyBoolean();
            case ViewModelPropertyColorBase::typeKey:
                return new ViewModelPropertyColor();
            case ViewModelPropertyAssetImageBase::typeKey:
                return new ViewModelPropertyAssetImage();
            case ViewModelInstanceBooleanBase::typeKey:
                return new ViewModelInstanceBoolean();
            case ViewModelInstanceListBase::typeKey:
                return new ViewModelInstanceList();
            case ViewModelInstanceNumberBase::typeKey:
                return new ViewModelInstanceNumber();
            case ViewModelInstanceTriggerBase::typeKey:
                return new ViewModelInstanceTrigger();
            case ViewModelInstanceSymbolListIndexBase::typeKey:
                return new ViewModelInstanceSymbolListIndex();
            case ViewModelPropertyStringBase::typeKey:
                return new ViewModelPropertyString();
            case ViewModelInstanceViewModelBase::typeKey:
                return new ViewModelInstanceViewModel();
            case ViewModelPropertyTriggerBase::typeKey:
                return new ViewModelPropertyTrigger();
            case ViewModelInstanceAssetBase::typeKey:
                return new ViewModelInstanceAsset();
            case ViewModelInstanceAssetImageBase::typeKey:
                return new ViewModelInstanceAssetImage();
            case DataEnumValueBase::typeKey:
                return new DataEnumValue();
            case DrawTargetBase::typeKey:
                return new DrawTarget();
            case CustomPropertyNumberBase::typeKey:
                return new CustomPropertyNumber();
            case DistanceConstraintBase::typeKey:
                return new DistanceConstraint();
            case IKConstraintBase::typeKey:
                return new IKConstraint();
            case FollowPathConstraintBase::typeKey:
                return new FollowPathConstraint();
            case TranslationConstraintBase::typeKey:
                return new TranslationConstraint();
            case ClampedScrollPhysicsBase::typeKey:
                return new ClampedScrollPhysics();
            case ScrollConstraintBase::typeKey:
                return new ScrollConstraint();
            case ElasticScrollPhysicsBase::typeKey:
                return new ElasticScrollPhysics();
            case ScrollBarConstraintBase::typeKey:
                return new ScrollBarConstraint();
            case TransformConstraintBase::typeKey:
                return new TransformConstraint();
            case ScaleConstraintBase::typeKey:
                return new ScaleConstraint();
            case RotationConstraintBase::typeKey:
                return new RotationConstraint();
            case NodeBase::typeKey:
                return new Node();
            case ForegroundLayoutDrawableBase::typeKey:
                return new ForegroundLayoutDrawable();
            case NestedArtboardBase::typeKey:
                return new NestedArtboard();
            case ArtboardComponentListBase::typeKey:
                return new ArtboardComponentList();
            case CustomPropertyColorBase::typeKey:
                return new CustomPropertyColor();
            case SoloBase::typeKey:
                return new Solo();
            case NestedArtboardLayoutBase::typeKey:
                return new NestedArtboardLayout();
            case NSlicerTileModeBase::typeKey:
                return new NSlicerTileMode();
            case AxisYBase::typeKey:
                return new AxisY();
            case LayoutComponentStyleBase::typeKey:
                return new LayoutComponentStyle();
            case AxisXBase::typeKey:
                return new AxisX();
            case NSlicerBase::typeKey:
                return new NSlicer();
            case NSlicedNodeBase::typeKey:
                return new NSlicedNode();
            case ArtboardComponentListOverrideBase::typeKey:
                return new ArtboardComponentListOverride();
            case ListenerFireEventBase::typeKey:
                return new ListenerFireEvent();
            case TransitionSelfComparatorBase::typeKey:
                return new TransitionSelfComparator();
            case StateMachineFireTriggerBase::typeKey:
                return new StateMachineFireTrigger();
            case TransitionValueTriggerComparatorBase::typeKey:
                return new TransitionValueTriggerComparator();
            case KeyFrameUintBase::typeKey:
                return new KeyFrameUint();
            case NestedSimpleAnimationBase::typeKey:
                return new NestedSimpleAnimation();
            case AnimationStateBase::typeKey:
                return new AnimationState();
            case NestedTriggerBase::typeKey:
                return new NestedTrigger();
            case KeyedObjectBase::typeKey:
                return new KeyedObject();
            case AnimationBase::typeKey:
                return new Animation();
            case BlendAnimationDirectBase::typeKey:
                return new BlendAnimationDirect();
            case StateMachineNumberBase::typeKey:
                return new StateMachineNumber();
            case CubicValueInterpolatorBase::typeKey:
                return new CubicValueInterpolator();
            case TransitionTriggerConditionBase::typeKey:
                return new TransitionTriggerCondition();
            case KeyedPropertyBase::typeKey:
                return new KeyedProperty();
            case StateMachineListenerBase::typeKey:
                return new StateMachineListener();
            case TransitionPropertyArtboardComparatorBase::typeKey:
                return new TransitionPropertyArtboardComparator();
            case TransitionPropertyViewModelComparatorBase::typeKey:
                return new TransitionPropertyViewModelComparator();
            case KeyFrameIdBase::typeKey:
                return new KeyFrameId();
            case KeyFrameBoolBase::typeKey:
                return new KeyFrameBool();
            case ListenerBoolChangeBase::typeKey:
                return new ListenerBoolChange();
            case ListenerAlignTargetBase::typeKey:
                return new ListenerAlignTarget();
            case TransitionNumberConditionBase::typeKey:
                return new TransitionNumberCondition();
            case TransitionValueBooleanComparatorBase::typeKey:
                return new TransitionValueBooleanComparator();
            case TransitionViewModelConditionBase::typeKey:
                return new TransitionViewModelCondition();
            case TransitionArtboardConditionBase::typeKey:
                return new TransitionArtboardCondition();
            case AnyStateBase::typeKey:
                return new AnyState();
            case BlendState1DInputBase::typeKey:
                return new BlendState1DInput();
            case CubicInterpolatorComponentBase::typeKey:
                return new CubicInterpolatorComponent();
            case StateMachineLayerBase::typeKey:
                return new StateMachineLayer();
            case KeyFrameStringBase::typeKey:
                return new KeyFrameString();
            case ListenerNumberChangeBase::typeKey:
                return new ListenerNumberChange();
            case CubicEaseInterpolatorBase::typeKey:
                return new CubicEaseInterpolator();
            case TransitionValueIdComparatorBase::typeKey:
                return new TransitionValueIdComparator();
            case StateTransitionBase::typeKey:
                return new StateTransition();
            case NestedBoolBase::typeKey:
                return new NestedBool();
            case KeyFrameDoubleBase::typeKey:
                return new KeyFrameDouble();
            case KeyFrameColorBase::typeKey:
                return new KeyFrameColor();
            case StateMachineBase::typeKey:
                return new StateMachine();
            case StateMachineFireEventBase::typeKey:
                return new StateMachineFireEvent();
            case EntryStateBase::typeKey:
                return new EntryState();
            case LinearAnimationBase::typeKey:
                return new LinearAnimation();
            case StateMachineTriggerBase::typeKey:
                return new StateMachineTrigger();
            case TransitionValueColorComparatorBase::typeKey:
                return new TransitionValueColorComparator();
            case ListenerTriggerChangeBase::typeKey:
                return new ListenerTriggerChange();
            case BlendStateDirectBase::typeKey:
                return new BlendStateDirect();
            case ListenerViewModelChangeBase::typeKey:
                return new ListenerViewModelChange();
            case TransitionValueNumberComparatorBase::typeKey:
                return new TransitionValueNumberComparator();
            case NestedStateMachineBase::typeKey:
                return new NestedStateMachine();
            case ElasticInterpolatorBase::typeKey:
                return new ElasticInterpolator();
            case ExitStateBase::typeKey:
                return new ExitState();
            case NestedNumberBase::typeKey:
                return new NestedNumber();
            case TransitionValueEnumComparatorBase::typeKey:
                return new TransitionValueEnumComparator();
            case KeyFrameCallbackBase::typeKey:
                return new KeyFrameCallback();
            case TransitionValueStringComparatorBase::typeKey:
                return new TransitionValueStringComparator();
            case NestedRemapAnimationBase::typeKey:
                return new NestedRemapAnimation();
            case TransitionValueAssetComparatorBase::typeKey:
                return new TransitionValueAssetComparator();
            case TransitionBoolConditionBase::typeKey:
                return new TransitionBoolCondition();
            case BlendState1DViewModelBase::typeKey:
                return new BlendState1DViewModel();
            case BlendStateTransitionBase::typeKey:
                return new BlendStateTransition();
            case StateMachineBoolBase::typeKey:
                return new StateMachineBool();
            case BlendAnimation1DBase::typeKey:
                return new BlendAnimation1D();
            case DashPathBase::typeKey:
                return new DashPath();
            case LinearGradientBase::typeKey:
                return new LinearGradient();
            case RadialGradientBase::typeKey:
                return new RadialGradient();
            case DashBase::typeKey:
                return new Dash();
            case StrokeBase::typeKey:
                return new Stroke();
            case SolidColorBase::typeKey:
                return new SolidColor();
            case GradientStopBase::typeKey:
                return new GradientStop();
            case FeatherBase::typeKey:
                return new Feather();
            case TrimPathBase::typeKey:
                return new TrimPath();
            case FillBase::typeKey:
                return new Fill();
            case MeshVertexBase::typeKey:
                return new MeshVertex();
            case ShapeBase::typeKey:
                return new Shape();
            case StraightVertexBase::typeKey:
                return new StraightVertex();
            case CubicAsymmetricVertexBase::typeKey:
                return new CubicAsymmetricVertex();
            case MeshBase::typeKey:
                return new Mesh();
            case PointsPathBase::typeKey:
                return new PointsPath();
            case ContourMeshVertexBase::typeKey:
                return new ContourMeshVertex();
            case RectangleBase::typeKey:
                return new Rectangle();
            case CubicMirroredVertexBase::typeKey:
                return new CubicMirroredVertex();
            case TriangleBase::typeKey:
                return new Triangle();
            case EllipseBase::typeKey:
                return new Ellipse();
            case ListPathBase::typeKey:
                return new ListPath();
            case ClippingShapeBase::typeKey:
                return new ClippingShape();
            case PolygonBase::typeKey:
                return new Polygon();
            case StarBase::typeKey:
                return new Star();
            case ImageBase::typeKey:
                return new Image();
            case CubicDetachedVertexBase::typeKey:
                return new CubicDetachedVertex();
            case CustomPropertyGroupBase::typeKey:
                return new CustomPropertyGroup();
            case EventBase::typeKey:
                return new Event();
            case DrawRulesBase::typeKey:
                return new DrawRules();
            case CustomPropertyBooleanBase::typeKey:
                return new CustomPropertyBoolean();
            case LayoutComponentBase::typeKey:
                return new LayoutComponent();
            case ArtboardBase::typeKey:
                return new Artboard();
            case JoystickBase::typeKey:
                return new Joystick();
            case BackboardBase::typeKey:
                return new Backboard();
            case OpenUrlEventBase::typeKey:
                return new OpenUrlEvent();
            case BindablePropertyArtboardBase::typeKey:
                return new BindablePropertyArtboard();
            case BindablePropertyIntegerBase::typeKey:
                return new BindablePropertyInteger();
            case BindablePropertyTriggerBase::typeKey:
                return new BindablePropertyTrigger();
            case BindablePropertyBooleanBase::typeKey:
                return new BindablePropertyBoolean();
            case DataBindBase::typeKey:
                return new DataBind();
            case BindablePropertyAssetBase::typeKey:
                return new BindablePropertyAsset();
            case DataConverterNumberToListBase::typeKey:
                return new DataConverterNumberToList();
            case DataConverterFormulaBase::typeKey:
                return new DataConverterFormula();
            case DataConverterToNumberBase::typeKey:
                return new DataConverterToNumber();
            case DataConverterOperationBase::typeKey:
                return new DataConverterOperation();
            case DataConverterOperationValueBase::typeKey:
                return new DataConverterOperationValue();
            case DataConverterSystemDegsToRadsBase::typeKey:
                return new DataConverterSystemDegsToRads();
            case DataConverterRangeMapperBase::typeKey:
                return new DataConverterRangeMapper();
            case DataConverterInterpolatorBase::typeKey:
                return new DataConverterInterpolator();
            case DataConverterSystemNormalizerBase::typeKey:
                return new DataConverterSystemNormalizer();
            case DataConverterListToLengthBase::typeKey:
                return new DataConverterListToLength();
            case DataConverterGroupItemBase::typeKey:
                return new DataConverterGroupItem();
            case DataConverterGroupBase::typeKey:
                return new DataConverterGroup();
            case DataConverterStringRemoveZerosBase::typeKey:
                return new DataConverterStringRemoveZeros();
            case DataConverterRounderBase::typeKey:
                return new DataConverterRounder();
            case DataConverterStringPadBase::typeKey:
                return new DataConverterStringPad();
            case DataConverterTriggerBase::typeKey:
                return new DataConverterTrigger();
            case DataConverterStringTrimBase::typeKey:
                return new DataConverterStringTrim();
            case FormulaTokenBase::typeKey:
                return new FormulaToken();
            case FormulaTokenArgumentSeparatorBase::typeKey:
                return new FormulaTokenArgumentSeparator();
            case FormulaTokenParenthesisBase::typeKey:
                return new FormulaTokenParenthesis();
            case FormulaTokenParenthesisCloseBase::typeKey:
                return new FormulaTokenParenthesisClose();
            case FormulaTokenOperationBase::typeKey:
                return new FormulaTokenOperation();
            case FormulaTokenFunctionBase::typeKey:
                return new FormulaTokenFunction();
            case FormulaTokenValueBase::typeKey:
                return new FormulaTokenValue();
            case FormulaTokenParenthesisOpenBase::typeKey:
                return new FormulaTokenParenthesisOpen();
            case FormulaTokenInputBase::typeKey:
                return new FormulaTokenInput();
            case DataConverterOperationViewModelBase::typeKey:
                return new DataConverterOperationViewModel();
            case DataConverterBooleanNegateBase::typeKey:
                return new DataConverterBooleanNegate();
            case DataConverterToStringBase::typeKey:
                return new DataConverterToString();
            case DataBindContextBase::typeKey:
                return new DataBindContext();
            case BindablePropertyListBase::typeKey:
                return new BindablePropertyList();
            case BindablePropertyStringBase::typeKey:
                return new BindablePropertyString();
            case BindablePropertyNumberBase::typeKey:
                return new BindablePropertyNumber();
            case BindablePropertyEnumBase::typeKey:
                return new BindablePropertyEnum();
            case BindablePropertyColorBase::typeKey:
                return new BindablePropertyColor();
            case NestedArtboardLeafBase::typeKey:
                return new NestedArtboardLeaf();
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
            case TextModifierRangeBase::typeKey:
                return new TextModifierRange();
            case TextFollowPathModifierBase::typeKey:
                return new TextFollowPathModifier();
            case TextInputCursorBase::typeKey:
                return new TextInputCursor();
            case TextInputTextBase::typeKey:
                return new TextInputText();
            case TextStyleFeatureBase::typeKey:
                return new TextStyleFeature();
            case TextVariationModifierBase::typeKey:
                return new TextVariationModifier();
            case TextModifierGroupBase::typeKey:
                return new TextModifierGroup();
            case TextStyleBase::typeKey:
                return new TextStyle();
            case TextStylePaintBase::typeKey:
                return new TextStylePaint();
            case TextInputSelectedTextBase::typeKey:
                return new TextInputSelectedText();
            case TextInputBase::typeKey:
                return new TextInput();
            case TextStyleAxisBase::typeKey:
                return new TextStyleAxis();
            case TextInputSelectionBase::typeKey:
                return new TextInputSelection();
            case TextBase::typeKey:
                return new Text();
            case TextValueRunBase::typeKey:
                return new TextValueRun();
            case CustomPropertyEnumBase::typeKey:
                return new CustomPropertyEnum();
            case CustomPropertyStringBase::typeKey:
                return new CustomPropertyString();
            case FolderBase::typeKey:
                return new Folder();
            case ImageAssetBase::typeKey:
                return new ImageAsset();
            case FontAssetBase::typeKey:
                return new FontAsset();
            case AudioAssetBase::typeKey:
                return new AudioAsset();
            case FileAssetContentsBase::typeKey:
                return new FileAssetContents();
            case AudioEventBase::typeKey:
                return new AudioEvent();
            case CustomPropertyTriggerBase::typeKey:
                return new CustomPropertyTrigger();
        }
        return nullptr;
    }
    static void setUint(Core* object, int propertyKey, uint32_t value)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceListItemBase::viewModelIdPropertyKey:
                object->as<ViewModelInstanceListItemBase>()->viewModelId(value);
                break;
            case ViewModelInstanceListItemBase::viewModelInstanceIdPropertyKey:
                object->as<ViewModelInstanceListItemBase>()
                    ->viewModelInstanceId(value);
                break;
            case ViewModelPropertyBase::symbolTypeValuePropertyKey:
                object->as<ViewModelPropertyBase>()->symbolTypeValue(value);
                break;
            case ViewModelInstanceValueBase::viewModelPropertyIdPropertyKey:
                object->as<ViewModelInstanceValueBase>()->viewModelPropertyId(
                    value);
                break;
            case ViewModelPropertyEnumCustomBase::enumIdPropertyKey:
                object->as<ViewModelPropertyEnumCustomBase>()->enumId(value);
                break;
            case ViewModelInstanceEnumBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceEnumBase>()->propertyValue(value);
                break;
            case ViewModelInstanceArtboardBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceArtboardBase>()->propertyValue(
                    value);
                break;
            case ViewModelPropertyEnumSystemBase::enumTypePropertyKey:
                object->as<ViewModelPropertyEnumSystemBase>()->enumType(value);
                break;
            case DataEnumSystemBase::enumTypePropertyKey:
                object->as<DataEnumSystemBase>()->enumType(value);
                break;
            case ViewModelPropertyViewModelBase::
                viewModelReferenceIdPropertyKey:
                object->as<ViewModelPropertyViewModelBase>()
                    ->viewModelReferenceId(value);
                break;
            case ComponentBase::parentIdPropertyKey:
                object->as<ComponentBase>()->parentId(value);
                break;
            case ViewModelInstanceBase::viewModelIdPropertyKey:
                object->as<ViewModelInstanceBase>()->viewModelId(value);
                break;
            case ViewModelInstanceTriggerBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceTriggerBase>()->propertyValue(
                    value);
                break;
            case ViewModelInstanceSymbolListIndexBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceSymbolListIndexBase>()
                    ->propertyValue(value);
                break;
            case ViewModelInstanceViewModelBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceViewModelBase>()->propertyValue(
                    value);
                break;
            case ViewModelInstanceAssetBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceAssetBase>()->propertyValue(value);
                break;
            case DrawTargetBase::drawableIdPropertyKey:
                object->as<DrawTargetBase>()->drawableId(value);
                break;
            case DrawTargetBase::placementValuePropertyKey:
                object->as<DrawTargetBase>()->placementValue(value);
                break;
            case TargetedConstraintBase::targetIdPropertyKey:
                object->as<TargetedConstraintBase>()->targetId(value);
                break;
            case DistanceConstraintBase::modeValuePropertyKey:
                object->as<DistanceConstraintBase>()->modeValue(value);
                break;
            case TransformSpaceConstraintBase::sourceSpaceValuePropertyKey:
                object->as<TransformSpaceConstraintBase>()->sourceSpaceValue(
                    value);
                break;
            case TransformSpaceConstraintBase::destSpaceValuePropertyKey:
                object->as<TransformSpaceConstraintBase>()->destSpaceValue(
                    value);
                break;
            case TransformComponentConstraintBase::minMaxSpaceValuePropertyKey:
                object->as<TransformComponentConstraintBase>()
                    ->minMaxSpaceValue(value);
                break;
            case IKConstraintBase::parentBoneCountPropertyKey:
                object->as<IKConstraintBase>()->parentBoneCount(value);
                break;
            case ScrollPhysicsBase::constraintIdPropertyKey:
                object->as<ScrollPhysicsBase>()->constraintId(value);
                break;
            case DraggableConstraintBase::directionValuePropertyKey:
                object->as<DraggableConstraintBase>()->directionValue(value);
                break;
            case ScrollConstraintBase::physicsTypeValuePropertyKey:
                object->as<ScrollConstraintBase>()->physicsTypeValue(value);
                break;
            case ScrollConstraintBase::physicsIdPropertyKey:
                object->as<ScrollConstraintBase>()->physicsId(value);
                break;
            case ScrollBarConstraintBase::scrollConstraintIdPropertyKey:
                object->as<ScrollBarConstraintBase>()->scrollConstraintId(
                    value);
                break;
            case DrawableBase::blendModeValuePropertyKey:
                object->as<DrawableBase>()->blendModeValue(value);
                break;
            case DrawableBase::drawableFlagsPropertyKey:
                object->as<DrawableBase>()->drawableFlags(value);
                break;
            case NestedArtboardBase::artboardIdPropertyKey:
                object->as<NestedArtboardBase>()->artboardId(value);
                break;
            case ArtboardComponentListBase::listSourcePropertyKey:
                object->as<ArtboardComponentListBase>()->listSource(value);
                break;
            case NestedAnimationBase::animationIdPropertyKey:
                object->as<NestedAnimationBase>()->animationId(value);
                break;
            case SoloBase::activeComponentIdPropertyKey:
                object->as<SoloBase>()->activeComponentId(value);
                break;
            case NestedArtboardLayoutBase::instanceWidthUnitsValuePropertyKey:
                object->as<NestedArtboardLayoutBase>()->instanceWidthUnitsValue(
                    value);
                break;
            case NestedArtboardLayoutBase::instanceHeightUnitsValuePropertyKey:
                object->as<NestedArtboardLayoutBase>()
                    ->instanceHeightUnitsValue(value);
                break;
            case NestedArtboardLayoutBase::instanceWidthScaleTypePropertyKey:
                object->as<NestedArtboardLayoutBase>()->instanceWidthScaleType(
                    value);
                break;
            case NestedArtboardLayoutBase::instanceHeightScaleTypePropertyKey:
                object->as<NestedArtboardLayoutBase>()->instanceHeightScaleType(
                    value);
                break;
            case NSlicerTileModeBase::patchIndexPropertyKey:
                object->as<NSlicerTileModeBase>()->patchIndex(value);
                break;
            case NSlicerTileModeBase::stylePropertyKey:
                object->as<NSlicerTileModeBase>()->style(value);
                break;
            case LayoutComponentStyleBase::flexBasisUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->flexBasisUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::layoutWidthScaleTypePropertyKey:
                object->as<LayoutComponentStyleBase>()->layoutWidthScaleType(
                    value);
                break;
            case LayoutComponentStyleBase::layoutHeightScaleTypePropertyKey:
                object->as<LayoutComponentStyleBase>()->layoutHeightScaleType(
                    value);
                break;
            case LayoutComponentStyleBase::layoutAlignmentTypePropertyKey:
                object->as<LayoutComponentStyleBase>()->layoutAlignmentType(
                    value);
                break;
            case LayoutComponentStyleBase::animationStyleTypePropertyKey:
                object->as<LayoutComponentStyleBase>()->animationStyleType(
                    value);
                break;
            case LayoutComponentStyleBase::interpolationTypePropertyKey:
                object->as<LayoutComponentStyleBase>()->interpolationType(
                    value);
                break;
            case LayoutComponentStyleBase::interpolatorIdPropertyKey:
                object->as<LayoutComponentStyleBase>()->interpolatorId(value);
                break;
            case LayoutComponentStyleBase::displayValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->displayValue(value);
                break;
            case LayoutComponentStyleBase::positionTypeValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->positionTypeValue(
                    value);
                break;
            case LayoutComponentStyleBase::flexDirectionValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->flexDirectionValue(
                    value);
                break;
            case LayoutComponentStyleBase::directionValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->directionValue(value);
                break;
            case LayoutComponentStyleBase::alignContentValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->alignContentValue(
                    value);
                break;
            case LayoutComponentStyleBase::alignItemsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->alignItemsValue(value);
                break;
            case LayoutComponentStyleBase::alignSelfValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->alignSelfValue(value);
                break;
            case LayoutComponentStyleBase::justifyContentValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->justifyContentValue(
                    value);
                break;
            case LayoutComponentStyleBase::flexWrapValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->flexWrapValue(value);
                break;
            case LayoutComponentStyleBase::overflowValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->overflowValue(value);
                break;
            case LayoutComponentStyleBase::widthUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->widthUnitsValue(value);
                break;
            case LayoutComponentStyleBase::heightUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->heightUnitsValue(value);
                break;
            case LayoutComponentStyleBase::borderLeftUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->borderLeftUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::borderRightUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->borderRightUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::borderTopUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->borderTopUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::borderBottomUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->borderBottomUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::marginLeftUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->marginLeftUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::marginRightUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->marginRightUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::marginTopUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->marginTopUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::marginBottomUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->marginBottomUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::paddingLeftUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->paddingLeftUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::paddingRightUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->paddingRightUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::paddingTopUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->paddingTopUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::paddingBottomUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->paddingBottomUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::positionLeftUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->positionLeftUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::positionRightUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->positionRightUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::positionTopUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->positionTopUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::positionBottomUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()
                    ->positionBottomUnitsValue(value);
                break;
            case LayoutComponentStyleBase::gapHorizontalUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->gapHorizontalUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::gapVerticalUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->gapVerticalUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::minWidthUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->minWidthUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::minHeightUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->minHeightUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::maxWidthUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->maxWidthUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::maxHeightUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->maxHeightUnitsValue(
                    value);
                break;
            case ArtboardComponentListOverrideBase::artboardIdPropertyKey:
                object->as<ArtboardComponentListOverrideBase>()->artboardId(
                    value);
                break;
            case ArtboardComponentListOverrideBase::
                instanceWidthUnitsValuePropertyKey:
                object->as<ArtboardComponentListOverrideBase>()
                    ->instanceWidthUnitsValue(value);
                break;
            case ArtboardComponentListOverrideBase::
                instanceHeightUnitsValuePropertyKey:
                object->as<ArtboardComponentListOverrideBase>()
                    ->instanceHeightUnitsValue(value);
                break;
            case ArtboardComponentListOverrideBase::
                instanceWidthScaleTypePropertyKey:
                object->as<ArtboardComponentListOverrideBase>()
                    ->instanceWidthScaleType(value);
                break;
            case ArtboardComponentListOverrideBase::
                instanceHeightScaleTypePropertyKey:
                object->as<ArtboardComponentListOverrideBase>()
                    ->instanceHeightScaleType(value);
                break;
            case ListenerFireEventBase::eventIdPropertyKey:
                object->as<ListenerFireEventBase>()->eventId(value);
                break;
            case LayerStateBase::flagsPropertyKey:
                object->as<LayerStateBase>()->flags(value);
                break;
            case StateMachineFireActionBase::occursValuePropertyKey:
                object->as<StateMachineFireActionBase>()->occursValue(value);
                break;
            case TransitionValueTriggerComparatorBase::valuePropertyKey:
                object->as<TransitionValueTriggerComparatorBase>()->value(
                    value);
                break;
            case KeyFrameBase::framePropertyKey:
                object->as<KeyFrameBase>()->frame(value);
                break;
            case InterpolatingKeyFrameBase::interpolationTypePropertyKey:
                object->as<InterpolatingKeyFrameBase>()->interpolationType(
                    value);
                break;
            case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
                object->as<InterpolatingKeyFrameBase>()->interpolatorId(value);
                break;
            case KeyFrameUintBase::valuePropertyKey:
                object->as<KeyFrameUintBase>()->value(value);
                break;
            case ListenerInputChangeBase::inputIdPropertyKey:
                object->as<ListenerInputChangeBase>()->inputId(value);
                break;
            case ListenerInputChangeBase::nestedInputIdPropertyKey:
                object->as<ListenerInputChangeBase>()->nestedInputId(value);
                break;
            case AnimationStateBase::animationIdPropertyKey:
                object->as<AnimationStateBase>()->animationId(value);
                break;
            case NestedInputBase::inputIdPropertyKey:
                object->as<NestedInputBase>()->inputId(value);
                break;
            case KeyedObjectBase::objectIdPropertyKey:
                object->as<KeyedObjectBase>()->objectId(value);
                break;
            case BlendAnimationBase::animationIdPropertyKey:
                object->as<BlendAnimationBase>()->animationId(value);
                break;
            case BlendAnimationDirectBase::inputIdPropertyKey:
                object->as<BlendAnimationDirectBase>()->inputId(value);
                break;
            case BlendAnimationDirectBase::blendSourcePropertyKey:
                object->as<BlendAnimationDirectBase>()->blendSource(value);
                break;
            case TransitionInputConditionBase::inputIdPropertyKey:
                object->as<TransitionInputConditionBase>()->inputId(value);
                break;
            case KeyedPropertyBase::propertyKeyPropertyKey:
                object->as<KeyedPropertyBase>()->propertyKey(value);
                break;
            case StateMachineListenerBase::targetIdPropertyKey:
                object->as<StateMachineListenerBase>()->targetId(value);
                break;
            case StateMachineListenerBase::listenerTypeValuePropertyKey:
                object->as<StateMachineListenerBase>()->listenerTypeValue(
                    value);
                break;
            case StateMachineListenerBase::eventIdPropertyKey:
                object->as<StateMachineListenerBase>()->eventId(value);
                break;
            case TransitionPropertyArtboardComparatorBase::
                propertyTypePropertyKey:
                object->as<TransitionPropertyArtboardComparatorBase>()
                    ->propertyType(value);
                break;
            case KeyFrameIdBase::valuePropertyKey:
                object->as<KeyFrameIdBase>()->value(value);
                break;
            case ListenerBoolChangeBase::valuePropertyKey:
                object->as<ListenerBoolChangeBase>()->value(value);
                break;
            case ListenerAlignTargetBase::targetIdPropertyKey:
                object->as<ListenerAlignTargetBase>()->targetId(value);
                break;
            case TransitionValueConditionBase::opValuePropertyKey:
                object->as<TransitionValueConditionBase>()->opValue(value);
                break;
            case TransitionViewModelConditionBase::opValuePropertyKey:
                object->as<TransitionViewModelConditionBase>()->opValue(value);
                break;
            case BlendState1DInputBase::inputIdPropertyKey:
                object->as<BlendState1DInputBase>()->inputId(value);
                break;
            case TransitionValueIdComparatorBase::valuePropertyKey:
                object->as<TransitionValueIdComparatorBase>()->value(value);
                break;
            case StateTransitionBase::stateToIdPropertyKey:
                object->as<StateTransitionBase>()->stateToId(value);
                break;
            case StateTransitionBase::flagsPropertyKey:
                object->as<StateTransitionBase>()->flags(value);
                break;
            case StateTransitionBase::durationPropertyKey:
                object->as<StateTransitionBase>()->duration(value);
                break;
            case StateTransitionBase::exitTimePropertyKey:
                object->as<StateTransitionBase>()->exitTime(value);
                break;
            case StateTransitionBase::interpolationTypePropertyKey:
                object->as<StateTransitionBase>()->interpolationType(value);
                break;
            case StateTransitionBase::interpolatorIdPropertyKey:
                object->as<StateTransitionBase>()->interpolatorId(value);
                break;
            case StateTransitionBase::randomWeightPropertyKey:
                object->as<StateTransitionBase>()->randomWeight(value);
                break;
            case StateMachineFireEventBase::eventIdPropertyKey:
                object->as<StateMachineFireEventBase>()->eventId(value);
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
            case ElasticInterpolatorBase::easingValuePropertyKey:
                object->as<ElasticInterpolatorBase>()->easingValue(value);
                break;
            case BlendStateTransitionBase::exitBlendAnimationIdPropertyKey:
                object->as<BlendStateTransitionBase>()->exitBlendAnimationId(
                    value);
                break;
            case ShapePaintBase::blendModeValuePropertyKey:
                object->as<ShapePaintBase>()->blendModeValue(value);
                break;
            case StrokeBase::capPropertyKey:
                object->as<StrokeBase>()->cap(value);
                break;
            case StrokeBase::joinPropertyKey:
                object->as<StrokeBase>()->join(value);
                break;
            case FeatherBase::spaceValuePropertyKey:
                object->as<FeatherBase>()->spaceValue(value);
                break;
            case TrimPathBase::modeValuePropertyKey:
                object->as<TrimPathBase>()->modeValue(value);
                break;
            case FillBase::fillRulePropertyKey:
                object->as<FillBase>()->fillRule(value);
                break;
            case PathBase::pathFlagsPropertyKey:
                object->as<PathBase>()->pathFlags(value);
                break;
            case ListPathBase::listSourcePropertyKey:
                object->as<ListPathBase>()->listSource(value);
                break;
            case ClippingShapeBase::sourceIdPropertyKey:
                object->as<ClippingShapeBase>()->sourceId(value);
                break;
            case ClippingShapeBase::fillRulePropertyKey:
                object->as<ClippingShapeBase>()->fillRule(value);
                break;
            case PolygonBase::pointsPropertyKey:
                object->as<PolygonBase>()->points(value);
                break;
            case ImageBase::assetIdPropertyKey:
                object->as<ImageBase>()->assetId(value);
                break;
            case DrawRulesBase::drawTargetIdPropertyKey:
                object->as<DrawRulesBase>()->drawTargetId(value);
                break;
            case LayoutComponentBase::styleIdPropertyKey:
                object->as<LayoutComponentBase>()->styleId(value);
                break;
            case ArtboardBase::defaultStateMachineIdPropertyKey:
                object->as<ArtboardBase>()->defaultStateMachineId(value);
                break;
            case ArtboardBase::viewModelIdPropertyKey:
                object->as<ArtboardBase>()->viewModelId(value);
                break;
            case JoystickBase::xIdPropertyKey:
                object->as<JoystickBase>()->xId(value);
                break;
            case JoystickBase::yIdPropertyKey:
                object->as<JoystickBase>()->yId(value);
                break;
            case JoystickBase::joystickFlagsPropertyKey:
                object->as<JoystickBase>()->joystickFlags(value);
                break;
            case JoystickBase::handleSourceIdPropertyKey:
                object->as<JoystickBase>()->handleSourceId(value);
                break;
            case OpenUrlEventBase::targetValuePropertyKey:
                object->as<OpenUrlEventBase>()->targetValue(value);
                break;
            case BindablePropertyIdBase::propertyValuePropertyKey:
                object->as<BindablePropertyIdBase>()->propertyValue(value);
                break;
            case BindablePropertyIntegerBase::propertyValuePropertyKey:
                object->as<BindablePropertyIntegerBase>()->propertyValue(value);
                break;
            case DataBindBase::propertyKeyPropertyKey:
                object->as<DataBindBase>()->propertyKey(value);
                break;
            case DataBindBase::flagsPropertyKey:
                object->as<DataBindBase>()->flags(value);
                break;
            case DataBindBase::converterIdPropertyKey:
                object->as<DataBindBase>()->converterId(value);
                break;
            case DataConverterNumberToListBase::viewModelIdPropertyKey:
                object->as<DataConverterNumberToListBase>()->viewModelId(value);
                break;
            case DataConverterOperationBase::operationTypePropertyKey:
                object->as<DataConverterOperationBase>()->operationType(value);
                break;
            case DataConverterRangeMapperBase::interpolationTypePropertyKey:
                object->as<DataConverterRangeMapperBase>()->interpolationType(
                    value);
                break;
            case DataConverterRangeMapperBase::interpolatorIdPropertyKey:
                object->as<DataConverterRangeMapperBase>()->interpolatorId(
                    value);
                break;
            case DataConverterRangeMapperBase::flagsPropertyKey:
                object->as<DataConverterRangeMapperBase>()->flags(value);
                break;
            case DataConverterInterpolatorBase::interpolationTypePropertyKey:
                object->as<DataConverterInterpolatorBase>()->interpolationType(
                    value);
                break;
            case DataConverterInterpolatorBase::interpolatorIdPropertyKey:
                object->as<DataConverterInterpolatorBase>()->interpolatorId(
                    value);
                break;
            case DataConverterGroupItemBase::converterIdPropertyKey:
                object->as<DataConverterGroupItemBase>()->converterId(value);
                break;
            case DataConverterRounderBase::decimalsPropertyKey:
                object->as<DataConverterRounderBase>()->decimals(value);
                break;
            case DataConverterStringPadBase::lengthPropertyKey:
                object->as<DataConverterStringPadBase>()->length(value);
                break;
            case DataConverterStringPadBase::padTypePropertyKey:
                object->as<DataConverterStringPadBase>()->padType(value);
                break;
            case DataConverterStringTrimBase::trimTypePropertyKey:
                object->as<DataConverterStringTrimBase>()->trimType(value);
                break;
            case FormulaTokenOperationBase::operationTypePropertyKey:
                object->as<FormulaTokenOperationBase>()->operationType(value);
                break;
            case FormulaTokenFunctionBase::functionTypePropertyKey:
                object->as<FormulaTokenFunctionBase>()->functionType(value);
                break;
            case DataConverterToStringBase::flagsPropertyKey:
                object->as<DataConverterToStringBase>()->flags(value);
                break;
            case DataConverterToStringBase::decimalsPropertyKey:
                object->as<DataConverterToStringBase>()->decimals(value);
                break;
            case BindablePropertyListBase::propertyValuePropertyKey:
                object->as<BindablePropertyListBase>()->propertyValue(value);
                break;
            case BindablePropertyEnumBase::propertyValuePropertyKey:
                object->as<BindablePropertyEnumBase>()->propertyValue(value);
                break;
            case NestedArtboardLeafBase::fitPropertyKey:
                object->as<NestedArtboardLeafBase>()->fit(value);
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
            case TextModifierRangeBase::unitsValuePropertyKey:
                object->as<TextModifierRangeBase>()->unitsValue(value);
                break;
            case TextModifierRangeBase::typeValuePropertyKey:
                object->as<TextModifierRangeBase>()->typeValue(value);
                break;
            case TextModifierRangeBase::modeValuePropertyKey:
                object->as<TextModifierRangeBase>()->modeValue(value);
                break;
            case TextModifierRangeBase::runIdPropertyKey:
                object->as<TextModifierRangeBase>()->runId(value);
                break;
            case TextTargetModifierBase::targetIdPropertyKey:
                object->as<TextTargetModifierBase>()->targetId(value);
                break;
            case TextStyleFeatureBase::tagPropertyKey:
                object->as<TextStyleFeatureBase>()->tag(value);
                break;
            case TextStyleFeatureBase::featureValuePropertyKey:
                object->as<TextStyleFeatureBase>()->featureValue(value);
                break;
            case TextVariationModifierBase::axisTagPropertyKey:
                object->as<TextVariationModifierBase>()->axisTag(value);
                break;
            case TextModifierGroupBase::modifierFlagsPropertyKey:
                object->as<TextModifierGroupBase>()->modifierFlags(value);
                break;
            case TextStyleBase::fontAssetIdPropertyKey:
                object->as<TextStyleBase>()->fontAssetId(value);
                break;
            case TextStyleAxisBase::tagPropertyKey:
                object->as<TextStyleAxisBase>()->tag(value);
                break;
            case TextBase::alignValuePropertyKey:
                object->as<TextBase>()->alignValue(value);
                break;
            case TextBase::sizingValuePropertyKey:
                object->as<TextBase>()->sizingValue(value);
                break;
            case TextBase::overflowValuePropertyKey:
                object->as<TextBase>()->overflowValue(value);
                break;
            case TextBase::originValuePropertyKey:
                object->as<TextBase>()->originValue(value);
                break;
            case TextBase::wrapValuePropertyKey:
                object->as<TextBase>()->wrapValue(value);
                break;
            case TextBase::verticalAlignValuePropertyKey:
                object->as<TextBase>()->verticalAlignValue(value);
                break;
            case TextValueRunBase::styleIdPropertyKey:
                object->as<TextValueRunBase>()->styleId(value);
                break;
            case CustomPropertyEnumBase::propertyValuePropertyKey:
                object->as<CustomPropertyEnumBase>()->propertyValue(value);
                break;
            case CustomPropertyEnumBase::enumIdPropertyKey:
                object->as<CustomPropertyEnumBase>()->enumId(value);
                break;
            case FileAssetBase::assetIdPropertyKey:
                object->as<FileAssetBase>()->assetId(value);
                break;
            case AudioEventBase::assetIdPropertyKey:
                object->as<AudioEventBase>()->assetId(value);
                break;
            case CustomPropertyTriggerBase::propertyValuePropertyKey:
                object->as<CustomPropertyTriggerBase>()->propertyValue(value);
                break;
        }
    }
    static void setString(Core* object, int propertyKey, std::string value)
    {
        switch (propertyKey)
        {
            case ViewModelComponentBase::namePropertyKey:
                object->as<ViewModelComponentBase>()->name(value);
                break;
            case DataEnumCustomBase::namePropertyKey:
                object->as<DataEnumCustomBase>()->name(value);
                break;
            case ViewModelInstanceStringBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceStringBase>()->propertyValue(value);
                break;
            case ComponentBase::namePropertyKey:
                object->as<ComponentBase>()->name(value);
                break;
            case DataEnumValueBase::keyPropertyKey:
                object->as<DataEnumValueBase>()->key(value);
                break;
            case DataEnumValueBase::valuePropertyKey:
                object->as<DataEnumValueBase>()->value(value);
                break;
            case AnimationBase::namePropertyKey:
                object->as<AnimationBase>()->name(value);
                break;
            case StateMachineComponentBase::namePropertyKey:
                object->as<StateMachineComponentBase>()->name(value);
                break;
            case KeyFrameStringBase::valuePropertyKey:
                object->as<KeyFrameStringBase>()->value(value);
                break;
            case TransitionValueStringComparatorBase::valuePropertyKey:
                object->as<TransitionValueStringComparatorBase>()->value(value);
                break;
            case OpenUrlEventBase::urlPropertyKey:
                object->as<OpenUrlEventBase>()->url(value);
                break;
            case DataConverterBase::namePropertyKey:
                object->as<DataConverterBase>()->name(value);
                break;
            case DataConverterStringPadBase::textPropertyKey:
                object->as<DataConverterStringPadBase>()->text(value);
                break;
            case DataConverterToStringBase::colorFormatPropertyKey:
                object->as<DataConverterToStringBase>()->colorFormat(value);
                break;
            case BindablePropertyStringBase::propertyValuePropertyKey:
                object->as<BindablePropertyStringBase>()->propertyValue(value);
                break;
            case TextInputBase::textPropertyKey:
                object->as<TextInputBase>()->text(value);
                break;
            case TextValueRunBase::textPropertyKey:
                object->as<TextValueRunBase>()->text(value);
                break;
            case CustomPropertyStringBase::propertyValuePropertyKey:
                object->as<CustomPropertyStringBase>()->propertyValue(value);
                break;
            case AssetBase::namePropertyKey:
                object->as<AssetBase>()->name(value);
                break;
            case FileAssetBase::cdnBaseUrlPropertyKey:
                object->as<FileAssetBase>()->cdnBaseUrl(value);
                break;
        }
    }
    static void setColor(Core* object, int propertyKey, int value)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceColorBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceColorBase>()->propertyValue(value);
                break;
            case CustomPropertyColorBase::propertyValuePropertyKey:
                object->as<CustomPropertyColorBase>()->propertyValue(value);
                break;
            case KeyFrameColorBase::valuePropertyKey:
                object->as<KeyFrameColorBase>()->value(value);
                break;
            case TransitionValueColorComparatorBase::valuePropertyKey:
                object->as<TransitionValueColorComparatorBase>()->value(value);
                break;
            case SolidColorBase::colorValuePropertyKey:
                object->as<SolidColorBase>()->colorValue(value);
                break;
            case GradientStopBase::colorValuePropertyKey:
                object->as<GradientStopBase>()->colorValue(value);
                break;
            case BindablePropertyColorBase::propertyValuePropertyKey:
                object->as<BindablePropertyColorBase>()->propertyValue(value);
                break;
        }
    }
    static void setBool(Core* object, int propertyKey, bool value)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceBooleanBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceBooleanBase>()->propertyValue(
                    value);
                break;
            case TransformComponentConstraintBase::offsetPropertyKey:
                object->as<TransformComponentConstraintBase>()->offset(value);
                break;
            case TransformComponentConstraintBase::doesCopyPropertyKey:
                object->as<TransformComponentConstraintBase>()->doesCopy(value);
                break;
            case TransformComponentConstraintBase::minPropertyKey:
                object->as<TransformComponentConstraintBase>()->min(value);
                break;
            case TransformComponentConstraintBase::maxPropertyKey:
                object->as<TransformComponentConstraintBase>()->max(value);
                break;
            case TransformComponentConstraintYBase::doesCopyYPropertyKey:
                object->as<TransformComponentConstraintYBase>()->doesCopyY(
                    value);
                break;
            case TransformComponentConstraintYBase::minYPropertyKey:
                object->as<TransformComponentConstraintYBase>()->minY(value);
                break;
            case TransformComponentConstraintYBase::maxYPropertyKey:
                object->as<TransformComponentConstraintYBase>()->maxY(value);
                break;
            case IKConstraintBase::invertDirectionPropertyKey:
                object->as<IKConstraintBase>()->invertDirection(value);
                break;
            case FollowPathConstraintBase::orientPropertyKey:
                object->as<FollowPathConstraintBase>()->orient(value);
                break;
            case FollowPathConstraintBase::offsetPropertyKey:
                object->as<FollowPathConstraintBase>()->offset(value);
                break;
            case ScrollConstraintBase::snapPropertyKey:
                object->as<ScrollConstraintBase>()->snap(value);
                break;
            case ScrollConstraintBase::virtualizePropertyKey:
                object->as<ScrollConstraintBase>()->virtualize(value);
                break;
            case ScrollConstraintBase::infinitePropertyKey:
                object->as<ScrollConstraintBase>()->infinite(value);
                break;
            case ScrollBarConstraintBase::autoSizePropertyKey:
                object->as<ScrollBarConstraintBase>()->autoSize(value);
                break;
            case AxisBase::normalizedPropertyKey:
                object->as<AxisBase>()->normalized(value);
                break;
            case LayoutComponentStyleBase::intrinsicallySizedValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->intrinsicallySizedValue(
                    value);
                break;
            case LayoutComponentStyleBase::linkCornerRadiusPropertyKey:
                object->as<LayoutComponentStyleBase>()->linkCornerRadius(value);
                break;
            case NestedSimpleAnimationBase::isPlayingPropertyKey:
                object->as<NestedSimpleAnimationBase>()->isPlaying(value);
                break;
            case KeyFrameBoolBase::valuePropertyKey:
                object->as<KeyFrameBoolBase>()->value(value);
                break;
            case ListenerAlignTargetBase::preserveOffsetPropertyKey:
                object->as<ListenerAlignTargetBase>()->preserveOffset(value);
                break;
            case TransitionValueBooleanComparatorBase::valuePropertyKey:
                object->as<TransitionValueBooleanComparatorBase>()->value(
                    value);
                break;
            case NestedBoolBase::nestedValuePropertyKey:
                object->as<NestedBoolBase>()->nestedValue(value);
                break;
            case LinearAnimationBase::enableWorkAreaPropertyKey:
                object->as<LinearAnimationBase>()->enableWorkArea(value);
                break;
            case LinearAnimationBase::quantizePropertyKey:
                object->as<LinearAnimationBase>()->quantize(value);
                break;
            case StateMachineBoolBase::valuePropertyKey:
                object->as<StateMachineBoolBase>()->value(value);
                break;
            case ShapePaintBase::isVisiblePropertyKey:
                object->as<ShapePaintBase>()->isVisible(value);
                break;
            case DashPathBase::offsetIsPercentagePropertyKey:
                object->as<DashPathBase>()->offsetIsPercentage(value);
                break;
            case DashBase::lengthIsPercentagePropertyKey:
                object->as<DashBase>()->lengthIsPercentage(value);
                break;
            case StrokeBase::transformAffectsStrokePropertyKey:
                object->as<StrokeBase>()->transformAffectsStroke(value);
                break;
            case FeatherBase::innerPropertyKey:
                object->as<FeatherBase>()->inner(value);
                break;
            case PathBase::isHolePropertyKey:
                object->as<PathBase>()->isHole(value);
                break;
            case PointsCommonPathBase::isClosedPropertyKey:
                object->as<PointsCommonPathBase>()->isClosed(value);
                break;
            case RectangleBase::linkCornerRadiusPropertyKey:
                object->as<RectangleBase>()->linkCornerRadius(value);
                break;
            case ClippingShapeBase::isVisiblePropertyKey:
                object->as<ClippingShapeBase>()->isVisible(value);
                break;
            case CustomPropertyBooleanBase::propertyValuePropertyKey:
                object->as<CustomPropertyBooleanBase>()->propertyValue(value);
                break;
            case LayoutComponentBase::clipPropertyKey:
                object->as<LayoutComponentBase>()->clip(value);
                break;
            case BindablePropertyBooleanBase::propertyValuePropertyKey:
                object->as<BindablePropertyBooleanBase>()->propertyValue(value);
                break;
            case TextModifierRangeBase::clampPropertyKey:
                object->as<TextModifierRangeBase>()->clamp(value);
                break;
            case TextFollowPathModifierBase::radialPropertyKey:
                object->as<TextFollowPathModifierBase>()->radial(value);
                break;
            case TextFollowPathModifierBase::orientPropertyKey:
                object->as<TextFollowPathModifierBase>()->orient(value);
                break;
            case TextBase::fitFromBaselinePropertyKey:
                object->as<TextBase>()->fitFromBaseline(value);
                break;
        }
    }
    static void setDouble(Core* object, int propertyKey, float value)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceNumberBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceNumberBase>()->propertyValue(value);
                break;
            case CustomPropertyNumberBase::propertyValuePropertyKey:
                object->as<CustomPropertyNumberBase>()->propertyValue(value);
                break;
            case ConstraintBase::strengthPropertyKey:
                object->as<ConstraintBase>()->strength(value);
                break;
            case DistanceConstraintBase::distancePropertyKey:
                object->as<DistanceConstraintBase>()->distance(value);
                break;
            case TransformComponentConstraintBase::copyFactorPropertyKey:
                object->as<TransformComponentConstraintBase>()->copyFactor(
                    value);
                break;
            case TransformComponentConstraintBase::minValuePropertyKey:
                object->as<TransformComponentConstraintBase>()->minValue(value);
                break;
            case TransformComponentConstraintBase::maxValuePropertyKey:
                object->as<TransformComponentConstraintBase>()->maxValue(value);
                break;
            case TransformComponentConstraintYBase::copyFactorYPropertyKey:
                object->as<TransformComponentConstraintYBase>()->copyFactorY(
                    value);
                break;
            case TransformComponentConstraintYBase::minValueYPropertyKey:
                object->as<TransformComponentConstraintYBase>()->minValueY(
                    value);
                break;
            case TransformComponentConstraintYBase::maxValueYPropertyKey:
                object->as<TransformComponentConstraintYBase>()->maxValueY(
                    value);
                break;
            case FollowPathConstraintBase::distancePropertyKey:
                object->as<FollowPathConstraintBase>()->distance(value);
                break;
            case ScrollConstraintBase::scrollOffsetXPropertyKey:
                object->as<ScrollConstraintBase>()->scrollOffsetX(value);
                break;
            case ScrollConstraintBase::scrollOffsetYPropertyKey:
                object->as<ScrollConstraintBase>()->scrollOffsetY(value);
                break;
            case ScrollConstraintBase::scrollPercentXPropertyKey:
                object->as<ScrollConstraintBase>()->scrollPercentX(value);
                break;
            case ScrollConstraintBase::scrollPercentYPropertyKey:
                object->as<ScrollConstraintBase>()->scrollPercentY(value);
                break;
            case ScrollConstraintBase::scrollIndexPropertyKey:
                object->as<ScrollConstraintBase>()->scrollIndex(value);
                break;
            case ElasticScrollPhysicsBase::frictionPropertyKey:
                object->as<ElasticScrollPhysicsBase>()->friction(value);
                break;
            case ElasticScrollPhysicsBase::speedMultiplierPropertyKey:
                object->as<ElasticScrollPhysicsBase>()->speedMultiplier(value);
                break;
            case ElasticScrollPhysicsBase::elasticFactorPropertyKey:
                object->as<ElasticScrollPhysicsBase>()->elasticFactor(value);
                break;
            case TransformConstraintBase::originXPropertyKey:
                object->as<TransformConstraintBase>()->originX(value);
                break;
            case TransformConstraintBase::originYPropertyKey:
                object->as<TransformConstraintBase>()->originY(value);
                break;
            case WorldTransformComponentBase::opacityPropertyKey:
                object->as<WorldTransformComponentBase>()->opacity(value);
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
            case NodeBase::xPropertyKey:
            case NodeBase::xArtboardPropertyKey:
                object->as<NodeBase>()->x(value);
                break;
            case NodeBase::yPropertyKey:
            case NodeBase::yArtboardPropertyKey:
                object->as<NodeBase>()->y(value);
                break;
            case NodeBase::computedLocalXPropertyKey:
                object->as<NodeBase>()->computedLocalX(value);
                break;
            case NodeBase::computedLocalYPropertyKey:
                object->as<NodeBase>()->computedLocalY(value);
                break;
            case NodeBase::computedWorldXPropertyKey:
                object->as<NodeBase>()->computedWorldX(value);
                break;
            case NodeBase::computedWorldYPropertyKey:
                object->as<NodeBase>()->computedWorldY(value);
                break;
            case NodeBase::computedRootXPropertyKey:
                object->as<NodeBase>()->computedRootX(value);
                break;
            case NodeBase::computedRootYPropertyKey:
                object->as<NodeBase>()->computedRootY(value);
                break;
            case NodeBase::computedWidthPropertyKey:
                object->as<NodeBase>()->computedWidth(value);
                break;
            case NodeBase::computedHeightPropertyKey:
                object->as<NodeBase>()->computedHeight(value);
                break;
            case NestedArtboardLayoutBase::instanceWidthPropertyKey:
                object->as<NestedArtboardLayoutBase>()->instanceWidth(value);
                break;
            case NestedArtboardLayoutBase::instanceHeightPropertyKey:
                object->as<NestedArtboardLayoutBase>()->instanceHeight(value);
                break;
            case AxisBase::offsetPropertyKey:
                object->as<AxisBase>()->offset(value);
                break;
            case LayoutComponentStyleBase::gapHorizontalPropertyKey:
                object->as<LayoutComponentStyleBase>()->gapHorizontal(value);
                break;
            case LayoutComponentStyleBase::gapVerticalPropertyKey:
                object->as<LayoutComponentStyleBase>()->gapVertical(value);
                break;
            case LayoutComponentStyleBase::maxWidthPropertyKey:
                object->as<LayoutComponentStyleBase>()->maxWidth(value);
                break;
            case LayoutComponentStyleBase::maxHeightPropertyKey:
                object->as<LayoutComponentStyleBase>()->maxHeight(value);
                break;
            case LayoutComponentStyleBase::minWidthPropertyKey:
                object->as<LayoutComponentStyleBase>()->minWidth(value);
                break;
            case LayoutComponentStyleBase::minHeightPropertyKey:
                object->as<LayoutComponentStyleBase>()->minHeight(value);
                break;
            case LayoutComponentStyleBase::borderLeftPropertyKey:
                object->as<LayoutComponentStyleBase>()->borderLeft(value);
                break;
            case LayoutComponentStyleBase::borderRightPropertyKey:
                object->as<LayoutComponentStyleBase>()->borderRight(value);
                break;
            case LayoutComponentStyleBase::borderTopPropertyKey:
                object->as<LayoutComponentStyleBase>()->borderTop(value);
                break;
            case LayoutComponentStyleBase::borderBottomPropertyKey:
                object->as<LayoutComponentStyleBase>()->borderBottom(value);
                break;
            case LayoutComponentStyleBase::marginLeftPropertyKey:
                object->as<LayoutComponentStyleBase>()->marginLeft(value);
                break;
            case LayoutComponentStyleBase::marginRightPropertyKey:
                object->as<LayoutComponentStyleBase>()->marginRight(value);
                break;
            case LayoutComponentStyleBase::marginTopPropertyKey:
                object->as<LayoutComponentStyleBase>()->marginTop(value);
                break;
            case LayoutComponentStyleBase::marginBottomPropertyKey:
                object->as<LayoutComponentStyleBase>()->marginBottom(value);
                break;
            case LayoutComponentStyleBase::paddingLeftPropertyKey:
                object->as<LayoutComponentStyleBase>()->paddingLeft(value);
                break;
            case LayoutComponentStyleBase::paddingRightPropertyKey:
                object->as<LayoutComponentStyleBase>()->paddingRight(value);
                break;
            case LayoutComponentStyleBase::paddingTopPropertyKey:
                object->as<LayoutComponentStyleBase>()->paddingTop(value);
                break;
            case LayoutComponentStyleBase::paddingBottomPropertyKey:
                object->as<LayoutComponentStyleBase>()->paddingBottom(value);
                break;
            case LayoutComponentStyleBase::positionLeftPropertyKey:
                object->as<LayoutComponentStyleBase>()->positionLeft(value);
                break;
            case LayoutComponentStyleBase::positionRightPropertyKey:
                object->as<LayoutComponentStyleBase>()->positionRight(value);
                break;
            case LayoutComponentStyleBase::positionTopPropertyKey:
                object->as<LayoutComponentStyleBase>()->positionTop(value);
                break;
            case LayoutComponentStyleBase::positionBottomPropertyKey:
                object->as<LayoutComponentStyleBase>()->positionBottom(value);
                break;
            case LayoutComponentStyleBase::flexPropertyKey:
                object->as<LayoutComponentStyleBase>()->flex(value);
                break;
            case LayoutComponentStyleBase::flexGrowPropertyKey:
                object->as<LayoutComponentStyleBase>()->flexGrow(value);
                break;
            case LayoutComponentStyleBase::flexShrinkPropertyKey:
                object->as<LayoutComponentStyleBase>()->flexShrink(value);
                break;
            case LayoutComponentStyleBase::flexBasisPropertyKey:
                object->as<LayoutComponentStyleBase>()->flexBasis(value);
                break;
            case LayoutComponentStyleBase::aspectRatioPropertyKey:
                object->as<LayoutComponentStyleBase>()->aspectRatio(value);
                break;
            case LayoutComponentStyleBase::interpolationTimePropertyKey:
                object->as<LayoutComponentStyleBase>()->interpolationTime(
                    value);
                break;
            case LayoutComponentStyleBase::cornerRadiusTLPropertyKey:
                object->as<LayoutComponentStyleBase>()->cornerRadiusTL(value);
                break;
            case LayoutComponentStyleBase::cornerRadiusTRPropertyKey:
                object->as<LayoutComponentStyleBase>()->cornerRadiusTR(value);
                break;
            case LayoutComponentStyleBase::cornerRadiusBLPropertyKey:
                object->as<LayoutComponentStyleBase>()->cornerRadiusBL(value);
                break;
            case LayoutComponentStyleBase::cornerRadiusBRPropertyKey:
                object->as<LayoutComponentStyleBase>()->cornerRadiusBR(value);
                break;
            case NSlicedNodeBase::initialWidthPropertyKey:
                object->as<NSlicedNodeBase>()->initialWidth(value);
                break;
            case NSlicedNodeBase::initialHeightPropertyKey:
                object->as<NSlicedNodeBase>()->initialHeight(value);
                break;
            case NSlicedNodeBase::widthPropertyKey:
                object->as<NSlicedNodeBase>()->width(value);
                break;
            case NSlicedNodeBase::heightPropertyKey:
                object->as<NSlicedNodeBase>()->height(value);
                break;
            case ArtboardComponentListOverrideBase::instanceWidthPropertyKey:
                object->as<ArtboardComponentListOverrideBase>()->instanceWidth(
                    value);
                break;
            case ArtboardComponentListOverrideBase::instanceHeightPropertyKey:
                object->as<ArtboardComponentListOverrideBase>()->instanceHeight(
                    value);
                break;
            case NestedLinearAnimationBase::mixPropertyKey:
                object->as<NestedLinearAnimationBase>()->mix(value);
                break;
            case NestedSimpleAnimationBase::speedPropertyKey:
                object->as<NestedSimpleAnimationBase>()->speed(value);
                break;
            case AdvanceableStateBase::speedPropertyKey:
                object->as<AdvanceableStateBase>()->speed(value);
                break;
            case BlendAnimationDirectBase::mixValuePropertyKey:
                object->as<BlendAnimationDirectBase>()->mixValue(value);
                break;
            case StateMachineNumberBase::valuePropertyKey:
                object->as<StateMachineNumberBase>()->value(value);
                break;
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
            case TransitionNumberConditionBase::valuePropertyKey:
                object->as<TransitionNumberConditionBase>()->value(value);
                break;
            case CubicInterpolatorComponentBase::x1PropertyKey:
                object->as<CubicInterpolatorComponentBase>()->x1(value);
                break;
            case CubicInterpolatorComponentBase::y1PropertyKey:
                object->as<CubicInterpolatorComponentBase>()->y1(value);
                break;
            case CubicInterpolatorComponentBase::x2PropertyKey:
                object->as<CubicInterpolatorComponentBase>()->x2(value);
                break;
            case CubicInterpolatorComponentBase::y2PropertyKey:
                object->as<CubicInterpolatorComponentBase>()->y2(value);
                break;
            case ListenerNumberChangeBase::valuePropertyKey:
                object->as<ListenerNumberChangeBase>()->value(value);
                break;
            case KeyFrameDoubleBase::valuePropertyKey:
                object->as<KeyFrameDoubleBase>()->value(value);
                break;
            case LinearAnimationBase::speedPropertyKey:
                object->as<LinearAnimationBase>()->speed(value);
                break;
            case TransitionValueNumberComparatorBase::valuePropertyKey:
                object->as<TransitionValueNumberComparatorBase>()->value(value);
                break;
            case ElasticInterpolatorBase::amplitudePropertyKey:
                object->as<ElasticInterpolatorBase>()->amplitude(value);
                break;
            case ElasticInterpolatorBase::periodPropertyKey:
                object->as<ElasticInterpolatorBase>()->period(value);
                break;
            case NestedNumberBase::nestedValuePropertyKey:
                object->as<NestedNumberBase>()->nestedValue(value);
                break;
            case NestedRemapAnimationBase::timePropertyKey:
                object->as<NestedRemapAnimationBase>()->time(value);
                break;
            case BlendAnimation1DBase::valuePropertyKey:
                object->as<BlendAnimation1DBase>()->value(value);
                break;
            case DashPathBase::offsetPropertyKey:
                object->as<DashPathBase>()->offset(value);
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
            case DashBase::lengthPropertyKey:
                object->as<DashBase>()->length(value);
                break;
            case StrokeBase::thicknessPropertyKey:
                object->as<StrokeBase>()->thickness(value);
                break;
            case GradientStopBase::positionPropertyKey:
                object->as<GradientStopBase>()->position(value);
                break;
            case FeatherBase::strengthPropertyKey:
                object->as<FeatherBase>()->strength(value);
                break;
            case FeatherBase::offsetXPropertyKey:
                object->as<FeatherBase>()->offsetX(value);
                break;
            case FeatherBase::offsetYPropertyKey:
                object->as<FeatherBase>()->offsetY(value);
                break;
            case TrimPathBase::startPropertyKey:
                object->as<TrimPathBase>()->start(value);
                break;
            case TrimPathBase::endPropertyKey:
                object->as<TrimPathBase>()->end(value);
                break;
            case TrimPathBase::offsetPropertyKey:
                object->as<TrimPathBase>()->offset(value);
                break;
            case VertexBase::xPropertyKey:
                object->as<VertexBase>()->x(value);
                break;
            case VertexBase::yPropertyKey:
                object->as<VertexBase>()->y(value);
                break;
            case MeshVertexBase::uPropertyKey:
                object->as<MeshVertexBase>()->u(value);
                break;
            case MeshVertexBase::vPropertyKey:
                object->as<MeshVertexBase>()->v(value);
                break;
            case ShapeBase::lengthPropertyKey:
                object->as<ShapeBase>()->length(value);
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
            case ParametricPathBase::originXPropertyKey:
                object->as<ParametricPathBase>()->originX(value);
                break;
            case ParametricPathBase::originYPropertyKey:
                object->as<ParametricPathBase>()->originY(value);
                break;
            case RectangleBase::cornerRadiusTLPropertyKey:
                object->as<RectangleBase>()->cornerRadiusTL(value);
                break;
            case RectangleBase::cornerRadiusTRPropertyKey:
                object->as<RectangleBase>()->cornerRadiusTR(value);
                break;
            case RectangleBase::cornerRadiusBLPropertyKey:
                object->as<RectangleBase>()->cornerRadiusBL(value);
                break;
            case RectangleBase::cornerRadiusBRPropertyKey:
                object->as<RectangleBase>()->cornerRadiusBR(value);
                break;
            case CubicMirroredVertexBase::rotationPropertyKey:
                object->as<CubicMirroredVertexBase>()->rotation(value);
                break;
            case CubicMirroredVertexBase::distancePropertyKey:
                object->as<CubicMirroredVertexBase>()->distance(value);
                break;
            case PolygonBase::cornerRadiusPropertyKey:
                object->as<PolygonBase>()->cornerRadius(value);
                break;
            case StarBase::innerRadiusPropertyKey:
                object->as<StarBase>()->innerRadius(value);
                break;
            case ImageBase::originXPropertyKey:
                object->as<ImageBase>()->originX(value);
                break;
            case ImageBase::originYPropertyKey:
                object->as<ImageBase>()->originY(value);
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
            case LayoutComponentBase::widthPropertyKey:
                object->as<LayoutComponentBase>()->width(value);
                break;
            case LayoutComponentBase::heightPropertyKey:
                object->as<LayoutComponentBase>()->height(value);
                break;
            case LayoutComponentBase::fractionalWidthPropertyKey:
                object->as<LayoutComponentBase>()->fractionalWidth(value);
                break;
            case LayoutComponentBase::fractionalHeightPropertyKey:
                object->as<LayoutComponentBase>()->fractionalHeight(value);
                break;
            case ArtboardBase::originXPropertyKey:
                object->as<ArtboardBase>()->originX(value);
                break;
            case ArtboardBase::originYPropertyKey:
                object->as<ArtboardBase>()->originY(value);
                break;
            case JoystickBase::xPropertyKey:
                object->as<JoystickBase>()->x(value);
                break;
            case JoystickBase::yPropertyKey:
                object->as<JoystickBase>()->y(value);
                break;
            case JoystickBase::posXPropertyKey:
                object->as<JoystickBase>()->posX(value);
                break;
            case JoystickBase::posYPropertyKey:
                object->as<JoystickBase>()->posY(value);
                break;
            case JoystickBase::originXPropertyKey:
                object->as<JoystickBase>()->originX(value);
                break;
            case JoystickBase::originYPropertyKey:
                object->as<JoystickBase>()->originY(value);
                break;
            case JoystickBase::widthPropertyKey:
                object->as<JoystickBase>()->width(value);
                break;
            case JoystickBase::heightPropertyKey:
                object->as<JoystickBase>()->height(value);
                break;
            case DataConverterOperationValueBase::operationValuePropertyKey:
                object->as<DataConverterOperationValueBase>()->operationValue(
                    value);
                break;
            case DataConverterRangeMapperBase::minInputPropertyKey:
                object->as<DataConverterRangeMapperBase>()->minInput(value);
                break;
            case DataConverterRangeMapperBase::maxInputPropertyKey:
                object->as<DataConverterRangeMapperBase>()->maxInput(value);
                break;
            case DataConverterRangeMapperBase::minOutputPropertyKey:
                object->as<DataConverterRangeMapperBase>()->minOutput(value);
                break;
            case DataConverterRangeMapperBase::maxOutputPropertyKey:
                object->as<DataConverterRangeMapperBase>()->maxOutput(value);
                break;
            case DataConverterInterpolatorBase::durationPropertyKey:
                object->as<DataConverterInterpolatorBase>()->duration(value);
                break;
            case FormulaTokenValueBase::operationValuePropertyKey:
                object->as<FormulaTokenValueBase>()->operationValue(value);
                break;
            case BindablePropertyNumberBase::propertyValuePropertyKey:
                object->as<BindablePropertyNumberBase>()->propertyValue(value);
                break;
            case NestedArtboardLeafBase::alignmentXPropertyKey:
                object->as<NestedArtboardLeafBase>()->alignmentX(value);
                break;
            case NestedArtboardLeafBase::alignmentYPropertyKey:
                object->as<NestedArtboardLeafBase>()->alignmentY(value);
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
            case TextModifierRangeBase::modifyFromPropertyKey:
                object->as<TextModifierRangeBase>()->modifyFrom(value);
                break;
            case TextModifierRangeBase::modifyToPropertyKey:
                object->as<TextModifierRangeBase>()->modifyTo(value);
                break;
            case TextModifierRangeBase::strengthPropertyKey:
                object->as<TextModifierRangeBase>()->strength(value);
                break;
            case TextModifierRangeBase::falloffFromPropertyKey:
                object->as<TextModifierRangeBase>()->falloffFrom(value);
                break;
            case TextModifierRangeBase::falloffToPropertyKey:
                object->as<TextModifierRangeBase>()->falloffTo(value);
                break;
            case TextModifierRangeBase::offsetPropertyKey:
                object->as<TextModifierRangeBase>()->offset(value);
                break;
            case TextFollowPathModifierBase::startPropertyKey:
                object->as<TextFollowPathModifierBase>()->start(value);
                break;
            case TextFollowPathModifierBase::endPropertyKey:
                object->as<TextFollowPathModifierBase>()->end(value);
                break;
            case TextFollowPathModifierBase::strengthPropertyKey:
                object->as<TextFollowPathModifierBase>()->strength(value);
                break;
            case TextFollowPathModifierBase::offsetPropertyKey:
                object->as<TextFollowPathModifierBase>()->offset(value);
                break;
            case TextVariationModifierBase::axisValuePropertyKey:
                object->as<TextVariationModifierBase>()->axisValue(value);
                break;
            case TextModifierGroupBase::originXPropertyKey:
                object->as<TextModifierGroupBase>()->originX(value);
                break;
            case TextModifierGroupBase::originYPropertyKey:
                object->as<TextModifierGroupBase>()->originY(value);
                break;
            case TextModifierGroupBase::opacityPropertyKey:
                object->as<TextModifierGroupBase>()->opacity(value);
                break;
            case TextModifierGroupBase::xPropertyKey:
                object->as<TextModifierGroupBase>()->x(value);
                break;
            case TextModifierGroupBase::yPropertyKey:
                object->as<TextModifierGroupBase>()->y(value);
                break;
            case TextModifierGroupBase::rotationPropertyKey:
                object->as<TextModifierGroupBase>()->rotation(value);
                break;
            case TextModifierGroupBase::scaleXPropertyKey:
                object->as<TextModifierGroupBase>()->scaleX(value);
                break;
            case TextModifierGroupBase::scaleYPropertyKey:
                object->as<TextModifierGroupBase>()->scaleY(value);
                break;
            case TextStyleBase::fontSizePropertyKey:
                object->as<TextStyleBase>()->fontSize(value);
                break;
            case TextStyleBase::lineHeightPropertyKey:
                object->as<TextStyleBase>()->lineHeight(value);
                break;
            case TextStyleBase::letterSpacingPropertyKey:
                object->as<TextStyleBase>()->letterSpacing(value);
                break;
            case TextInputBase::selectionRadiusPropertyKey:
                object->as<TextInputBase>()->selectionRadius(value);
                break;
            case TextStyleAxisBase::axisValuePropertyKey:
                object->as<TextStyleAxisBase>()->axisValue(value);
                break;
            case TextBase::widthPropertyKey:
                object->as<TextBase>()->width(value);
                break;
            case TextBase::heightPropertyKey:
                object->as<TextBase>()->height(value);
                break;
            case TextBase::originXPropertyKey:
                object->as<TextBase>()->originX(value);
                break;
            case TextBase::originYPropertyKey:
                object->as<TextBase>()->originY(value);
                break;
            case TextBase::paragraphSpacingPropertyKey:
                object->as<TextBase>()->paragraphSpacing(value);
                break;
            case DrawableAssetBase::heightPropertyKey:
                object->as<DrawableAssetBase>()->height(value);
                break;
            case DrawableAssetBase::widthPropertyKey:
                object->as<DrawableAssetBase>()->width(value);
                break;
            case ExportAudioBase::volumePropertyKey:
                object->as<ExportAudioBase>()->volume(value);
                break;
        }
    }
    static void setCallback(Core* object, int propertyKey, CallbackData value)
    {
        switch (propertyKey)
        {
            case NestedTriggerBase::firePropertyKey:
                object->as<NestedTriggerBase>()->fire(value);
                break;
            case EventBase::triggerPropertyKey:
                object->as<EventBase>()->trigger(value);
                break;
            case CustomPropertyTriggerBase::firePropertyKey:
                object->as<CustomPropertyTriggerBase>()->fire(value);
                break;
        }
    }
    static uint32_t getUint(Core* object, int propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceListItemBase::viewModelIdPropertyKey:
                return object->as<ViewModelInstanceListItemBase>()
                    ->viewModelId();
            case ViewModelInstanceListItemBase::viewModelInstanceIdPropertyKey:
                return object->as<ViewModelInstanceListItemBase>()
                    ->viewModelInstanceId();
            case ViewModelPropertyBase::symbolTypeValuePropertyKey:
                return object->as<ViewModelPropertyBase>()->symbolTypeValue();
            case ViewModelInstanceValueBase::viewModelPropertyIdPropertyKey:
                return object->as<ViewModelInstanceValueBase>()
                    ->viewModelPropertyId();
            case ViewModelPropertyEnumCustomBase::enumIdPropertyKey:
                return object->as<ViewModelPropertyEnumCustomBase>()->enumId();
            case ViewModelInstanceEnumBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceEnumBase>()->propertyValue();
            case ViewModelInstanceArtboardBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceArtboardBase>()
                    ->propertyValue();
            case ViewModelPropertyEnumSystemBase::enumTypePropertyKey:
                return object->as<ViewModelPropertyEnumSystemBase>()
                    ->enumType();
            case DataEnumSystemBase::enumTypePropertyKey:
                return object->as<DataEnumSystemBase>()->enumType();
            case ViewModelPropertyViewModelBase::
                viewModelReferenceIdPropertyKey:
                return object->as<ViewModelPropertyViewModelBase>()
                    ->viewModelReferenceId();
            case ComponentBase::parentIdPropertyKey:
                return object->as<ComponentBase>()->parentId();
            case ViewModelInstanceBase::viewModelIdPropertyKey:
                return object->as<ViewModelInstanceBase>()->viewModelId();
            case ViewModelInstanceTriggerBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceTriggerBase>()
                    ->propertyValue();
            case ViewModelInstanceSymbolListIndexBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceSymbolListIndexBase>()
                    ->propertyValue();
            case ViewModelInstanceViewModelBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceViewModelBase>()
                    ->propertyValue();
            case ViewModelInstanceAssetBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceAssetBase>()
                    ->propertyValue();
            case DrawTargetBase::drawableIdPropertyKey:
                return object->as<DrawTargetBase>()->drawableId();
            case DrawTargetBase::placementValuePropertyKey:
                return object->as<DrawTargetBase>()->placementValue();
            case TargetedConstraintBase::targetIdPropertyKey:
                return object->as<TargetedConstraintBase>()->targetId();
            case DistanceConstraintBase::modeValuePropertyKey:
                return object->as<DistanceConstraintBase>()->modeValue();
            case TransformSpaceConstraintBase::sourceSpaceValuePropertyKey:
                return object->as<TransformSpaceConstraintBase>()
                    ->sourceSpaceValue();
            case TransformSpaceConstraintBase::destSpaceValuePropertyKey:
                return object->as<TransformSpaceConstraintBase>()
                    ->destSpaceValue();
            case TransformComponentConstraintBase::minMaxSpaceValuePropertyKey:
                return object->as<TransformComponentConstraintBase>()
                    ->minMaxSpaceValue();
            case IKConstraintBase::parentBoneCountPropertyKey:
                return object->as<IKConstraintBase>()->parentBoneCount();
            case ScrollPhysicsBase::constraintIdPropertyKey:
                return object->as<ScrollPhysicsBase>()->constraintId();
            case DraggableConstraintBase::directionValuePropertyKey:
                return object->as<DraggableConstraintBase>()->directionValue();
            case ScrollConstraintBase::physicsTypeValuePropertyKey:
                return object->as<ScrollConstraintBase>()->physicsTypeValue();
            case ScrollConstraintBase::physicsIdPropertyKey:
                return object->as<ScrollConstraintBase>()->physicsId();
            case ScrollBarConstraintBase::scrollConstraintIdPropertyKey:
                return object->as<ScrollBarConstraintBase>()
                    ->scrollConstraintId();
            case DrawableBase::blendModeValuePropertyKey:
                return object->as<DrawableBase>()->blendModeValue();
            case DrawableBase::drawableFlagsPropertyKey:
                return object->as<DrawableBase>()->drawableFlags();
            case NestedArtboardBase::artboardIdPropertyKey:
                return object->as<NestedArtboardBase>()->artboardId();
            case ArtboardComponentListBase::listSourcePropertyKey:
                return object->as<ArtboardComponentListBase>()->listSource();
            case NestedAnimationBase::animationIdPropertyKey:
                return object->as<NestedAnimationBase>()->animationId();
            case SoloBase::activeComponentIdPropertyKey:
                return object->as<SoloBase>()->activeComponentId();
            case NestedArtboardLayoutBase::instanceWidthUnitsValuePropertyKey:
                return object->as<NestedArtboardLayoutBase>()
                    ->instanceWidthUnitsValue();
            case NestedArtboardLayoutBase::instanceHeightUnitsValuePropertyKey:
                return object->as<NestedArtboardLayoutBase>()
                    ->instanceHeightUnitsValue();
            case NestedArtboardLayoutBase::instanceWidthScaleTypePropertyKey:
                return object->as<NestedArtboardLayoutBase>()
                    ->instanceWidthScaleType();
            case NestedArtboardLayoutBase::instanceHeightScaleTypePropertyKey:
                return object->as<NestedArtboardLayoutBase>()
                    ->instanceHeightScaleType();
            case NSlicerTileModeBase::patchIndexPropertyKey:
                return object->as<NSlicerTileModeBase>()->patchIndex();
            case NSlicerTileModeBase::stylePropertyKey:
                return object->as<NSlicerTileModeBase>()->style();
            case LayoutComponentStyleBase::flexBasisUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->flexBasisUnitsValue();
            case LayoutComponentStyleBase::layoutWidthScaleTypePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->layoutWidthScaleType();
            case LayoutComponentStyleBase::layoutHeightScaleTypePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->layoutHeightScaleType();
            case LayoutComponentStyleBase::layoutAlignmentTypePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->layoutAlignmentType();
            case LayoutComponentStyleBase::animationStyleTypePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->animationStyleType();
            case LayoutComponentStyleBase::interpolationTypePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->interpolationType();
            case LayoutComponentStyleBase::interpolatorIdPropertyKey:
                return object->as<LayoutComponentStyleBase>()->interpolatorId();
            case LayoutComponentStyleBase::displayValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()->displayValue();
            case LayoutComponentStyleBase::positionTypeValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->positionTypeValue();
            case LayoutComponentStyleBase::flexDirectionValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->flexDirectionValue();
            case LayoutComponentStyleBase::directionValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()->directionValue();
            case LayoutComponentStyleBase::alignContentValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->alignContentValue();
            case LayoutComponentStyleBase::alignItemsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->alignItemsValue();
            case LayoutComponentStyleBase::alignSelfValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()->alignSelfValue();
            case LayoutComponentStyleBase::justifyContentValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->justifyContentValue();
            case LayoutComponentStyleBase::flexWrapValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()->flexWrapValue();
            case LayoutComponentStyleBase::overflowValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()->overflowValue();
            case LayoutComponentStyleBase::widthUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->widthUnitsValue();
            case LayoutComponentStyleBase::heightUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->heightUnitsValue();
            case LayoutComponentStyleBase::borderLeftUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->borderLeftUnitsValue();
            case LayoutComponentStyleBase::borderRightUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->borderRightUnitsValue();
            case LayoutComponentStyleBase::borderTopUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->borderTopUnitsValue();
            case LayoutComponentStyleBase::borderBottomUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->borderBottomUnitsValue();
            case LayoutComponentStyleBase::marginLeftUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->marginLeftUnitsValue();
            case LayoutComponentStyleBase::marginRightUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->marginRightUnitsValue();
            case LayoutComponentStyleBase::marginTopUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->marginTopUnitsValue();
            case LayoutComponentStyleBase::marginBottomUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->marginBottomUnitsValue();
            case LayoutComponentStyleBase::paddingLeftUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->paddingLeftUnitsValue();
            case LayoutComponentStyleBase::paddingRightUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->paddingRightUnitsValue();
            case LayoutComponentStyleBase::paddingTopUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->paddingTopUnitsValue();
            case LayoutComponentStyleBase::paddingBottomUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->paddingBottomUnitsValue();
            case LayoutComponentStyleBase::positionLeftUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->positionLeftUnitsValue();
            case LayoutComponentStyleBase::positionRightUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->positionRightUnitsValue();
            case LayoutComponentStyleBase::positionTopUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->positionTopUnitsValue();
            case LayoutComponentStyleBase::positionBottomUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->positionBottomUnitsValue();
            case LayoutComponentStyleBase::gapHorizontalUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->gapHorizontalUnitsValue();
            case LayoutComponentStyleBase::gapVerticalUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->gapVerticalUnitsValue();
            case LayoutComponentStyleBase::minWidthUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->minWidthUnitsValue();
            case LayoutComponentStyleBase::minHeightUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->minHeightUnitsValue();
            case LayoutComponentStyleBase::maxWidthUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->maxWidthUnitsValue();
            case LayoutComponentStyleBase::maxHeightUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->maxHeightUnitsValue();
            case ArtboardComponentListOverrideBase::artboardIdPropertyKey:
                return object->as<ArtboardComponentListOverrideBase>()
                    ->artboardId();
            case ArtboardComponentListOverrideBase::
                instanceWidthUnitsValuePropertyKey:
                return object->as<ArtboardComponentListOverrideBase>()
                    ->instanceWidthUnitsValue();
            case ArtboardComponentListOverrideBase::
                instanceHeightUnitsValuePropertyKey:
                return object->as<ArtboardComponentListOverrideBase>()
                    ->instanceHeightUnitsValue();
            case ArtboardComponentListOverrideBase::
                instanceWidthScaleTypePropertyKey:
                return object->as<ArtboardComponentListOverrideBase>()
                    ->instanceWidthScaleType();
            case ArtboardComponentListOverrideBase::
                instanceHeightScaleTypePropertyKey:
                return object->as<ArtboardComponentListOverrideBase>()
                    ->instanceHeightScaleType();
            case ListenerFireEventBase::eventIdPropertyKey:
                return object->as<ListenerFireEventBase>()->eventId();
            case LayerStateBase::flagsPropertyKey:
                return object->as<LayerStateBase>()->flags();
            case StateMachineFireActionBase::occursValuePropertyKey:
                return object->as<StateMachineFireActionBase>()->occursValue();
            case TransitionValueTriggerComparatorBase::valuePropertyKey:
                return object->as<TransitionValueTriggerComparatorBase>()
                    ->value();
            case KeyFrameBase::framePropertyKey:
                return object->as<KeyFrameBase>()->frame();
            case InterpolatingKeyFrameBase::interpolationTypePropertyKey:
                return object->as<InterpolatingKeyFrameBase>()
                    ->interpolationType();
            case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
                return object->as<InterpolatingKeyFrameBase>()
                    ->interpolatorId();
            case KeyFrameUintBase::valuePropertyKey:
                return object->as<KeyFrameUintBase>()->value();
            case ListenerInputChangeBase::inputIdPropertyKey:
                return object->as<ListenerInputChangeBase>()->inputId();
            case ListenerInputChangeBase::nestedInputIdPropertyKey:
                return object->as<ListenerInputChangeBase>()->nestedInputId();
            case AnimationStateBase::animationIdPropertyKey:
                return object->as<AnimationStateBase>()->animationId();
            case NestedInputBase::inputIdPropertyKey:
                return object->as<NestedInputBase>()->inputId();
            case KeyedObjectBase::objectIdPropertyKey:
                return object->as<KeyedObjectBase>()->objectId();
            case BlendAnimationBase::animationIdPropertyKey:
                return object->as<BlendAnimationBase>()->animationId();
            case BlendAnimationDirectBase::inputIdPropertyKey:
                return object->as<BlendAnimationDirectBase>()->inputId();
            case BlendAnimationDirectBase::blendSourcePropertyKey:
                return object->as<BlendAnimationDirectBase>()->blendSource();
            case TransitionInputConditionBase::inputIdPropertyKey:
                return object->as<TransitionInputConditionBase>()->inputId();
            case KeyedPropertyBase::propertyKeyPropertyKey:
                return object->as<KeyedPropertyBase>()->propertyKey();
            case StateMachineListenerBase::targetIdPropertyKey:
                return object->as<StateMachineListenerBase>()->targetId();
            case StateMachineListenerBase::listenerTypeValuePropertyKey:
                return object->as<StateMachineListenerBase>()
                    ->listenerTypeValue();
            case StateMachineListenerBase::eventIdPropertyKey:
                return object->as<StateMachineListenerBase>()->eventId();
            case TransitionPropertyArtboardComparatorBase::
                propertyTypePropertyKey:
                return object->as<TransitionPropertyArtboardComparatorBase>()
                    ->propertyType();
            case KeyFrameIdBase::valuePropertyKey:
                return object->as<KeyFrameIdBase>()->value();
            case ListenerBoolChangeBase::valuePropertyKey:
                return object->as<ListenerBoolChangeBase>()->value();
            case ListenerAlignTargetBase::targetIdPropertyKey:
                return object->as<ListenerAlignTargetBase>()->targetId();
            case TransitionValueConditionBase::opValuePropertyKey:
                return object->as<TransitionValueConditionBase>()->opValue();
            case TransitionViewModelConditionBase::opValuePropertyKey:
                return object->as<TransitionViewModelConditionBase>()
                    ->opValue();
            case BlendState1DInputBase::inputIdPropertyKey:
                return object->as<BlendState1DInputBase>()->inputId();
            case TransitionValueIdComparatorBase::valuePropertyKey:
                return object->as<TransitionValueIdComparatorBase>()->value();
            case StateTransitionBase::stateToIdPropertyKey:
                return object->as<StateTransitionBase>()->stateToId();
            case StateTransitionBase::flagsPropertyKey:
                return object->as<StateTransitionBase>()->flags();
            case StateTransitionBase::durationPropertyKey:
                return object->as<StateTransitionBase>()->duration();
            case StateTransitionBase::exitTimePropertyKey:
                return object->as<StateTransitionBase>()->exitTime();
            case StateTransitionBase::interpolationTypePropertyKey:
                return object->as<StateTransitionBase>()->interpolationType();
            case StateTransitionBase::interpolatorIdPropertyKey:
                return object->as<StateTransitionBase>()->interpolatorId();
            case StateTransitionBase::randomWeightPropertyKey:
                return object->as<StateTransitionBase>()->randomWeight();
            case StateMachineFireEventBase::eventIdPropertyKey:
                return object->as<StateMachineFireEventBase>()->eventId();
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
            case ElasticInterpolatorBase::easingValuePropertyKey:
                return object->as<ElasticInterpolatorBase>()->easingValue();
            case BlendStateTransitionBase::exitBlendAnimationIdPropertyKey:
                return object->as<BlendStateTransitionBase>()
                    ->exitBlendAnimationId();
            case ShapePaintBase::blendModeValuePropertyKey:
                return object->as<ShapePaintBase>()->blendModeValue();
            case StrokeBase::capPropertyKey:
                return object->as<StrokeBase>()->cap();
            case StrokeBase::joinPropertyKey:
                return object->as<StrokeBase>()->join();
            case FeatherBase::spaceValuePropertyKey:
                return object->as<FeatherBase>()->spaceValue();
            case TrimPathBase::modeValuePropertyKey:
                return object->as<TrimPathBase>()->modeValue();
            case FillBase::fillRulePropertyKey:
                return object->as<FillBase>()->fillRule();
            case PathBase::pathFlagsPropertyKey:
                return object->as<PathBase>()->pathFlags();
            case ListPathBase::listSourcePropertyKey:
                return object->as<ListPathBase>()->listSource();
            case ClippingShapeBase::sourceIdPropertyKey:
                return object->as<ClippingShapeBase>()->sourceId();
            case ClippingShapeBase::fillRulePropertyKey:
                return object->as<ClippingShapeBase>()->fillRule();
            case PolygonBase::pointsPropertyKey:
                return object->as<PolygonBase>()->points();
            case ImageBase::assetIdPropertyKey:
                return object->as<ImageBase>()->assetId();
            case DrawRulesBase::drawTargetIdPropertyKey:
                return object->as<DrawRulesBase>()->drawTargetId();
            case LayoutComponentBase::styleIdPropertyKey:
                return object->as<LayoutComponentBase>()->styleId();
            case ArtboardBase::defaultStateMachineIdPropertyKey:
                return object->as<ArtboardBase>()->defaultStateMachineId();
            case ArtboardBase::viewModelIdPropertyKey:
                return object->as<ArtboardBase>()->viewModelId();
            case JoystickBase::xIdPropertyKey:
                return object->as<JoystickBase>()->xId();
            case JoystickBase::yIdPropertyKey:
                return object->as<JoystickBase>()->yId();
            case JoystickBase::joystickFlagsPropertyKey:
                return object->as<JoystickBase>()->joystickFlags();
            case JoystickBase::handleSourceIdPropertyKey:
                return object->as<JoystickBase>()->handleSourceId();
            case OpenUrlEventBase::targetValuePropertyKey:
                return object->as<OpenUrlEventBase>()->targetValue();
            case BindablePropertyIdBase::propertyValuePropertyKey:
                return object->as<BindablePropertyIdBase>()->propertyValue();
            case BindablePropertyIntegerBase::propertyValuePropertyKey:
                return object->as<BindablePropertyIntegerBase>()
                    ->propertyValue();
            case DataBindBase::propertyKeyPropertyKey:
                return object->as<DataBindBase>()->propertyKey();
            case DataBindBase::flagsPropertyKey:
                return object->as<DataBindBase>()->flags();
            case DataBindBase::converterIdPropertyKey:
                return object->as<DataBindBase>()->converterId();
            case DataConverterNumberToListBase::viewModelIdPropertyKey:
                return object->as<DataConverterNumberToListBase>()
                    ->viewModelId();
            case DataConverterOperationBase::operationTypePropertyKey:
                return object->as<DataConverterOperationBase>()
                    ->operationType();
            case DataConverterRangeMapperBase::interpolationTypePropertyKey:
                return object->as<DataConverterRangeMapperBase>()
                    ->interpolationType();
            case DataConverterRangeMapperBase::interpolatorIdPropertyKey:
                return object->as<DataConverterRangeMapperBase>()
                    ->interpolatorId();
            case DataConverterRangeMapperBase::flagsPropertyKey:
                return object->as<DataConverterRangeMapperBase>()->flags();
            case DataConverterInterpolatorBase::interpolationTypePropertyKey:
                return object->as<DataConverterInterpolatorBase>()
                    ->interpolationType();
            case DataConverterInterpolatorBase::interpolatorIdPropertyKey:
                return object->as<DataConverterInterpolatorBase>()
                    ->interpolatorId();
            case DataConverterGroupItemBase::converterIdPropertyKey:
                return object->as<DataConverterGroupItemBase>()->converterId();
            case DataConverterRounderBase::decimalsPropertyKey:
                return object->as<DataConverterRounderBase>()->decimals();
            case DataConverterStringPadBase::lengthPropertyKey:
                return object->as<DataConverterStringPadBase>()->length();
            case DataConverterStringPadBase::padTypePropertyKey:
                return object->as<DataConverterStringPadBase>()->padType();
            case DataConverterStringTrimBase::trimTypePropertyKey:
                return object->as<DataConverterStringTrimBase>()->trimType();
            case FormulaTokenOperationBase::operationTypePropertyKey:
                return object->as<FormulaTokenOperationBase>()->operationType();
            case FormulaTokenFunctionBase::functionTypePropertyKey:
                return object->as<FormulaTokenFunctionBase>()->functionType();
            case DataConverterToStringBase::flagsPropertyKey:
                return object->as<DataConverterToStringBase>()->flags();
            case DataConverterToStringBase::decimalsPropertyKey:
                return object->as<DataConverterToStringBase>()->decimals();
            case BindablePropertyListBase::propertyValuePropertyKey:
                return object->as<BindablePropertyListBase>()->propertyValue();
            case BindablePropertyEnumBase::propertyValuePropertyKey:
                return object->as<BindablePropertyEnumBase>()->propertyValue();
            case NestedArtboardLeafBase::fitPropertyKey:
                return object->as<NestedArtboardLeafBase>()->fit();
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
            case TextModifierRangeBase::unitsValuePropertyKey:
                return object->as<TextModifierRangeBase>()->unitsValue();
            case TextModifierRangeBase::typeValuePropertyKey:
                return object->as<TextModifierRangeBase>()->typeValue();
            case TextModifierRangeBase::modeValuePropertyKey:
                return object->as<TextModifierRangeBase>()->modeValue();
            case TextModifierRangeBase::runIdPropertyKey:
                return object->as<TextModifierRangeBase>()->runId();
            case TextTargetModifierBase::targetIdPropertyKey:
                return object->as<TextTargetModifierBase>()->targetId();
            case TextStyleFeatureBase::tagPropertyKey:
                return object->as<TextStyleFeatureBase>()->tag();
            case TextStyleFeatureBase::featureValuePropertyKey:
                return object->as<TextStyleFeatureBase>()->featureValue();
            case TextVariationModifierBase::axisTagPropertyKey:
                return object->as<TextVariationModifierBase>()->axisTag();
            case TextModifierGroupBase::modifierFlagsPropertyKey:
                return object->as<TextModifierGroupBase>()->modifierFlags();
            case TextStyleBase::fontAssetIdPropertyKey:
                return object->as<TextStyleBase>()->fontAssetId();
            case TextStyleAxisBase::tagPropertyKey:
                return object->as<TextStyleAxisBase>()->tag();
            case TextBase::alignValuePropertyKey:
                return object->as<TextBase>()->alignValue();
            case TextBase::sizingValuePropertyKey:
                return object->as<TextBase>()->sizingValue();
            case TextBase::overflowValuePropertyKey:
                return object->as<TextBase>()->overflowValue();
            case TextBase::originValuePropertyKey:
                return object->as<TextBase>()->originValue();
            case TextBase::wrapValuePropertyKey:
                return object->as<TextBase>()->wrapValue();
            case TextBase::verticalAlignValuePropertyKey:
                return object->as<TextBase>()->verticalAlignValue();
            case TextValueRunBase::styleIdPropertyKey:
                return object->as<TextValueRunBase>()->styleId();
            case CustomPropertyEnumBase::propertyValuePropertyKey:
                return object->as<CustomPropertyEnumBase>()->propertyValue();
            case CustomPropertyEnumBase::enumIdPropertyKey:
                return object->as<CustomPropertyEnumBase>()->enumId();
            case FileAssetBase::assetIdPropertyKey:
                return object->as<FileAssetBase>()->assetId();
            case AudioEventBase::assetIdPropertyKey:
                return object->as<AudioEventBase>()->assetId();
            case CustomPropertyTriggerBase::propertyValuePropertyKey:
                return object->as<CustomPropertyTriggerBase>()->propertyValue();
        }
        return 0;
    }
    static std::string getString(Core* object, int propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelComponentBase::namePropertyKey:
                return object->as<ViewModelComponentBase>()->name();
            case DataEnumCustomBase::namePropertyKey:
                return object->as<DataEnumCustomBase>()->name();
            case ViewModelInstanceStringBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceStringBase>()
                    ->propertyValue();
            case ComponentBase::namePropertyKey:
                return object->as<ComponentBase>()->name();
            case DataEnumValueBase::keyPropertyKey:
                return object->as<DataEnumValueBase>()->key();
            case DataEnumValueBase::valuePropertyKey:
                return object->as<DataEnumValueBase>()->value();
            case AnimationBase::namePropertyKey:
                return object->as<AnimationBase>()->name();
            case StateMachineComponentBase::namePropertyKey:
                return object->as<StateMachineComponentBase>()->name();
            case KeyFrameStringBase::valuePropertyKey:
                return object->as<KeyFrameStringBase>()->value();
            case TransitionValueStringComparatorBase::valuePropertyKey:
                return object->as<TransitionValueStringComparatorBase>()
                    ->value();
            case OpenUrlEventBase::urlPropertyKey:
                return object->as<OpenUrlEventBase>()->url();
            case DataConverterBase::namePropertyKey:
                return object->as<DataConverterBase>()->name();
            case DataConverterStringPadBase::textPropertyKey:
                return object->as<DataConverterStringPadBase>()->text();
            case DataConverterToStringBase::colorFormatPropertyKey:
                return object->as<DataConverterToStringBase>()->colorFormat();
            case BindablePropertyStringBase::propertyValuePropertyKey:
                return object->as<BindablePropertyStringBase>()
                    ->propertyValue();
            case TextInputBase::textPropertyKey:
                return object->as<TextInputBase>()->text();
            case TextValueRunBase::textPropertyKey:
                return object->as<TextValueRunBase>()->text();
            case CustomPropertyStringBase::propertyValuePropertyKey:
                return object->as<CustomPropertyStringBase>()->propertyValue();
            case AssetBase::namePropertyKey:
                return object->as<AssetBase>()->name();
            case FileAssetBase::cdnBaseUrlPropertyKey:
                return object->as<FileAssetBase>()->cdnBaseUrl();
        }
        return "";
    }
    static int getColor(Core* object, int propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceColorBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceColorBase>()
                    ->propertyValue();
            case CustomPropertyColorBase::propertyValuePropertyKey:
                return object->as<CustomPropertyColorBase>()->propertyValue();
            case KeyFrameColorBase::valuePropertyKey:
                return object->as<KeyFrameColorBase>()->value();
            case TransitionValueColorComparatorBase::valuePropertyKey:
                return object->as<TransitionValueColorComparatorBase>()
                    ->value();
            case SolidColorBase::colorValuePropertyKey:
                return object->as<SolidColorBase>()->colorValue();
            case GradientStopBase::colorValuePropertyKey:
                return object->as<GradientStopBase>()->colorValue();
            case BindablePropertyColorBase::propertyValuePropertyKey:
                return object->as<BindablePropertyColorBase>()->propertyValue();
        }
        return 0;
    }
    static bool getBool(Core* object, int propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceBooleanBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceBooleanBase>()
                    ->propertyValue();
            case TransformComponentConstraintBase::offsetPropertyKey:
                return object->as<TransformComponentConstraintBase>()->offset();
            case TransformComponentConstraintBase::doesCopyPropertyKey:
                return object->as<TransformComponentConstraintBase>()
                    ->doesCopy();
            case TransformComponentConstraintBase::minPropertyKey:
                return object->as<TransformComponentConstraintBase>()->min();
            case TransformComponentConstraintBase::maxPropertyKey:
                return object->as<TransformComponentConstraintBase>()->max();
            case TransformComponentConstraintYBase::doesCopyYPropertyKey:
                return object->as<TransformComponentConstraintYBase>()
                    ->doesCopyY();
            case TransformComponentConstraintYBase::minYPropertyKey:
                return object->as<TransformComponentConstraintYBase>()->minY();
            case TransformComponentConstraintYBase::maxYPropertyKey:
                return object->as<TransformComponentConstraintYBase>()->maxY();
            case IKConstraintBase::invertDirectionPropertyKey:
                return object->as<IKConstraintBase>()->invertDirection();
            case FollowPathConstraintBase::orientPropertyKey:
                return object->as<FollowPathConstraintBase>()->orient();
            case FollowPathConstraintBase::offsetPropertyKey:
                return object->as<FollowPathConstraintBase>()->offset();
            case ScrollConstraintBase::snapPropertyKey:
                return object->as<ScrollConstraintBase>()->snap();
            case ScrollConstraintBase::virtualizePropertyKey:
                return object->as<ScrollConstraintBase>()->virtualize();
            case ScrollConstraintBase::infinitePropertyKey:
                return object->as<ScrollConstraintBase>()->infinite();
            case ScrollBarConstraintBase::autoSizePropertyKey:
                return object->as<ScrollBarConstraintBase>()->autoSize();
            case AxisBase::normalizedPropertyKey:
                return object->as<AxisBase>()->normalized();
            case LayoutComponentStyleBase::intrinsicallySizedValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->intrinsicallySizedValue();
            case LayoutComponentStyleBase::linkCornerRadiusPropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->linkCornerRadius();
            case NestedSimpleAnimationBase::isPlayingPropertyKey:
                return object->as<NestedSimpleAnimationBase>()->isPlaying();
            case KeyFrameBoolBase::valuePropertyKey:
                return object->as<KeyFrameBoolBase>()->value();
            case ListenerAlignTargetBase::preserveOffsetPropertyKey:
                return object->as<ListenerAlignTargetBase>()->preserveOffset();
            case TransitionValueBooleanComparatorBase::valuePropertyKey:
                return object->as<TransitionValueBooleanComparatorBase>()
                    ->value();
            case NestedBoolBase::nestedValuePropertyKey:
                return object->as<NestedBoolBase>()->nestedValue();
            case LinearAnimationBase::enableWorkAreaPropertyKey:
                return object->as<LinearAnimationBase>()->enableWorkArea();
            case LinearAnimationBase::quantizePropertyKey:
                return object->as<LinearAnimationBase>()->quantize();
            case StateMachineBoolBase::valuePropertyKey:
                return object->as<StateMachineBoolBase>()->value();
            case ShapePaintBase::isVisiblePropertyKey:
                return object->as<ShapePaintBase>()->isVisible();
            case DashPathBase::offsetIsPercentagePropertyKey:
                return object->as<DashPathBase>()->offsetIsPercentage();
            case DashBase::lengthIsPercentagePropertyKey:
                return object->as<DashBase>()->lengthIsPercentage();
            case StrokeBase::transformAffectsStrokePropertyKey:
                return object->as<StrokeBase>()->transformAffectsStroke();
            case FeatherBase::innerPropertyKey:
                return object->as<FeatherBase>()->inner();
            case PathBase::isHolePropertyKey:
                return object->as<PathBase>()->isHole();
            case PointsCommonPathBase::isClosedPropertyKey:
                return object->as<PointsCommonPathBase>()->isClosed();
            case RectangleBase::linkCornerRadiusPropertyKey:
                return object->as<RectangleBase>()->linkCornerRadius();
            case ClippingShapeBase::isVisiblePropertyKey:
                return object->as<ClippingShapeBase>()->isVisible();
            case CustomPropertyBooleanBase::propertyValuePropertyKey:
                return object->as<CustomPropertyBooleanBase>()->propertyValue();
            case LayoutComponentBase::clipPropertyKey:
                return object->as<LayoutComponentBase>()->clip();
            case BindablePropertyBooleanBase::propertyValuePropertyKey:
                return object->as<BindablePropertyBooleanBase>()
                    ->propertyValue();
            case TextModifierRangeBase::clampPropertyKey:
                return object->as<TextModifierRangeBase>()->clamp();
            case TextFollowPathModifierBase::radialPropertyKey:
                return object->as<TextFollowPathModifierBase>()->radial();
            case TextFollowPathModifierBase::orientPropertyKey:
                return object->as<TextFollowPathModifierBase>()->orient();
            case TextBase::fitFromBaselinePropertyKey:
                return object->as<TextBase>()->fitFromBaseline();
        }
        return false;
    }
    static float getDouble(Core* object, int propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceNumberBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceNumberBase>()
                    ->propertyValue();
            case CustomPropertyNumberBase::propertyValuePropertyKey:
                return object->as<CustomPropertyNumberBase>()->propertyValue();
            case ConstraintBase::strengthPropertyKey:
                return object->as<ConstraintBase>()->strength();
            case DistanceConstraintBase::distancePropertyKey:
                return object->as<DistanceConstraintBase>()->distance();
            case TransformComponentConstraintBase::copyFactorPropertyKey:
                return object->as<TransformComponentConstraintBase>()
                    ->copyFactor();
            case TransformComponentConstraintBase::minValuePropertyKey:
                return object->as<TransformComponentConstraintBase>()
                    ->minValue();
            case TransformComponentConstraintBase::maxValuePropertyKey:
                return object->as<TransformComponentConstraintBase>()
                    ->maxValue();
            case TransformComponentConstraintYBase::copyFactorYPropertyKey:
                return object->as<TransformComponentConstraintYBase>()
                    ->copyFactorY();
            case TransformComponentConstraintYBase::minValueYPropertyKey:
                return object->as<TransformComponentConstraintYBase>()
                    ->minValueY();
            case TransformComponentConstraintYBase::maxValueYPropertyKey:
                return object->as<TransformComponentConstraintYBase>()
                    ->maxValueY();
            case FollowPathConstraintBase::distancePropertyKey:
                return object->as<FollowPathConstraintBase>()->distance();
            case ScrollConstraintBase::scrollOffsetXPropertyKey:
                return object->as<ScrollConstraintBase>()->scrollOffsetX();
            case ScrollConstraintBase::scrollOffsetYPropertyKey:
                return object->as<ScrollConstraintBase>()->scrollOffsetY();
            case ScrollConstraintBase::scrollPercentXPropertyKey:
                return object->as<ScrollConstraintBase>()->scrollPercentX();
            case ScrollConstraintBase::scrollPercentYPropertyKey:
                return object->as<ScrollConstraintBase>()->scrollPercentY();
            case ScrollConstraintBase::scrollIndexPropertyKey:
                return object->as<ScrollConstraintBase>()->scrollIndex();
            case ElasticScrollPhysicsBase::frictionPropertyKey:
                return object->as<ElasticScrollPhysicsBase>()->friction();
            case ElasticScrollPhysicsBase::speedMultiplierPropertyKey:
                return object->as<ElasticScrollPhysicsBase>()
                    ->speedMultiplier();
            case ElasticScrollPhysicsBase::elasticFactorPropertyKey:
                return object->as<ElasticScrollPhysicsBase>()->elasticFactor();
            case TransformConstraintBase::originXPropertyKey:
                return object->as<TransformConstraintBase>()->originX();
            case TransformConstraintBase::originYPropertyKey:
                return object->as<TransformConstraintBase>()->originY();
            case WorldTransformComponentBase::opacityPropertyKey:
                return object->as<WorldTransformComponentBase>()->opacity();
            case TransformComponentBase::rotationPropertyKey:
                return object->as<TransformComponentBase>()->rotation();
            case TransformComponentBase::scaleXPropertyKey:
                return object->as<TransformComponentBase>()->scaleX();
            case TransformComponentBase::scaleYPropertyKey:
                return object->as<TransformComponentBase>()->scaleY();
            case NodeBase::xPropertyKey:
            case NodeBase::xArtboardPropertyKey:
                return object->as<NodeBase>()->x();
            case NodeBase::yPropertyKey:
            case NodeBase::yArtboardPropertyKey:
                return object->as<NodeBase>()->y();
            case NodeBase::computedLocalXPropertyKey:
                return object->as<NodeBase>()->computedLocalX();
            case NodeBase::computedLocalYPropertyKey:
                return object->as<NodeBase>()->computedLocalY();
            case NodeBase::computedWorldXPropertyKey:
                return object->as<NodeBase>()->computedWorldX();
            case NodeBase::computedWorldYPropertyKey:
                return object->as<NodeBase>()->computedWorldY();
            case NodeBase::computedRootXPropertyKey:
                return object->as<NodeBase>()->computedRootX();
            case NodeBase::computedRootYPropertyKey:
                return object->as<NodeBase>()->computedRootY();
            case NodeBase::computedWidthPropertyKey:
                return object->as<NodeBase>()->computedWidth();
            case NodeBase::computedHeightPropertyKey:
                return object->as<NodeBase>()->computedHeight();
            case NestedArtboardLayoutBase::instanceWidthPropertyKey:
                return object->as<NestedArtboardLayoutBase>()->instanceWidth();
            case NestedArtboardLayoutBase::instanceHeightPropertyKey:
                return object->as<NestedArtboardLayoutBase>()->instanceHeight();
            case AxisBase::offsetPropertyKey:
                return object->as<AxisBase>()->offset();
            case LayoutComponentStyleBase::gapHorizontalPropertyKey:
                return object->as<LayoutComponentStyleBase>()->gapHorizontal();
            case LayoutComponentStyleBase::gapVerticalPropertyKey:
                return object->as<LayoutComponentStyleBase>()->gapVertical();
            case LayoutComponentStyleBase::maxWidthPropertyKey:
                return object->as<LayoutComponentStyleBase>()->maxWidth();
            case LayoutComponentStyleBase::maxHeightPropertyKey:
                return object->as<LayoutComponentStyleBase>()->maxHeight();
            case LayoutComponentStyleBase::minWidthPropertyKey:
                return object->as<LayoutComponentStyleBase>()->minWidth();
            case LayoutComponentStyleBase::minHeightPropertyKey:
                return object->as<LayoutComponentStyleBase>()->minHeight();
            case LayoutComponentStyleBase::borderLeftPropertyKey:
                return object->as<LayoutComponentStyleBase>()->borderLeft();
            case LayoutComponentStyleBase::borderRightPropertyKey:
                return object->as<LayoutComponentStyleBase>()->borderRight();
            case LayoutComponentStyleBase::borderTopPropertyKey:
                return object->as<LayoutComponentStyleBase>()->borderTop();
            case LayoutComponentStyleBase::borderBottomPropertyKey:
                return object->as<LayoutComponentStyleBase>()->borderBottom();
            case LayoutComponentStyleBase::marginLeftPropertyKey:
                return object->as<LayoutComponentStyleBase>()->marginLeft();
            case LayoutComponentStyleBase::marginRightPropertyKey:
                return object->as<LayoutComponentStyleBase>()->marginRight();
            case LayoutComponentStyleBase::marginTopPropertyKey:
                return object->as<LayoutComponentStyleBase>()->marginTop();
            case LayoutComponentStyleBase::marginBottomPropertyKey:
                return object->as<LayoutComponentStyleBase>()->marginBottom();
            case LayoutComponentStyleBase::paddingLeftPropertyKey:
                return object->as<LayoutComponentStyleBase>()->paddingLeft();
            case LayoutComponentStyleBase::paddingRightPropertyKey:
                return object->as<LayoutComponentStyleBase>()->paddingRight();
            case LayoutComponentStyleBase::paddingTopPropertyKey:
                return object->as<LayoutComponentStyleBase>()->paddingTop();
            case LayoutComponentStyleBase::paddingBottomPropertyKey:
                return object->as<LayoutComponentStyleBase>()->paddingBottom();
            case LayoutComponentStyleBase::positionLeftPropertyKey:
                return object->as<LayoutComponentStyleBase>()->positionLeft();
            case LayoutComponentStyleBase::positionRightPropertyKey:
                return object->as<LayoutComponentStyleBase>()->positionRight();
            case LayoutComponentStyleBase::positionTopPropertyKey:
                return object->as<LayoutComponentStyleBase>()->positionTop();
            case LayoutComponentStyleBase::positionBottomPropertyKey:
                return object->as<LayoutComponentStyleBase>()->positionBottom();
            case LayoutComponentStyleBase::flexPropertyKey:
                return object->as<LayoutComponentStyleBase>()->flex();
            case LayoutComponentStyleBase::flexGrowPropertyKey:
                return object->as<LayoutComponentStyleBase>()->flexGrow();
            case LayoutComponentStyleBase::flexShrinkPropertyKey:
                return object->as<LayoutComponentStyleBase>()->flexShrink();
            case LayoutComponentStyleBase::flexBasisPropertyKey:
                return object->as<LayoutComponentStyleBase>()->flexBasis();
            case LayoutComponentStyleBase::aspectRatioPropertyKey:
                return object->as<LayoutComponentStyleBase>()->aspectRatio();
            case LayoutComponentStyleBase::interpolationTimePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->interpolationTime();
            case LayoutComponentStyleBase::cornerRadiusTLPropertyKey:
                return object->as<LayoutComponentStyleBase>()->cornerRadiusTL();
            case LayoutComponentStyleBase::cornerRadiusTRPropertyKey:
                return object->as<LayoutComponentStyleBase>()->cornerRadiusTR();
            case LayoutComponentStyleBase::cornerRadiusBLPropertyKey:
                return object->as<LayoutComponentStyleBase>()->cornerRadiusBL();
            case LayoutComponentStyleBase::cornerRadiusBRPropertyKey:
                return object->as<LayoutComponentStyleBase>()->cornerRadiusBR();
            case NSlicedNodeBase::initialWidthPropertyKey:
                return object->as<NSlicedNodeBase>()->initialWidth();
            case NSlicedNodeBase::initialHeightPropertyKey:
                return object->as<NSlicedNodeBase>()->initialHeight();
            case NSlicedNodeBase::widthPropertyKey:
                return object->as<NSlicedNodeBase>()->width();
            case NSlicedNodeBase::heightPropertyKey:
                return object->as<NSlicedNodeBase>()->height();
            case ArtboardComponentListOverrideBase::instanceWidthPropertyKey:
                return object->as<ArtboardComponentListOverrideBase>()
                    ->instanceWidth();
            case ArtboardComponentListOverrideBase::instanceHeightPropertyKey:
                return object->as<ArtboardComponentListOverrideBase>()
                    ->instanceHeight();
            case NestedLinearAnimationBase::mixPropertyKey:
                return object->as<NestedLinearAnimationBase>()->mix();
            case NestedSimpleAnimationBase::speedPropertyKey:
                return object->as<NestedSimpleAnimationBase>()->speed();
            case AdvanceableStateBase::speedPropertyKey:
                return object->as<AdvanceableStateBase>()->speed();
            case BlendAnimationDirectBase::mixValuePropertyKey:
                return object->as<BlendAnimationDirectBase>()->mixValue();
            case StateMachineNumberBase::valuePropertyKey:
                return object->as<StateMachineNumberBase>()->value();
            case CubicInterpolatorBase::x1PropertyKey:
                return object->as<CubicInterpolatorBase>()->x1();
            case CubicInterpolatorBase::y1PropertyKey:
                return object->as<CubicInterpolatorBase>()->y1();
            case CubicInterpolatorBase::x2PropertyKey:
                return object->as<CubicInterpolatorBase>()->x2();
            case CubicInterpolatorBase::y2PropertyKey:
                return object->as<CubicInterpolatorBase>()->y2();
            case TransitionNumberConditionBase::valuePropertyKey:
                return object->as<TransitionNumberConditionBase>()->value();
            case CubicInterpolatorComponentBase::x1PropertyKey:
                return object->as<CubicInterpolatorComponentBase>()->x1();
            case CubicInterpolatorComponentBase::y1PropertyKey:
                return object->as<CubicInterpolatorComponentBase>()->y1();
            case CubicInterpolatorComponentBase::x2PropertyKey:
                return object->as<CubicInterpolatorComponentBase>()->x2();
            case CubicInterpolatorComponentBase::y2PropertyKey:
                return object->as<CubicInterpolatorComponentBase>()->y2();
            case ListenerNumberChangeBase::valuePropertyKey:
                return object->as<ListenerNumberChangeBase>()->value();
            case KeyFrameDoubleBase::valuePropertyKey:
                return object->as<KeyFrameDoubleBase>()->value();
            case LinearAnimationBase::speedPropertyKey:
                return object->as<LinearAnimationBase>()->speed();
            case TransitionValueNumberComparatorBase::valuePropertyKey:
                return object->as<TransitionValueNumberComparatorBase>()
                    ->value();
            case ElasticInterpolatorBase::amplitudePropertyKey:
                return object->as<ElasticInterpolatorBase>()->amplitude();
            case ElasticInterpolatorBase::periodPropertyKey:
                return object->as<ElasticInterpolatorBase>()->period();
            case NestedNumberBase::nestedValuePropertyKey:
                return object->as<NestedNumberBase>()->nestedValue();
            case NestedRemapAnimationBase::timePropertyKey:
                return object->as<NestedRemapAnimationBase>()->time();
            case BlendAnimation1DBase::valuePropertyKey:
                return object->as<BlendAnimation1DBase>()->value();
            case DashPathBase::offsetPropertyKey:
                return object->as<DashPathBase>()->offset();
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
            case DashBase::lengthPropertyKey:
                return object->as<DashBase>()->length();
            case StrokeBase::thicknessPropertyKey:
                return object->as<StrokeBase>()->thickness();
            case GradientStopBase::positionPropertyKey:
                return object->as<GradientStopBase>()->position();
            case FeatherBase::strengthPropertyKey:
                return object->as<FeatherBase>()->strength();
            case FeatherBase::offsetXPropertyKey:
                return object->as<FeatherBase>()->offsetX();
            case FeatherBase::offsetYPropertyKey:
                return object->as<FeatherBase>()->offsetY();
            case TrimPathBase::startPropertyKey:
                return object->as<TrimPathBase>()->start();
            case TrimPathBase::endPropertyKey:
                return object->as<TrimPathBase>()->end();
            case TrimPathBase::offsetPropertyKey:
                return object->as<TrimPathBase>()->offset();
            case VertexBase::xPropertyKey:
                return object->as<VertexBase>()->x();
            case VertexBase::yPropertyKey:
                return object->as<VertexBase>()->y();
            case MeshVertexBase::uPropertyKey:
                return object->as<MeshVertexBase>()->u();
            case MeshVertexBase::vPropertyKey:
                return object->as<MeshVertexBase>()->v();
            case ShapeBase::lengthPropertyKey:
                return object->as<ShapeBase>()->length();
            case StraightVertexBase::radiusPropertyKey:
                return object->as<StraightVertexBase>()->radius();
            case CubicAsymmetricVertexBase::rotationPropertyKey:
                return object->as<CubicAsymmetricVertexBase>()->rotation();
            case CubicAsymmetricVertexBase::inDistancePropertyKey:
                return object->as<CubicAsymmetricVertexBase>()->inDistance();
            case CubicAsymmetricVertexBase::outDistancePropertyKey:
                return object->as<CubicAsymmetricVertexBase>()->outDistance();
            case ParametricPathBase::widthPropertyKey:
                return object->as<ParametricPathBase>()->width();
            case ParametricPathBase::heightPropertyKey:
                return object->as<ParametricPathBase>()->height();
            case ParametricPathBase::originXPropertyKey:
                return object->as<ParametricPathBase>()->originX();
            case ParametricPathBase::originYPropertyKey:
                return object->as<ParametricPathBase>()->originY();
            case RectangleBase::cornerRadiusTLPropertyKey:
                return object->as<RectangleBase>()->cornerRadiusTL();
            case RectangleBase::cornerRadiusTRPropertyKey:
                return object->as<RectangleBase>()->cornerRadiusTR();
            case RectangleBase::cornerRadiusBLPropertyKey:
                return object->as<RectangleBase>()->cornerRadiusBL();
            case RectangleBase::cornerRadiusBRPropertyKey:
                return object->as<RectangleBase>()->cornerRadiusBR();
            case CubicMirroredVertexBase::rotationPropertyKey:
                return object->as<CubicMirroredVertexBase>()->rotation();
            case CubicMirroredVertexBase::distancePropertyKey:
                return object->as<CubicMirroredVertexBase>()->distance();
            case PolygonBase::cornerRadiusPropertyKey:
                return object->as<PolygonBase>()->cornerRadius();
            case StarBase::innerRadiusPropertyKey:
                return object->as<StarBase>()->innerRadius();
            case ImageBase::originXPropertyKey:
                return object->as<ImageBase>()->originX();
            case ImageBase::originYPropertyKey:
                return object->as<ImageBase>()->originY();
            case CubicDetachedVertexBase::inRotationPropertyKey:
                return object->as<CubicDetachedVertexBase>()->inRotation();
            case CubicDetachedVertexBase::inDistancePropertyKey:
                return object->as<CubicDetachedVertexBase>()->inDistance();
            case CubicDetachedVertexBase::outRotationPropertyKey:
                return object->as<CubicDetachedVertexBase>()->outRotation();
            case CubicDetachedVertexBase::outDistancePropertyKey:
                return object->as<CubicDetachedVertexBase>()->outDistance();
            case LayoutComponentBase::widthPropertyKey:
                return object->as<LayoutComponentBase>()->width();
            case LayoutComponentBase::heightPropertyKey:
                return object->as<LayoutComponentBase>()->height();
            case LayoutComponentBase::fractionalWidthPropertyKey:
                return object->as<LayoutComponentBase>()->fractionalWidth();
            case LayoutComponentBase::fractionalHeightPropertyKey:
                return object->as<LayoutComponentBase>()->fractionalHeight();
            case ArtboardBase::originXPropertyKey:
                return object->as<ArtboardBase>()->originX();
            case ArtboardBase::originYPropertyKey:
                return object->as<ArtboardBase>()->originY();
            case JoystickBase::xPropertyKey:
                return object->as<JoystickBase>()->x();
            case JoystickBase::yPropertyKey:
                return object->as<JoystickBase>()->y();
            case JoystickBase::posXPropertyKey:
                return object->as<JoystickBase>()->posX();
            case JoystickBase::posYPropertyKey:
                return object->as<JoystickBase>()->posY();
            case JoystickBase::originXPropertyKey:
                return object->as<JoystickBase>()->originX();
            case JoystickBase::originYPropertyKey:
                return object->as<JoystickBase>()->originY();
            case JoystickBase::widthPropertyKey:
                return object->as<JoystickBase>()->width();
            case JoystickBase::heightPropertyKey:
                return object->as<JoystickBase>()->height();
            case DataConverterOperationValueBase::operationValuePropertyKey:
                return object->as<DataConverterOperationValueBase>()
                    ->operationValue();
            case DataConverterRangeMapperBase::minInputPropertyKey:
                return object->as<DataConverterRangeMapperBase>()->minInput();
            case DataConverterRangeMapperBase::maxInputPropertyKey:
                return object->as<DataConverterRangeMapperBase>()->maxInput();
            case DataConverterRangeMapperBase::minOutputPropertyKey:
                return object->as<DataConverterRangeMapperBase>()->minOutput();
            case DataConverterRangeMapperBase::maxOutputPropertyKey:
                return object->as<DataConverterRangeMapperBase>()->maxOutput();
            case DataConverterInterpolatorBase::durationPropertyKey:
                return object->as<DataConverterInterpolatorBase>()->duration();
            case FormulaTokenValueBase::operationValuePropertyKey:
                return object->as<FormulaTokenValueBase>()->operationValue();
            case BindablePropertyNumberBase::propertyValuePropertyKey:
                return object->as<BindablePropertyNumberBase>()
                    ->propertyValue();
            case NestedArtboardLeafBase::alignmentXPropertyKey:
                return object->as<NestedArtboardLeafBase>()->alignmentX();
            case NestedArtboardLeafBase::alignmentYPropertyKey:
                return object->as<NestedArtboardLeafBase>()->alignmentY();
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
            case TextModifierRangeBase::modifyFromPropertyKey:
                return object->as<TextModifierRangeBase>()->modifyFrom();
            case TextModifierRangeBase::modifyToPropertyKey:
                return object->as<TextModifierRangeBase>()->modifyTo();
            case TextModifierRangeBase::strengthPropertyKey:
                return object->as<TextModifierRangeBase>()->strength();
            case TextModifierRangeBase::falloffFromPropertyKey:
                return object->as<TextModifierRangeBase>()->falloffFrom();
            case TextModifierRangeBase::falloffToPropertyKey:
                return object->as<TextModifierRangeBase>()->falloffTo();
            case TextModifierRangeBase::offsetPropertyKey:
                return object->as<TextModifierRangeBase>()->offset();
            case TextFollowPathModifierBase::startPropertyKey:
                return object->as<TextFollowPathModifierBase>()->start();
            case TextFollowPathModifierBase::endPropertyKey:
                return object->as<TextFollowPathModifierBase>()->end();
            case TextFollowPathModifierBase::strengthPropertyKey:
                return object->as<TextFollowPathModifierBase>()->strength();
            case TextFollowPathModifierBase::offsetPropertyKey:
                return object->as<TextFollowPathModifierBase>()->offset();
            case TextVariationModifierBase::axisValuePropertyKey:
                return object->as<TextVariationModifierBase>()->axisValue();
            case TextModifierGroupBase::originXPropertyKey:
                return object->as<TextModifierGroupBase>()->originX();
            case TextModifierGroupBase::originYPropertyKey:
                return object->as<TextModifierGroupBase>()->originY();
            case TextModifierGroupBase::opacityPropertyKey:
                return object->as<TextModifierGroupBase>()->opacity();
            case TextModifierGroupBase::xPropertyKey:
                return object->as<TextModifierGroupBase>()->x();
            case TextModifierGroupBase::yPropertyKey:
                return object->as<TextModifierGroupBase>()->y();
            case TextModifierGroupBase::rotationPropertyKey:
                return object->as<TextModifierGroupBase>()->rotation();
            case TextModifierGroupBase::scaleXPropertyKey:
                return object->as<TextModifierGroupBase>()->scaleX();
            case TextModifierGroupBase::scaleYPropertyKey:
                return object->as<TextModifierGroupBase>()->scaleY();
            case TextStyleBase::fontSizePropertyKey:
                return object->as<TextStyleBase>()->fontSize();
            case TextStyleBase::lineHeightPropertyKey:
                return object->as<TextStyleBase>()->lineHeight();
            case TextStyleBase::letterSpacingPropertyKey:
                return object->as<TextStyleBase>()->letterSpacing();
            case TextInputBase::selectionRadiusPropertyKey:
                return object->as<TextInputBase>()->selectionRadius();
            case TextStyleAxisBase::axisValuePropertyKey:
                return object->as<TextStyleAxisBase>()->axisValue();
            case TextBase::widthPropertyKey:
                return object->as<TextBase>()->width();
            case TextBase::heightPropertyKey:
                return object->as<TextBase>()->height();
            case TextBase::originXPropertyKey:
                return object->as<TextBase>()->originX();
            case TextBase::originYPropertyKey:
                return object->as<TextBase>()->originY();
            case TextBase::paragraphSpacingPropertyKey:
                return object->as<TextBase>()->paragraphSpacing();
            case DrawableAssetBase::heightPropertyKey:
                return object->as<DrawableAssetBase>()->height();
            case DrawableAssetBase::widthPropertyKey:
                return object->as<DrawableAssetBase>()->width();
            case ExportAudioBase::volumePropertyKey:
                return object->as<ExportAudioBase>()->volume();
        }
        return 0.0f;
    }
    static int propertyFieldId(int propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceListItemBase::viewModelIdPropertyKey:
            case ViewModelInstanceListItemBase::viewModelInstanceIdPropertyKey:
            case ViewModelPropertyBase::symbolTypeValuePropertyKey:
            case ViewModelInstanceValueBase::viewModelPropertyIdPropertyKey:
            case ViewModelPropertyEnumCustomBase::enumIdPropertyKey:
            case ViewModelInstanceEnumBase::propertyValuePropertyKey:
            case ViewModelInstanceArtboardBase::propertyValuePropertyKey:
            case ViewModelPropertyEnumSystemBase::enumTypePropertyKey:
            case DataEnumSystemBase::enumTypePropertyKey:
            case ViewModelPropertyViewModelBase::
                viewModelReferenceIdPropertyKey:
            case ComponentBase::parentIdPropertyKey:
            case ViewModelInstanceBase::viewModelIdPropertyKey:
            case ViewModelInstanceTriggerBase::propertyValuePropertyKey:
            case ViewModelInstanceSymbolListIndexBase::propertyValuePropertyKey:
            case ViewModelInstanceViewModelBase::propertyValuePropertyKey:
            case ViewModelInstanceAssetBase::propertyValuePropertyKey:
            case DrawTargetBase::drawableIdPropertyKey:
            case DrawTargetBase::placementValuePropertyKey:
            case TargetedConstraintBase::targetIdPropertyKey:
            case DistanceConstraintBase::modeValuePropertyKey:
            case TransformSpaceConstraintBase::sourceSpaceValuePropertyKey:
            case TransformSpaceConstraintBase::destSpaceValuePropertyKey:
            case TransformComponentConstraintBase::minMaxSpaceValuePropertyKey:
            case IKConstraintBase::parentBoneCountPropertyKey:
            case ScrollPhysicsBase::constraintIdPropertyKey:
            case DraggableConstraintBase::directionValuePropertyKey:
            case ScrollConstraintBase::physicsTypeValuePropertyKey:
            case ScrollConstraintBase::physicsIdPropertyKey:
            case ScrollBarConstraintBase::scrollConstraintIdPropertyKey:
            case DrawableBase::blendModeValuePropertyKey:
            case DrawableBase::drawableFlagsPropertyKey:
            case NestedArtboardBase::artboardIdPropertyKey:
            case ArtboardComponentListBase::listSourcePropertyKey:
            case NestedAnimationBase::animationIdPropertyKey:
            case SoloBase::activeComponentIdPropertyKey:
            case NestedArtboardLayoutBase::instanceWidthUnitsValuePropertyKey:
            case NestedArtboardLayoutBase::instanceHeightUnitsValuePropertyKey:
            case NestedArtboardLayoutBase::instanceWidthScaleTypePropertyKey:
            case NestedArtboardLayoutBase::instanceHeightScaleTypePropertyKey:
            case NSlicerTileModeBase::patchIndexPropertyKey:
            case NSlicerTileModeBase::stylePropertyKey:
            case LayoutComponentStyleBase::flexBasisUnitsValuePropertyKey:
            case LayoutComponentStyleBase::layoutWidthScaleTypePropertyKey:
            case LayoutComponentStyleBase::layoutHeightScaleTypePropertyKey:
            case LayoutComponentStyleBase::layoutAlignmentTypePropertyKey:
            case LayoutComponentStyleBase::animationStyleTypePropertyKey:
            case LayoutComponentStyleBase::interpolationTypePropertyKey:
            case LayoutComponentStyleBase::interpolatorIdPropertyKey:
            case LayoutComponentStyleBase::displayValuePropertyKey:
            case LayoutComponentStyleBase::positionTypeValuePropertyKey:
            case LayoutComponentStyleBase::flexDirectionValuePropertyKey:
            case LayoutComponentStyleBase::directionValuePropertyKey:
            case LayoutComponentStyleBase::alignContentValuePropertyKey:
            case LayoutComponentStyleBase::alignItemsValuePropertyKey:
            case LayoutComponentStyleBase::alignSelfValuePropertyKey:
            case LayoutComponentStyleBase::justifyContentValuePropertyKey:
            case LayoutComponentStyleBase::flexWrapValuePropertyKey:
            case LayoutComponentStyleBase::overflowValuePropertyKey:
            case LayoutComponentStyleBase::widthUnitsValuePropertyKey:
            case LayoutComponentStyleBase::heightUnitsValuePropertyKey:
            case LayoutComponentStyleBase::borderLeftUnitsValuePropertyKey:
            case LayoutComponentStyleBase::borderRightUnitsValuePropertyKey:
            case LayoutComponentStyleBase::borderTopUnitsValuePropertyKey:
            case LayoutComponentStyleBase::borderBottomUnitsValuePropertyKey:
            case LayoutComponentStyleBase::marginLeftUnitsValuePropertyKey:
            case LayoutComponentStyleBase::marginRightUnitsValuePropertyKey:
            case LayoutComponentStyleBase::marginTopUnitsValuePropertyKey:
            case LayoutComponentStyleBase::marginBottomUnitsValuePropertyKey:
            case LayoutComponentStyleBase::paddingLeftUnitsValuePropertyKey:
            case LayoutComponentStyleBase::paddingRightUnitsValuePropertyKey:
            case LayoutComponentStyleBase::paddingTopUnitsValuePropertyKey:
            case LayoutComponentStyleBase::paddingBottomUnitsValuePropertyKey:
            case LayoutComponentStyleBase::positionLeftUnitsValuePropertyKey:
            case LayoutComponentStyleBase::positionRightUnitsValuePropertyKey:
            case LayoutComponentStyleBase::positionTopUnitsValuePropertyKey:
            case LayoutComponentStyleBase::positionBottomUnitsValuePropertyKey:
            case LayoutComponentStyleBase::gapHorizontalUnitsValuePropertyKey:
            case LayoutComponentStyleBase::gapVerticalUnitsValuePropertyKey:
            case LayoutComponentStyleBase::minWidthUnitsValuePropertyKey:
            case LayoutComponentStyleBase::minHeightUnitsValuePropertyKey:
            case LayoutComponentStyleBase::maxWidthUnitsValuePropertyKey:
            case LayoutComponentStyleBase::maxHeightUnitsValuePropertyKey:
            case ArtboardComponentListOverrideBase::artboardIdPropertyKey:
            case ArtboardComponentListOverrideBase::
                instanceWidthUnitsValuePropertyKey:
            case ArtboardComponentListOverrideBase::
                instanceHeightUnitsValuePropertyKey:
            case ArtboardComponentListOverrideBase::
                instanceWidthScaleTypePropertyKey:
            case ArtboardComponentListOverrideBase::
                instanceHeightScaleTypePropertyKey:
            case ListenerFireEventBase::eventIdPropertyKey:
            case LayerStateBase::flagsPropertyKey:
            case StateMachineFireActionBase::occursValuePropertyKey:
            case TransitionValueTriggerComparatorBase::valuePropertyKey:
            case KeyFrameBase::framePropertyKey:
            case InterpolatingKeyFrameBase::interpolationTypePropertyKey:
            case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
            case KeyFrameUintBase::valuePropertyKey:
            case ListenerInputChangeBase::inputIdPropertyKey:
            case ListenerInputChangeBase::nestedInputIdPropertyKey:
            case AnimationStateBase::animationIdPropertyKey:
            case NestedInputBase::inputIdPropertyKey:
            case KeyedObjectBase::objectIdPropertyKey:
            case BlendAnimationBase::animationIdPropertyKey:
            case BlendAnimationDirectBase::inputIdPropertyKey:
            case BlendAnimationDirectBase::blendSourcePropertyKey:
            case TransitionInputConditionBase::inputIdPropertyKey:
            case KeyedPropertyBase::propertyKeyPropertyKey:
            case StateMachineListenerBase::targetIdPropertyKey:
            case StateMachineListenerBase::listenerTypeValuePropertyKey:
            case StateMachineListenerBase::eventIdPropertyKey:
            case TransitionPropertyArtboardComparatorBase::
                propertyTypePropertyKey:
            case KeyFrameIdBase::valuePropertyKey:
            case ListenerBoolChangeBase::valuePropertyKey:
            case ListenerAlignTargetBase::targetIdPropertyKey:
            case TransitionValueConditionBase::opValuePropertyKey:
            case TransitionViewModelConditionBase::opValuePropertyKey:
            case BlendState1DInputBase::inputIdPropertyKey:
            case TransitionValueIdComparatorBase::valuePropertyKey:
            case StateTransitionBase::stateToIdPropertyKey:
            case StateTransitionBase::flagsPropertyKey:
            case StateTransitionBase::durationPropertyKey:
            case StateTransitionBase::exitTimePropertyKey:
            case StateTransitionBase::interpolationTypePropertyKey:
            case StateTransitionBase::interpolatorIdPropertyKey:
            case StateTransitionBase::randomWeightPropertyKey:
            case StateMachineFireEventBase::eventIdPropertyKey:
            case LinearAnimationBase::fpsPropertyKey:
            case LinearAnimationBase::durationPropertyKey:
            case LinearAnimationBase::loopValuePropertyKey:
            case LinearAnimationBase::workStartPropertyKey:
            case LinearAnimationBase::workEndPropertyKey:
            case ElasticInterpolatorBase::easingValuePropertyKey:
            case BlendStateTransitionBase::exitBlendAnimationIdPropertyKey:
            case ShapePaintBase::blendModeValuePropertyKey:
            case StrokeBase::capPropertyKey:
            case StrokeBase::joinPropertyKey:
            case FeatherBase::spaceValuePropertyKey:
            case TrimPathBase::modeValuePropertyKey:
            case FillBase::fillRulePropertyKey:
            case PathBase::pathFlagsPropertyKey:
            case ListPathBase::listSourcePropertyKey:
            case ClippingShapeBase::sourceIdPropertyKey:
            case ClippingShapeBase::fillRulePropertyKey:
            case PolygonBase::pointsPropertyKey:
            case ImageBase::assetIdPropertyKey:
            case DrawRulesBase::drawTargetIdPropertyKey:
            case LayoutComponentBase::styleIdPropertyKey:
            case ArtboardBase::defaultStateMachineIdPropertyKey:
            case ArtboardBase::viewModelIdPropertyKey:
            case JoystickBase::xIdPropertyKey:
            case JoystickBase::yIdPropertyKey:
            case JoystickBase::joystickFlagsPropertyKey:
            case JoystickBase::handleSourceIdPropertyKey:
            case OpenUrlEventBase::targetValuePropertyKey:
            case BindablePropertyIdBase::propertyValuePropertyKey:
            case BindablePropertyIntegerBase::propertyValuePropertyKey:
            case DataBindBase::propertyKeyPropertyKey:
            case DataBindBase::flagsPropertyKey:
            case DataBindBase::converterIdPropertyKey:
            case DataConverterNumberToListBase::viewModelIdPropertyKey:
            case DataConverterOperationBase::operationTypePropertyKey:
            case DataConverterRangeMapperBase::interpolationTypePropertyKey:
            case DataConverterRangeMapperBase::interpolatorIdPropertyKey:
            case DataConverterRangeMapperBase::flagsPropertyKey:
            case DataConverterInterpolatorBase::interpolationTypePropertyKey:
            case DataConverterInterpolatorBase::interpolatorIdPropertyKey:
            case DataConverterGroupItemBase::converterIdPropertyKey:
            case DataConverterRounderBase::decimalsPropertyKey:
            case DataConverterStringPadBase::lengthPropertyKey:
            case DataConverterStringPadBase::padTypePropertyKey:
            case DataConverterStringTrimBase::trimTypePropertyKey:
            case FormulaTokenOperationBase::operationTypePropertyKey:
            case FormulaTokenFunctionBase::functionTypePropertyKey:
            case DataConverterToStringBase::flagsPropertyKey:
            case DataConverterToStringBase::decimalsPropertyKey:
            case BindablePropertyListBase::propertyValuePropertyKey:
            case BindablePropertyEnumBase::propertyValuePropertyKey:
            case NestedArtboardLeafBase::fitPropertyKey:
            case WeightBase::valuesPropertyKey:
            case WeightBase::indicesPropertyKey:
            case TendonBase::boneIdPropertyKey:
            case CubicWeightBase::inValuesPropertyKey:
            case CubicWeightBase::inIndicesPropertyKey:
            case CubicWeightBase::outValuesPropertyKey:
            case CubicWeightBase::outIndicesPropertyKey:
            case TextModifierRangeBase::unitsValuePropertyKey:
            case TextModifierRangeBase::typeValuePropertyKey:
            case TextModifierRangeBase::modeValuePropertyKey:
            case TextModifierRangeBase::runIdPropertyKey:
            case TextTargetModifierBase::targetIdPropertyKey:
            case TextStyleFeatureBase::tagPropertyKey:
            case TextStyleFeatureBase::featureValuePropertyKey:
            case TextVariationModifierBase::axisTagPropertyKey:
            case TextModifierGroupBase::modifierFlagsPropertyKey:
            case TextStyleBase::fontAssetIdPropertyKey:
            case TextStyleAxisBase::tagPropertyKey:
            case TextBase::alignValuePropertyKey:
            case TextBase::sizingValuePropertyKey:
            case TextBase::overflowValuePropertyKey:
            case TextBase::originValuePropertyKey:
            case TextBase::wrapValuePropertyKey:
            case TextBase::verticalAlignValuePropertyKey:
            case TextValueRunBase::styleIdPropertyKey:
            case CustomPropertyEnumBase::propertyValuePropertyKey:
            case CustomPropertyEnumBase::enumIdPropertyKey:
            case FileAssetBase::assetIdPropertyKey:
            case AudioEventBase::assetIdPropertyKey:
            case CustomPropertyTriggerBase::propertyValuePropertyKey:
                return CoreUintType::id;
            case ViewModelComponentBase::namePropertyKey:
            case DataEnumCustomBase::namePropertyKey:
            case ViewModelInstanceStringBase::propertyValuePropertyKey:
            case ComponentBase::namePropertyKey:
            case DataEnumValueBase::keyPropertyKey:
            case DataEnumValueBase::valuePropertyKey:
            case AnimationBase::namePropertyKey:
            case StateMachineComponentBase::namePropertyKey:
            case KeyFrameStringBase::valuePropertyKey:
            case TransitionValueStringComparatorBase::valuePropertyKey:
            case OpenUrlEventBase::urlPropertyKey:
            case DataConverterBase::namePropertyKey:
            case DataConverterStringPadBase::textPropertyKey:
            case DataConverterToStringBase::colorFormatPropertyKey:
            case BindablePropertyStringBase::propertyValuePropertyKey:
            case TextInputBase::textPropertyKey:
            case TextValueRunBase::textPropertyKey:
            case CustomPropertyStringBase::propertyValuePropertyKey:
            case AssetBase::namePropertyKey:
            case FileAssetBase::cdnBaseUrlPropertyKey:
                return CoreStringType::id;
            case ViewModelInstanceColorBase::propertyValuePropertyKey:
            case CustomPropertyColorBase::propertyValuePropertyKey:
            case KeyFrameColorBase::valuePropertyKey:
            case TransitionValueColorComparatorBase::valuePropertyKey:
            case SolidColorBase::colorValuePropertyKey:
            case GradientStopBase::colorValuePropertyKey:
            case BindablePropertyColorBase::propertyValuePropertyKey:
                return CoreColorType::id;
            case ViewModelInstanceBooleanBase::propertyValuePropertyKey:
            case TransformComponentConstraintBase::offsetPropertyKey:
            case TransformComponentConstraintBase::doesCopyPropertyKey:
            case TransformComponentConstraintBase::minPropertyKey:
            case TransformComponentConstraintBase::maxPropertyKey:
            case TransformComponentConstraintYBase::doesCopyYPropertyKey:
            case TransformComponentConstraintYBase::minYPropertyKey:
            case TransformComponentConstraintYBase::maxYPropertyKey:
            case IKConstraintBase::invertDirectionPropertyKey:
            case FollowPathConstraintBase::orientPropertyKey:
            case FollowPathConstraintBase::offsetPropertyKey:
            case ScrollConstraintBase::snapPropertyKey:
            case ScrollConstraintBase::virtualizePropertyKey:
            case ScrollConstraintBase::infinitePropertyKey:
            case ScrollBarConstraintBase::autoSizePropertyKey:
            case AxisBase::normalizedPropertyKey:
            case LayoutComponentStyleBase::intrinsicallySizedValuePropertyKey:
            case LayoutComponentStyleBase::linkCornerRadiusPropertyKey:
            case NestedSimpleAnimationBase::isPlayingPropertyKey:
            case KeyFrameBoolBase::valuePropertyKey:
            case ListenerAlignTargetBase::preserveOffsetPropertyKey:
            case TransitionValueBooleanComparatorBase::valuePropertyKey:
            case NestedBoolBase::nestedValuePropertyKey:
            case LinearAnimationBase::enableWorkAreaPropertyKey:
            case LinearAnimationBase::quantizePropertyKey:
            case StateMachineBoolBase::valuePropertyKey:
            case ShapePaintBase::isVisiblePropertyKey:
            case DashPathBase::offsetIsPercentagePropertyKey:
            case DashBase::lengthIsPercentagePropertyKey:
            case StrokeBase::transformAffectsStrokePropertyKey:
            case FeatherBase::innerPropertyKey:
            case PathBase::isHolePropertyKey:
            case PointsCommonPathBase::isClosedPropertyKey:
            case RectangleBase::linkCornerRadiusPropertyKey:
            case ClippingShapeBase::isVisiblePropertyKey:
            case CustomPropertyBooleanBase::propertyValuePropertyKey:
            case LayoutComponentBase::clipPropertyKey:
            case BindablePropertyBooleanBase::propertyValuePropertyKey:
            case TextModifierRangeBase::clampPropertyKey:
            case TextFollowPathModifierBase::radialPropertyKey:
            case TextFollowPathModifierBase::orientPropertyKey:
            case TextBase::fitFromBaselinePropertyKey:
                return CoreBoolType::id;
            case ViewModelInstanceNumberBase::propertyValuePropertyKey:
            case CustomPropertyNumberBase::propertyValuePropertyKey:
            case ConstraintBase::strengthPropertyKey:
            case DistanceConstraintBase::distancePropertyKey:
            case TransformComponentConstraintBase::copyFactorPropertyKey:
            case TransformComponentConstraintBase::minValuePropertyKey:
            case TransformComponentConstraintBase::maxValuePropertyKey:
            case TransformComponentConstraintYBase::copyFactorYPropertyKey:
            case TransformComponentConstraintYBase::minValueYPropertyKey:
            case TransformComponentConstraintYBase::maxValueYPropertyKey:
            case FollowPathConstraintBase::distancePropertyKey:
            case ScrollConstraintBase::scrollOffsetXPropertyKey:
            case ScrollConstraintBase::scrollOffsetYPropertyKey:
            case ScrollConstraintBase::scrollPercentXPropertyKey:
            case ScrollConstraintBase::scrollPercentYPropertyKey:
            case ScrollConstraintBase::scrollIndexPropertyKey:
            case ElasticScrollPhysicsBase::frictionPropertyKey:
            case ElasticScrollPhysicsBase::speedMultiplierPropertyKey:
            case ElasticScrollPhysicsBase::elasticFactorPropertyKey:
            case TransformConstraintBase::originXPropertyKey:
            case TransformConstraintBase::originYPropertyKey:
            case WorldTransformComponentBase::opacityPropertyKey:
            case TransformComponentBase::rotationPropertyKey:
            case TransformComponentBase::scaleXPropertyKey:
            case TransformComponentBase::scaleYPropertyKey:
            case NodeBase::xPropertyKey:
            case NodeBase::xArtboardPropertyKey:
            case NodeBase::yPropertyKey:
            case NodeBase::yArtboardPropertyKey:
            case NodeBase::computedLocalXPropertyKey:
            case NodeBase::computedLocalYPropertyKey:
            case NodeBase::computedWorldXPropertyKey:
            case NodeBase::computedWorldYPropertyKey:
            case NodeBase::computedRootXPropertyKey:
            case NodeBase::computedRootYPropertyKey:
            case NodeBase::computedWidthPropertyKey:
            case NodeBase::computedHeightPropertyKey:
            case NestedArtboardLayoutBase::instanceWidthPropertyKey:
            case NestedArtboardLayoutBase::instanceHeightPropertyKey:
            case AxisBase::offsetPropertyKey:
            case LayoutComponentStyleBase::gapHorizontalPropertyKey:
            case LayoutComponentStyleBase::gapVerticalPropertyKey:
            case LayoutComponentStyleBase::maxWidthPropertyKey:
            case LayoutComponentStyleBase::maxHeightPropertyKey:
            case LayoutComponentStyleBase::minWidthPropertyKey:
            case LayoutComponentStyleBase::minHeightPropertyKey:
            case LayoutComponentStyleBase::borderLeftPropertyKey:
            case LayoutComponentStyleBase::borderRightPropertyKey:
            case LayoutComponentStyleBase::borderTopPropertyKey:
            case LayoutComponentStyleBase::borderBottomPropertyKey:
            case LayoutComponentStyleBase::marginLeftPropertyKey:
            case LayoutComponentStyleBase::marginRightPropertyKey:
            case LayoutComponentStyleBase::marginTopPropertyKey:
            case LayoutComponentStyleBase::marginBottomPropertyKey:
            case LayoutComponentStyleBase::paddingLeftPropertyKey:
            case LayoutComponentStyleBase::paddingRightPropertyKey:
            case LayoutComponentStyleBase::paddingTopPropertyKey:
            case LayoutComponentStyleBase::paddingBottomPropertyKey:
            case LayoutComponentStyleBase::positionLeftPropertyKey:
            case LayoutComponentStyleBase::positionRightPropertyKey:
            case LayoutComponentStyleBase::positionTopPropertyKey:
            case LayoutComponentStyleBase::positionBottomPropertyKey:
            case LayoutComponentStyleBase::flexPropertyKey:
            case LayoutComponentStyleBase::flexGrowPropertyKey:
            case LayoutComponentStyleBase::flexShrinkPropertyKey:
            case LayoutComponentStyleBase::flexBasisPropertyKey:
            case LayoutComponentStyleBase::aspectRatioPropertyKey:
            case LayoutComponentStyleBase::interpolationTimePropertyKey:
            case LayoutComponentStyleBase::cornerRadiusTLPropertyKey:
            case LayoutComponentStyleBase::cornerRadiusTRPropertyKey:
            case LayoutComponentStyleBase::cornerRadiusBLPropertyKey:
            case LayoutComponentStyleBase::cornerRadiusBRPropertyKey:
            case NSlicedNodeBase::initialWidthPropertyKey:
            case NSlicedNodeBase::initialHeightPropertyKey:
            case NSlicedNodeBase::widthPropertyKey:
            case NSlicedNodeBase::heightPropertyKey:
            case ArtboardComponentListOverrideBase::instanceWidthPropertyKey:
            case ArtboardComponentListOverrideBase::instanceHeightPropertyKey:
            case NestedLinearAnimationBase::mixPropertyKey:
            case NestedSimpleAnimationBase::speedPropertyKey:
            case AdvanceableStateBase::speedPropertyKey:
            case BlendAnimationDirectBase::mixValuePropertyKey:
            case StateMachineNumberBase::valuePropertyKey:
            case CubicInterpolatorBase::x1PropertyKey:
            case CubicInterpolatorBase::y1PropertyKey:
            case CubicInterpolatorBase::x2PropertyKey:
            case CubicInterpolatorBase::y2PropertyKey:
            case TransitionNumberConditionBase::valuePropertyKey:
            case CubicInterpolatorComponentBase::x1PropertyKey:
            case CubicInterpolatorComponentBase::y1PropertyKey:
            case CubicInterpolatorComponentBase::x2PropertyKey:
            case CubicInterpolatorComponentBase::y2PropertyKey:
            case ListenerNumberChangeBase::valuePropertyKey:
            case KeyFrameDoubleBase::valuePropertyKey:
            case LinearAnimationBase::speedPropertyKey:
            case TransitionValueNumberComparatorBase::valuePropertyKey:
            case ElasticInterpolatorBase::amplitudePropertyKey:
            case ElasticInterpolatorBase::periodPropertyKey:
            case NestedNumberBase::nestedValuePropertyKey:
            case NestedRemapAnimationBase::timePropertyKey:
            case BlendAnimation1DBase::valuePropertyKey:
            case DashPathBase::offsetPropertyKey:
            case LinearGradientBase::startXPropertyKey:
            case LinearGradientBase::startYPropertyKey:
            case LinearGradientBase::endXPropertyKey:
            case LinearGradientBase::endYPropertyKey:
            case LinearGradientBase::opacityPropertyKey:
            case DashBase::lengthPropertyKey:
            case StrokeBase::thicknessPropertyKey:
            case GradientStopBase::positionPropertyKey:
            case FeatherBase::strengthPropertyKey:
            case FeatherBase::offsetXPropertyKey:
            case FeatherBase::offsetYPropertyKey:
            case TrimPathBase::startPropertyKey:
            case TrimPathBase::endPropertyKey:
            case TrimPathBase::offsetPropertyKey:
            case VertexBase::xPropertyKey:
            case VertexBase::yPropertyKey:
            case MeshVertexBase::uPropertyKey:
            case MeshVertexBase::vPropertyKey:
            case ShapeBase::lengthPropertyKey:
            case StraightVertexBase::radiusPropertyKey:
            case CubicAsymmetricVertexBase::rotationPropertyKey:
            case CubicAsymmetricVertexBase::inDistancePropertyKey:
            case CubicAsymmetricVertexBase::outDistancePropertyKey:
            case ParametricPathBase::widthPropertyKey:
            case ParametricPathBase::heightPropertyKey:
            case ParametricPathBase::originXPropertyKey:
            case ParametricPathBase::originYPropertyKey:
            case RectangleBase::cornerRadiusTLPropertyKey:
            case RectangleBase::cornerRadiusTRPropertyKey:
            case RectangleBase::cornerRadiusBLPropertyKey:
            case RectangleBase::cornerRadiusBRPropertyKey:
            case CubicMirroredVertexBase::rotationPropertyKey:
            case CubicMirroredVertexBase::distancePropertyKey:
            case PolygonBase::cornerRadiusPropertyKey:
            case StarBase::innerRadiusPropertyKey:
            case ImageBase::originXPropertyKey:
            case ImageBase::originYPropertyKey:
            case CubicDetachedVertexBase::inRotationPropertyKey:
            case CubicDetachedVertexBase::inDistancePropertyKey:
            case CubicDetachedVertexBase::outRotationPropertyKey:
            case CubicDetachedVertexBase::outDistancePropertyKey:
            case LayoutComponentBase::widthPropertyKey:
            case LayoutComponentBase::heightPropertyKey:
            case LayoutComponentBase::fractionalWidthPropertyKey:
            case LayoutComponentBase::fractionalHeightPropertyKey:
            case ArtboardBase::originXPropertyKey:
            case ArtboardBase::originYPropertyKey:
            case JoystickBase::xPropertyKey:
            case JoystickBase::yPropertyKey:
            case JoystickBase::posXPropertyKey:
            case JoystickBase::posYPropertyKey:
            case JoystickBase::originXPropertyKey:
            case JoystickBase::originYPropertyKey:
            case JoystickBase::widthPropertyKey:
            case JoystickBase::heightPropertyKey:
            case DataConverterOperationValueBase::operationValuePropertyKey:
            case DataConverterRangeMapperBase::minInputPropertyKey:
            case DataConverterRangeMapperBase::maxInputPropertyKey:
            case DataConverterRangeMapperBase::minOutputPropertyKey:
            case DataConverterRangeMapperBase::maxOutputPropertyKey:
            case DataConverterInterpolatorBase::durationPropertyKey:
            case FormulaTokenValueBase::operationValuePropertyKey:
            case BindablePropertyNumberBase::propertyValuePropertyKey:
            case NestedArtboardLeafBase::alignmentXPropertyKey:
            case NestedArtboardLeafBase::alignmentYPropertyKey:
            case BoneBase::lengthPropertyKey:
            case RootBoneBase::xPropertyKey:
            case RootBoneBase::yPropertyKey:
            case SkinBase::xxPropertyKey:
            case SkinBase::yxPropertyKey:
            case SkinBase::xyPropertyKey:
            case SkinBase::yyPropertyKey:
            case SkinBase::txPropertyKey:
            case SkinBase::tyPropertyKey:
            case TendonBase::xxPropertyKey:
            case TendonBase::yxPropertyKey:
            case TendonBase::xyPropertyKey:
            case TendonBase::yyPropertyKey:
            case TendonBase::txPropertyKey:
            case TendonBase::tyPropertyKey:
            case TextModifierRangeBase::modifyFromPropertyKey:
            case TextModifierRangeBase::modifyToPropertyKey:
            case TextModifierRangeBase::strengthPropertyKey:
            case TextModifierRangeBase::falloffFromPropertyKey:
            case TextModifierRangeBase::falloffToPropertyKey:
            case TextModifierRangeBase::offsetPropertyKey:
            case TextFollowPathModifierBase::startPropertyKey:
            case TextFollowPathModifierBase::endPropertyKey:
            case TextFollowPathModifierBase::strengthPropertyKey:
            case TextFollowPathModifierBase::offsetPropertyKey:
            case TextVariationModifierBase::axisValuePropertyKey:
            case TextModifierGroupBase::originXPropertyKey:
            case TextModifierGroupBase::originYPropertyKey:
            case TextModifierGroupBase::opacityPropertyKey:
            case TextModifierGroupBase::xPropertyKey:
            case TextModifierGroupBase::yPropertyKey:
            case TextModifierGroupBase::rotationPropertyKey:
            case TextModifierGroupBase::scaleXPropertyKey:
            case TextModifierGroupBase::scaleYPropertyKey:
            case TextStyleBase::fontSizePropertyKey:
            case TextStyleBase::lineHeightPropertyKey:
            case TextStyleBase::letterSpacingPropertyKey:
            case TextInputBase::selectionRadiusPropertyKey:
            case TextStyleAxisBase::axisValuePropertyKey:
            case TextBase::widthPropertyKey:
            case TextBase::heightPropertyKey:
            case TextBase::originXPropertyKey:
            case TextBase::originYPropertyKey:
            case TextBase::paragraphSpacingPropertyKey:
            case DrawableAssetBase::heightPropertyKey:
            case DrawableAssetBase::widthPropertyKey:
            case ExportAudioBase::volumePropertyKey:
                return CoreDoubleType::id;
            case NestedArtboardBase::dataBindPathIdsPropertyKey:
            case StateMachineFireTriggerBase::viewModelPathIdsPropertyKey:
            case StateMachineListenerBase::viewModelPathIdsPropertyKey:
            case MeshBase::triangleIndexBytesPropertyKey:
            case DataConverterOperationViewModelBase::sourcePathIdsPropertyKey:
            case DataBindContextBase::sourcePathIdsPropertyKey:
            case FileAssetBase::cdnUuidPropertyKey:
            case FileAssetContentsBase::bytesPropertyKey:
                return CoreBytesType::id;
            default:
                return -1;
        }
    }
    static bool isCallback(uint32_t propertyKey)
    {
        switch (propertyKey)
        {
            case NestedTriggerBase::firePropertyKey:
            case EventBase::triggerPropertyKey:
            case CustomPropertyTriggerBase::firePropertyKey:
                return true;
            default:
                return false;
        }
    }
    static bool objectSupportsProperty(Core* object, uint32_t propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceListItemBase::viewModelIdPropertyKey:
                return object->is<ViewModelInstanceListItemBase>();
            case ViewModelInstanceListItemBase::viewModelInstanceIdPropertyKey:
                return object->is<ViewModelInstanceListItemBase>();
            case ViewModelPropertyBase::symbolTypeValuePropertyKey:
                return object->is<ViewModelPropertyBase>();
            case ViewModelInstanceValueBase::viewModelPropertyIdPropertyKey:
                return object->is<ViewModelInstanceValueBase>();
            case ViewModelPropertyEnumCustomBase::enumIdPropertyKey:
                return object->is<ViewModelPropertyEnumCustomBase>();
            case ViewModelInstanceEnumBase::propertyValuePropertyKey:
                return object->is<ViewModelInstanceEnumBase>();
            case ViewModelInstanceArtboardBase::propertyValuePropertyKey:
                return object->is<ViewModelInstanceArtboardBase>();
            case ViewModelPropertyEnumSystemBase::enumTypePropertyKey:
                return object->is<ViewModelPropertyEnumSystemBase>();
            case DataEnumSystemBase::enumTypePropertyKey:
                return object->is<DataEnumSystemBase>();
            case ViewModelPropertyViewModelBase::
                viewModelReferenceIdPropertyKey:
                return object->is<ViewModelPropertyViewModelBase>();
            case ComponentBase::parentIdPropertyKey:
                return object->is<ComponentBase>();
            case ViewModelInstanceBase::viewModelIdPropertyKey:
                return object->is<ViewModelInstanceBase>();
            case ViewModelInstanceTriggerBase::propertyValuePropertyKey:
                return object->is<ViewModelInstanceTriggerBase>();
            case ViewModelInstanceSymbolListIndexBase::propertyValuePropertyKey:
                return object->is<ViewModelInstanceSymbolListIndexBase>();
            case ViewModelInstanceViewModelBase::propertyValuePropertyKey:
                return object->is<ViewModelInstanceViewModelBase>();
            case ViewModelInstanceAssetBase::propertyValuePropertyKey:
                return object->is<ViewModelInstanceAssetBase>();
            case DrawTargetBase::drawableIdPropertyKey:
                return object->is<DrawTargetBase>();
            case DrawTargetBase::placementValuePropertyKey:
                return object->is<DrawTargetBase>();
            case TargetedConstraintBase::targetIdPropertyKey:
                return object->is<TargetedConstraintBase>();
            case DistanceConstraintBase::modeValuePropertyKey:
                return object->is<DistanceConstraintBase>();
            case TransformSpaceConstraintBase::sourceSpaceValuePropertyKey:
                return object->is<TransformSpaceConstraintBase>();
            case TransformSpaceConstraintBase::destSpaceValuePropertyKey:
                return object->is<TransformSpaceConstraintBase>();
            case TransformComponentConstraintBase::minMaxSpaceValuePropertyKey:
                return object->is<TransformComponentConstraintBase>();
            case IKConstraintBase::parentBoneCountPropertyKey:
                return object->is<IKConstraintBase>();
            case ScrollPhysicsBase::constraintIdPropertyKey:
                return object->is<ScrollPhysicsBase>();
            case DraggableConstraintBase::directionValuePropertyKey:
                return object->is<DraggableConstraintBase>();
            case ScrollConstraintBase::physicsTypeValuePropertyKey:
                return object->is<ScrollConstraintBase>();
            case ScrollConstraintBase::physicsIdPropertyKey:
                return object->is<ScrollConstraintBase>();
            case ScrollBarConstraintBase::scrollConstraintIdPropertyKey:
                return object->is<ScrollBarConstraintBase>();
            case DrawableBase::blendModeValuePropertyKey:
                return object->is<DrawableBase>();
            case DrawableBase::drawableFlagsPropertyKey:
                return object->is<DrawableBase>();
            case NestedArtboardBase::artboardIdPropertyKey:
                return object->is<NestedArtboardBase>();
            case ArtboardComponentListBase::listSourcePropertyKey:
                return object->is<ArtboardComponentListBase>();
            case NestedAnimationBase::animationIdPropertyKey:
                return object->is<NestedAnimationBase>();
            case SoloBase::activeComponentIdPropertyKey:
                return object->is<SoloBase>();
            case NestedArtboardLayoutBase::instanceWidthUnitsValuePropertyKey:
                return object->is<NestedArtboardLayoutBase>();
            case NestedArtboardLayoutBase::instanceHeightUnitsValuePropertyKey:
                return object->is<NestedArtboardLayoutBase>();
            case NestedArtboardLayoutBase::instanceWidthScaleTypePropertyKey:
                return object->is<NestedArtboardLayoutBase>();
            case NestedArtboardLayoutBase::instanceHeightScaleTypePropertyKey:
                return object->is<NestedArtboardLayoutBase>();
            case NSlicerTileModeBase::patchIndexPropertyKey:
                return object->is<NSlicerTileModeBase>();
            case NSlicerTileModeBase::stylePropertyKey:
                return object->is<NSlicerTileModeBase>();
            case LayoutComponentStyleBase::flexBasisUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::layoutWidthScaleTypePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::layoutHeightScaleTypePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::layoutAlignmentTypePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::animationStyleTypePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::interpolationTypePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::interpolatorIdPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::displayValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::positionTypeValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::flexDirectionValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::directionValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::alignContentValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::alignItemsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::alignSelfValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::justifyContentValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::flexWrapValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::overflowValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::widthUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::heightUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::borderLeftUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::borderRightUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::borderTopUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::borderBottomUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::marginLeftUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::marginRightUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::marginTopUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::marginBottomUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::paddingLeftUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::paddingRightUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::paddingTopUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::paddingBottomUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::positionLeftUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::positionRightUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::positionTopUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::positionBottomUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::gapHorizontalUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::gapVerticalUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::minWidthUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::minHeightUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::maxWidthUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::maxHeightUnitsValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case ArtboardComponentListOverrideBase::artboardIdPropertyKey:
                return object->is<ArtboardComponentListOverrideBase>();
            case ArtboardComponentListOverrideBase::
                instanceWidthUnitsValuePropertyKey:
                return object->is<ArtboardComponentListOverrideBase>();
            case ArtboardComponentListOverrideBase::
                instanceHeightUnitsValuePropertyKey:
                return object->is<ArtboardComponentListOverrideBase>();
            case ArtboardComponentListOverrideBase::
                instanceWidthScaleTypePropertyKey:
                return object->is<ArtboardComponentListOverrideBase>();
            case ArtboardComponentListOverrideBase::
                instanceHeightScaleTypePropertyKey:
                return object->is<ArtboardComponentListOverrideBase>();
            case ListenerFireEventBase::eventIdPropertyKey:
                return object->is<ListenerFireEventBase>();
            case LayerStateBase::flagsPropertyKey:
                return object->is<LayerStateBase>();
            case StateMachineFireActionBase::occursValuePropertyKey:
                return object->is<StateMachineFireActionBase>();
            case TransitionValueTriggerComparatorBase::valuePropertyKey:
                return object->is<TransitionValueTriggerComparatorBase>();
            case KeyFrameBase::framePropertyKey:
                return object->is<KeyFrameBase>();
            case InterpolatingKeyFrameBase::interpolationTypePropertyKey:
                return object->is<InterpolatingKeyFrameBase>();
            case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
                return object->is<InterpolatingKeyFrameBase>();
            case KeyFrameUintBase::valuePropertyKey:
                return object->is<KeyFrameUintBase>();
            case ListenerInputChangeBase::inputIdPropertyKey:
                return object->is<ListenerInputChangeBase>();
            case ListenerInputChangeBase::nestedInputIdPropertyKey:
                return object->is<ListenerInputChangeBase>();
            case AnimationStateBase::animationIdPropertyKey:
                return object->is<AnimationStateBase>();
            case NestedInputBase::inputIdPropertyKey:
                return object->is<NestedInputBase>();
            case KeyedObjectBase::objectIdPropertyKey:
                return object->is<KeyedObjectBase>();
            case BlendAnimationBase::animationIdPropertyKey:
                return object->is<BlendAnimationBase>();
            case BlendAnimationDirectBase::inputIdPropertyKey:
                return object->is<BlendAnimationDirectBase>();
            case BlendAnimationDirectBase::blendSourcePropertyKey:
                return object->is<BlendAnimationDirectBase>();
            case TransitionInputConditionBase::inputIdPropertyKey:
                return object->is<TransitionInputConditionBase>();
            case KeyedPropertyBase::propertyKeyPropertyKey:
                return object->is<KeyedPropertyBase>();
            case StateMachineListenerBase::targetIdPropertyKey:
                return object->is<StateMachineListenerBase>();
            case StateMachineListenerBase::listenerTypeValuePropertyKey:
                return object->is<StateMachineListenerBase>();
            case StateMachineListenerBase::eventIdPropertyKey:
                return object->is<StateMachineListenerBase>();
            case TransitionPropertyArtboardComparatorBase::
                propertyTypePropertyKey:
                return object->is<TransitionPropertyArtboardComparatorBase>();
            case KeyFrameIdBase::valuePropertyKey:
                return object->is<KeyFrameIdBase>();
            case ListenerBoolChangeBase::valuePropertyKey:
                return object->is<ListenerBoolChangeBase>();
            case ListenerAlignTargetBase::targetIdPropertyKey:
                return object->is<ListenerAlignTargetBase>();
            case TransitionValueConditionBase::opValuePropertyKey:
                return object->is<TransitionValueConditionBase>();
            case TransitionViewModelConditionBase::opValuePropertyKey:
                return object->is<TransitionViewModelConditionBase>();
            case BlendState1DInputBase::inputIdPropertyKey:
                return object->is<BlendState1DInputBase>();
            case TransitionValueIdComparatorBase::valuePropertyKey:
                return object->is<TransitionValueIdComparatorBase>();
            case StateTransitionBase::stateToIdPropertyKey:
                return object->is<StateTransitionBase>();
            case StateTransitionBase::flagsPropertyKey:
                return object->is<StateTransitionBase>();
            case StateTransitionBase::durationPropertyKey:
                return object->is<StateTransitionBase>();
            case StateTransitionBase::exitTimePropertyKey:
                return object->is<StateTransitionBase>();
            case StateTransitionBase::interpolationTypePropertyKey:
                return object->is<StateTransitionBase>();
            case StateTransitionBase::interpolatorIdPropertyKey:
                return object->is<StateTransitionBase>();
            case StateTransitionBase::randomWeightPropertyKey:
                return object->is<StateTransitionBase>();
            case StateMachineFireEventBase::eventIdPropertyKey:
                return object->is<StateMachineFireEventBase>();
            case LinearAnimationBase::fpsPropertyKey:
                return object->is<LinearAnimationBase>();
            case LinearAnimationBase::durationPropertyKey:
                return object->is<LinearAnimationBase>();
            case LinearAnimationBase::loopValuePropertyKey:
                return object->is<LinearAnimationBase>();
            case LinearAnimationBase::workStartPropertyKey:
                return object->is<LinearAnimationBase>();
            case LinearAnimationBase::workEndPropertyKey:
                return object->is<LinearAnimationBase>();
            case ElasticInterpolatorBase::easingValuePropertyKey:
                return object->is<ElasticInterpolatorBase>();
            case BlendStateTransitionBase::exitBlendAnimationIdPropertyKey:
                return object->is<BlendStateTransitionBase>();
            case ShapePaintBase::blendModeValuePropertyKey:
                return object->is<ShapePaintBase>();
            case StrokeBase::capPropertyKey:
                return object->is<StrokeBase>();
            case StrokeBase::joinPropertyKey:
                return object->is<StrokeBase>();
            case FeatherBase::spaceValuePropertyKey:
                return object->is<FeatherBase>();
            case TrimPathBase::modeValuePropertyKey:
                return object->is<TrimPathBase>();
            case FillBase::fillRulePropertyKey:
                return object->is<FillBase>();
            case PathBase::pathFlagsPropertyKey:
                return object->is<PathBase>();
            case ListPathBase::listSourcePropertyKey:
                return object->is<ListPathBase>();
            case ClippingShapeBase::sourceIdPropertyKey:
                return object->is<ClippingShapeBase>();
            case ClippingShapeBase::fillRulePropertyKey:
                return object->is<ClippingShapeBase>();
            case PolygonBase::pointsPropertyKey:
                return object->is<PolygonBase>();
            case ImageBase::assetIdPropertyKey:
                return object->is<ImageBase>();
            case DrawRulesBase::drawTargetIdPropertyKey:
                return object->is<DrawRulesBase>();
            case LayoutComponentBase::styleIdPropertyKey:
                return object->is<LayoutComponentBase>();
            case ArtboardBase::defaultStateMachineIdPropertyKey:
                return object->is<ArtboardBase>();
            case ArtboardBase::viewModelIdPropertyKey:
                return object->is<ArtboardBase>();
            case JoystickBase::xIdPropertyKey:
                return object->is<JoystickBase>();
            case JoystickBase::yIdPropertyKey:
                return object->is<JoystickBase>();
            case JoystickBase::joystickFlagsPropertyKey:
                return object->is<JoystickBase>();
            case JoystickBase::handleSourceIdPropertyKey:
                return object->is<JoystickBase>();
            case OpenUrlEventBase::targetValuePropertyKey:
                return object->is<OpenUrlEventBase>();
            case BindablePropertyIdBase::propertyValuePropertyKey:
                return object->is<BindablePropertyIdBase>();
            case BindablePropertyIntegerBase::propertyValuePropertyKey:
                return object->is<BindablePropertyIntegerBase>();
            case DataBindBase::propertyKeyPropertyKey:
                return object->is<DataBindBase>();
            case DataBindBase::flagsPropertyKey:
                return object->is<DataBindBase>();
            case DataBindBase::converterIdPropertyKey:
                return object->is<DataBindBase>();
            case DataConverterNumberToListBase::viewModelIdPropertyKey:
                return object->is<DataConverterNumberToListBase>();
            case DataConverterOperationBase::operationTypePropertyKey:
                return object->is<DataConverterOperationBase>();
            case DataConverterRangeMapperBase::interpolationTypePropertyKey:
                return object->is<DataConverterRangeMapperBase>();
            case DataConverterRangeMapperBase::interpolatorIdPropertyKey:
                return object->is<DataConverterRangeMapperBase>();
            case DataConverterRangeMapperBase::flagsPropertyKey:
                return object->is<DataConverterRangeMapperBase>();
            case DataConverterInterpolatorBase::interpolationTypePropertyKey:
                return object->is<DataConverterInterpolatorBase>();
            case DataConverterInterpolatorBase::interpolatorIdPropertyKey:
                return object->is<DataConverterInterpolatorBase>();
            case DataConverterGroupItemBase::converterIdPropertyKey:
                return object->is<DataConverterGroupItemBase>();
            case DataConverterRounderBase::decimalsPropertyKey:
                return object->is<DataConverterRounderBase>();
            case DataConverterStringPadBase::lengthPropertyKey:
                return object->is<DataConverterStringPadBase>();
            case DataConverterStringPadBase::padTypePropertyKey:
                return object->is<DataConverterStringPadBase>();
            case DataConverterStringTrimBase::trimTypePropertyKey:
                return object->is<DataConverterStringTrimBase>();
            case FormulaTokenOperationBase::operationTypePropertyKey:
                return object->is<FormulaTokenOperationBase>();
            case FormulaTokenFunctionBase::functionTypePropertyKey:
                return object->is<FormulaTokenFunctionBase>();
            case DataConverterToStringBase::flagsPropertyKey:
                return object->is<DataConverterToStringBase>();
            case DataConverterToStringBase::decimalsPropertyKey:
                return object->is<DataConverterToStringBase>();
            case BindablePropertyListBase::propertyValuePropertyKey:
                return object->is<BindablePropertyListBase>();
            case BindablePropertyEnumBase::propertyValuePropertyKey:
                return object->is<BindablePropertyEnumBase>();
            case NestedArtboardLeafBase::fitPropertyKey:
                return object->is<NestedArtboardLeafBase>();
            case WeightBase::valuesPropertyKey:
                return object->is<WeightBase>();
            case WeightBase::indicesPropertyKey:
                return object->is<WeightBase>();
            case TendonBase::boneIdPropertyKey:
                return object->is<TendonBase>();
            case CubicWeightBase::inValuesPropertyKey:
                return object->is<CubicWeightBase>();
            case CubicWeightBase::inIndicesPropertyKey:
                return object->is<CubicWeightBase>();
            case CubicWeightBase::outValuesPropertyKey:
                return object->is<CubicWeightBase>();
            case CubicWeightBase::outIndicesPropertyKey:
                return object->is<CubicWeightBase>();
            case TextModifierRangeBase::unitsValuePropertyKey:
                return object->is<TextModifierRangeBase>();
            case TextModifierRangeBase::typeValuePropertyKey:
                return object->is<TextModifierRangeBase>();
            case TextModifierRangeBase::modeValuePropertyKey:
                return object->is<TextModifierRangeBase>();
            case TextModifierRangeBase::runIdPropertyKey:
                return object->is<TextModifierRangeBase>();
            case TextTargetModifierBase::targetIdPropertyKey:
                return object->is<TextTargetModifierBase>();
            case TextStyleFeatureBase::tagPropertyKey:
                return object->is<TextStyleFeatureBase>();
            case TextStyleFeatureBase::featureValuePropertyKey:
                return object->is<TextStyleFeatureBase>();
            case TextVariationModifierBase::axisTagPropertyKey:
                return object->is<TextVariationModifierBase>();
            case TextModifierGroupBase::modifierFlagsPropertyKey:
                return object->is<TextModifierGroupBase>();
            case TextStyleBase::fontAssetIdPropertyKey:
                return object->is<TextStyleBase>();
            case TextStyleAxisBase::tagPropertyKey:
                return object->is<TextStyleAxisBase>();
            case TextBase::alignValuePropertyKey:
                return object->is<TextBase>();
            case TextBase::sizingValuePropertyKey:
                return object->is<TextBase>();
            case TextBase::overflowValuePropertyKey:
                return object->is<TextBase>();
            case TextBase::originValuePropertyKey:
                return object->is<TextBase>();
            case TextBase::wrapValuePropertyKey:
                return object->is<TextBase>();
            case TextBase::verticalAlignValuePropertyKey:
                return object->is<TextBase>();
            case TextValueRunBase::styleIdPropertyKey:
                return object->is<TextValueRunBase>();
            case CustomPropertyEnumBase::propertyValuePropertyKey:
                return object->is<CustomPropertyEnumBase>();
            case CustomPropertyEnumBase::enumIdPropertyKey:
                return object->is<CustomPropertyEnumBase>();
            case FileAssetBase::assetIdPropertyKey:
                return object->is<FileAssetBase>();
            case AudioEventBase::assetIdPropertyKey:
                return object->is<AudioEventBase>();
            case CustomPropertyTriggerBase::propertyValuePropertyKey:
                return object->is<CustomPropertyTriggerBase>();
            case ViewModelComponentBase::namePropertyKey:
                return object->is<ViewModelComponentBase>();
            case DataEnumCustomBase::namePropertyKey:
                return object->is<DataEnumCustomBase>();
            case ViewModelInstanceStringBase::propertyValuePropertyKey:
                return object->is<ViewModelInstanceStringBase>();
            case ComponentBase::namePropertyKey:
                return object->is<ComponentBase>();
            case DataEnumValueBase::keyPropertyKey:
                return object->is<DataEnumValueBase>();
            case DataEnumValueBase::valuePropertyKey:
                return object->is<DataEnumValueBase>();
            case AnimationBase::namePropertyKey:
                return object->is<AnimationBase>();
            case StateMachineComponentBase::namePropertyKey:
                return object->is<StateMachineComponentBase>();
            case KeyFrameStringBase::valuePropertyKey:
                return object->is<KeyFrameStringBase>();
            case TransitionValueStringComparatorBase::valuePropertyKey:
                return object->is<TransitionValueStringComparatorBase>();
            case OpenUrlEventBase::urlPropertyKey:
                return object->is<OpenUrlEventBase>();
            case DataConverterBase::namePropertyKey:
                return object->is<DataConverterBase>();
            case DataConverterStringPadBase::textPropertyKey:
                return object->is<DataConverterStringPadBase>();
            case DataConverterToStringBase::colorFormatPropertyKey:
                return object->is<DataConverterToStringBase>();
            case BindablePropertyStringBase::propertyValuePropertyKey:
                return object->is<BindablePropertyStringBase>();
            case TextInputBase::textPropertyKey:
                return object->is<TextInputBase>();
            case TextValueRunBase::textPropertyKey:
                return object->is<TextValueRunBase>();
            case CustomPropertyStringBase::propertyValuePropertyKey:
                return object->is<CustomPropertyStringBase>();
            case AssetBase::namePropertyKey:
                return object->is<AssetBase>();
            case FileAssetBase::cdnBaseUrlPropertyKey:
                return object->is<FileAssetBase>();
            case ViewModelInstanceColorBase::propertyValuePropertyKey:
                return object->is<ViewModelInstanceColorBase>();
            case CustomPropertyColorBase::propertyValuePropertyKey:
                return object->is<CustomPropertyColorBase>();
            case KeyFrameColorBase::valuePropertyKey:
                return object->is<KeyFrameColorBase>();
            case TransitionValueColorComparatorBase::valuePropertyKey:
                return object->is<TransitionValueColorComparatorBase>();
            case SolidColorBase::colorValuePropertyKey:
                return object->is<SolidColorBase>();
            case GradientStopBase::colorValuePropertyKey:
                return object->is<GradientStopBase>();
            case BindablePropertyColorBase::propertyValuePropertyKey:
                return object->is<BindablePropertyColorBase>();
            case ViewModelInstanceBooleanBase::propertyValuePropertyKey:
                return object->is<ViewModelInstanceBooleanBase>();
            case TransformComponentConstraintBase::offsetPropertyKey:
                return object->is<TransformComponentConstraintBase>();
            case TransformComponentConstraintBase::doesCopyPropertyKey:
                return object->is<TransformComponentConstraintBase>();
            case TransformComponentConstraintBase::minPropertyKey:
                return object->is<TransformComponentConstraintBase>();
            case TransformComponentConstraintBase::maxPropertyKey:
                return object->is<TransformComponentConstraintBase>();
            case TransformComponentConstraintYBase::doesCopyYPropertyKey:
                return object->is<TransformComponentConstraintYBase>();
            case TransformComponentConstraintYBase::minYPropertyKey:
                return object->is<TransformComponentConstraintYBase>();
            case TransformComponentConstraintYBase::maxYPropertyKey:
                return object->is<TransformComponentConstraintYBase>();
            case IKConstraintBase::invertDirectionPropertyKey:
                return object->is<IKConstraintBase>();
            case FollowPathConstraintBase::orientPropertyKey:
                return object->is<FollowPathConstraintBase>();
            case FollowPathConstraintBase::offsetPropertyKey:
                return object->is<FollowPathConstraintBase>();
            case ScrollConstraintBase::snapPropertyKey:
                return object->is<ScrollConstraintBase>();
            case ScrollConstraintBase::virtualizePropertyKey:
                return object->is<ScrollConstraintBase>();
            case ScrollConstraintBase::infinitePropertyKey:
                return object->is<ScrollConstraintBase>();
            case ScrollBarConstraintBase::autoSizePropertyKey:
                return object->is<ScrollBarConstraintBase>();
            case AxisBase::normalizedPropertyKey:
                return object->is<AxisBase>();
            case LayoutComponentStyleBase::intrinsicallySizedValuePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::linkCornerRadiusPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case NestedSimpleAnimationBase::isPlayingPropertyKey:
                return object->is<NestedSimpleAnimationBase>();
            case KeyFrameBoolBase::valuePropertyKey:
                return object->is<KeyFrameBoolBase>();
            case ListenerAlignTargetBase::preserveOffsetPropertyKey:
                return object->is<ListenerAlignTargetBase>();
            case TransitionValueBooleanComparatorBase::valuePropertyKey:
                return object->is<TransitionValueBooleanComparatorBase>();
            case NestedBoolBase::nestedValuePropertyKey:
                return object->is<NestedBoolBase>();
            case LinearAnimationBase::enableWorkAreaPropertyKey:
                return object->is<LinearAnimationBase>();
            case LinearAnimationBase::quantizePropertyKey:
                return object->is<LinearAnimationBase>();
            case StateMachineBoolBase::valuePropertyKey:
                return object->is<StateMachineBoolBase>();
            case ShapePaintBase::isVisiblePropertyKey:
                return object->is<ShapePaintBase>();
            case DashPathBase::offsetIsPercentagePropertyKey:
                return object->is<DashPathBase>();
            case DashBase::lengthIsPercentagePropertyKey:
                return object->is<DashBase>();
            case StrokeBase::transformAffectsStrokePropertyKey:
                return object->is<StrokeBase>();
            case FeatherBase::innerPropertyKey:
                return object->is<FeatherBase>();
            case PathBase::isHolePropertyKey:
                return object->is<PathBase>();
            case PointsCommonPathBase::isClosedPropertyKey:
                return object->is<PointsCommonPathBase>();
            case RectangleBase::linkCornerRadiusPropertyKey:
                return object->is<RectangleBase>();
            case ClippingShapeBase::isVisiblePropertyKey:
                return object->is<ClippingShapeBase>();
            case CustomPropertyBooleanBase::propertyValuePropertyKey:
                return object->is<CustomPropertyBooleanBase>();
            case LayoutComponentBase::clipPropertyKey:
                return object->is<LayoutComponentBase>();
            case BindablePropertyBooleanBase::propertyValuePropertyKey:
                return object->is<BindablePropertyBooleanBase>();
            case TextModifierRangeBase::clampPropertyKey:
                return object->is<TextModifierRangeBase>();
            case TextFollowPathModifierBase::radialPropertyKey:
                return object->is<TextFollowPathModifierBase>();
            case TextFollowPathModifierBase::orientPropertyKey:
                return object->is<TextFollowPathModifierBase>();
            case TextBase::fitFromBaselinePropertyKey:
                return object->is<TextBase>();
            case ViewModelInstanceNumberBase::propertyValuePropertyKey:
                return object->is<ViewModelInstanceNumberBase>();
            case CustomPropertyNumberBase::propertyValuePropertyKey:
                return object->is<CustomPropertyNumberBase>();
            case ConstraintBase::strengthPropertyKey:
                return object->is<ConstraintBase>();
            case DistanceConstraintBase::distancePropertyKey:
                return object->is<DistanceConstraintBase>();
            case TransformComponentConstraintBase::copyFactorPropertyKey:
                return object->is<TransformComponentConstraintBase>();
            case TransformComponentConstraintBase::minValuePropertyKey:
                return object->is<TransformComponentConstraintBase>();
            case TransformComponentConstraintBase::maxValuePropertyKey:
                return object->is<TransformComponentConstraintBase>();
            case TransformComponentConstraintYBase::copyFactorYPropertyKey:
                return object->is<TransformComponentConstraintYBase>();
            case TransformComponentConstraintYBase::minValueYPropertyKey:
                return object->is<TransformComponentConstraintYBase>();
            case TransformComponentConstraintYBase::maxValueYPropertyKey:
                return object->is<TransformComponentConstraintYBase>();
            case FollowPathConstraintBase::distancePropertyKey:
                return object->is<FollowPathConstraintBase>();
            case ScrollConstraintBase::scrollOffsetXPropertyKey:
                return object->is<ScrollConstraintBase>();
            case ScrollConstraintBase::scrollOffsetYPropertyKey:
                return object->is<ScrollConstraintBase>();
            case ScrollConstraintBase::scrollPercentXPropertyKey:
                return object->is<ScrollConstraintBase>();
            case ScrollConstraintBase::scrollPercentYPropertyKey:
                return object->is<ScrollConstraintBase>();
            case ScrollConstraintBase::scrollIndexPropertyKey:
                return object->is<ScrollConstraintBase>();
            case ElasticScrollPhysicsBase::frictionPropertyKey:
                return object->is<ElasticScrollPhysicsBase>();
            case ElasticScrollPhysicsBase::speedMultiplierPropertyKey:
                return object->is<ElasticScrollPhysicsBase>();
            case ElasticScrollPhysicsBase::elasticFactorPropertyKey:
                return object->is<ElasticScrollPhysicsBase>();
            case TransformConstraintBase::originXPropertyKey:
                return object->is<TransformConstraintBase>();
            case TransformConstraintBase::originYPropertyKey:
                return object->is<TransformConstraintBase>();
            case WorldTransformComponentBase::opacityPropertyKey:
                return object->is<WorldTransformComponentBase>();
            case TransformComponentBase::rotationPropertyKey:
                return object->is<TransformComponentBase>();
            case TransformComponentBase::scaleXPropertyKey:
                return object->is<TransformComponentBase>();
            case TransformComponentBase::scaleYPropertyKey:
                return object->is<TransformComponentBase>();
            case NodeBase::xPropertyKey:
            case NodeBase::xArtboardPropertyKey:
                return object->is<NodeBase>();
            case NodeBase::yPropertyKey:
            case NodeBase::yArtboardPropertyKey:
                return object->is<NodeBase>();
            case NodeBase::computedLocalXPropertyKey:
                return object->is<NodeBase>();
            case NodeBase::computedLocalYPropertyKey:
                return object->is<NodeBase>();
            case NodeBase::computedWorldXPropertyKey:
                return object->is<NodeBase>();
            case NodeBase::computedWorldYPropertyKey:
                return object->is<NodeBase>();
            case NodeBase::computedRootXPropertyKey:
                return object->is<NodeBase>();
            case NodeBase::computedRootYPropertyKey:
                return object->is<NodeBase>();
            case NodeBase::computedWidthPropertyKey:
                return object->is<NodeBase>();
            case NodeBase::computedHeightPropertyKey:
                return object->is<NodeBase>();
            case NestedArtboardLayoutBase::instanceWidthPropertyKey:
                return object->is<NestedArtboardLayoutBase>();
            case NestedArtboardLayoutBase::instanceHeightPropertyKey:
                return object->is<NestedArtboardLayoutBase>();
            case AxisBase::offsetPropertyKey:
                return object->is<AxisBase>();
            case LayoutComponentStyleBase::gapHorizontalPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::gapVerticalPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::maxWidthPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::maxHeightPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::minWidthPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::minHeightPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::borderLeftPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::borderRightPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::borderTopPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::borderBottomPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::marginLeftPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::marginRightPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::marginTopPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::marginBottomPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::paddingLeftPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::paddingRightPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::paddingTopPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::paddingBottomPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::positionLeftPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::positionRightPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::positionTopPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::positionBottomPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::flexPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::flexGrowPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::flexShrinkPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::flexBasisPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::aspectRatioPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::interpolationTimePropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::cornerRadiusTLPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::cornerRadiusTRPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::cornerRadiusBLPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case LayoutComponentStyleBase::cornerRadiusBRPropertyKey:
                return object->is<LayoutComponentStyleBase>();
            case NSlicedNodeBase::initialWidthPropertyKey:
                return object->is<NSlicedNodeBase>();
            case NSlicedNodeBase::initialHeightPropertyKey:
                return object->is<NSlicedNodeBase>();
            case NSlicedNodeBase::widthPropertyKey:
                return object->is<NSlicedNodeBase>();
            case NSlicedNodeBase::heightPropertyKey:
                return object->is<NSlicedNodeBase>();
            case ArtboardComponentListOverrideBase::instanceWidthPropertyKey:
                return object->is<ArtboardComponentListOverrideBase>();
            case ArtboardComponentListOverrideBase::instanceHeightPropertyKey:
                return object->is<ArtboardComponentListOverrideBase>();
            case NestedLinearAnimationBase::mixPropertyKey:
                return object->is<NestedLinearAnimationBase>();
            case NestedSimpleAnimationBase::speedPropertyKey:
                return object->is<NestedSimpleAnimationBase>();
            case AdvanceableStateBase::speedPropertyKey:
                return object->is<AdvanceableStateBase>();
            case BlendAnimationDirectBase::mixValuePropertyKey:
                return object->is<BlendAnimationDirectBase>();
            case StateMachineNumberBase::valuePropertyKey:
                return object->is<StateMachineNumberBase>();
            case CubicInterpolatorBase::x1PropertyKey:
                return object->is<CubicInterpolatorBase>();
            case CubicInterpolatorBase::y1PropertyKey:
                return object->is<CubicInterpolatorBase>();
            case CubicInterpolatorBase::x2PropertyKey:
                return object->is<CubicInterpolatorBase>();
            case CubicInterpolatorBase::y2PropertyKey:
                return object->is<CubicInterpolatorBase>();
            case TransitionNumberConditionBase::valuePropertyKey:
                return object->is<TransitionNumberConditionBase>();
            case CubicInterpolatorComponentBase::x1PropertyKey:
                return object->is<CubicInterpolatorComponentBase>();
            case CubicInterpolatorComponentBase::y1PropertyKey:
                return object->is<CubicInterpolatorComponentBase>();
            case CubicInterpolatorComponentBase::x2PropertyKey:
                return object->is<CubicInterpolatorComponentBase>();
            case CubicInterpolatorComponentBase::y2PropertyKey:
                return object->is<CubicInterpolatorComponentBase>();
            case ListenerNumberChangeBase::valuePropertyKey:
                return object->is<ListenerNumberChangeBase>();
            case KeyFrameDoubleBase::valuePropertyKey:
                return object->is<KeyFrameDoubleBase>();
            case LinearAnimationBase::speedPropertyKey:
                return object->is<LinearAnimationBase>();
            case TransitionValueNumberComparatorBase::valuePropertyKey:
                return object->is<TransitionValueNumberComparatorBase>();
            case ElasticInterpolatorBase::amplitudePropertyKey:
                return object->is<ElasticInterpolatorBase>();
            case ElasticInterpolatorBase::periodPropertyKey:
                return object->is<ElasticInterpolatorBase>();
            case NestedNumberBase::nestedValuePropertyKey:
                return object->is<NestedNumberBase>();
            case NestedRemapAnimationBase::timePropertyKey:
                return object->is<NestedRemapAnimationBase>();
            case BlendAnimation1DBase::valuePropertyKey:
                return object->is<BlendAnimation1DBase>();
            case DashPathBase::offsetPropertyKey:
                return object->is<DashPathBase>();
            case LinearGradientBase::startXPropertyKey:
                return object->is<LinearGradientBase>();
            case LinearGradientBase::startYPropertyKey:
                return object->is<LinearGradientBase>();
            case LinearGradientBase::endXPropertyKey:
                return object->is<LinearGradientBase>();
            case LinearGradientBase::endYPropertyKey:
                return object->is<LinearGradientBase>();
            case LinearGradientBase::opacityPropertyKey:
                return object->is<LinearGradientBase>();
            case DashBase::lengthPropertyKey:
                return object->is<DashBase>();
            case StrokeBase::thicknessPropertyKey:
                return object->is<StrokeBase>();
            case GradientStopBase::positionPropertyKey:
                return object->is<GradientStopBase>();
            case FeatherBase::strengthPropertyKey:
                return object->is<FeatherBase>();
            case FeatherBase::offsetXPropertyKey:
                return object->is<FeatherBase>();
            case FeatherBase::offsetYPropertyKey:
                return object->is<FeatherBase>();
            case TrimPathBase::startPropertyKey:
                return object->is<TrimPathBase>();
            case TrimPathBase::endPropertyKey:
                return object->is<TrimPathBase>();
            case TrimPathBase::offsetPropertyKey:
                return object->is<TrimPathBase>();
            case VertexBase::xPropertyKey:
                return object->is<VertexBase>();
            case VertexBase::yPropertyKey:
                return object->is<VertexBase>();
            case MeshVertexBase::uPropertyKey:
                return object->is<MeshVertexBase>();
            case MeshVertexBase::vPropertyKey:
                return object->is<MeshVertexBase>();
            case ShapeBase::lengthPropertyKey:
                return object->is<ShapeBase>();
            case StraightVertexBase::radiusPropertyKey:
                return object->is<StraightVertexBase>();
            case CubicAsymmetricVertexBase::rotationPropertyKey:
                return object->is<CubicAsymmetricVertexBase>();
            case CubicAsymmetricVertexBase::inDistancePropertyKey:
                return object->is<CubicAsymmetricVertexBase>();
            case CubicAsymmetricVertexBase::outDistancePropertyKey:
                return object->is<CubicAsymmetricVertexBase>();
            case ParametricPathBase::widthPropertyKey:
                return object->is<ParametricPathBase>();
            case ParametricPathBase::heightPropertyKey:
                return object->is<ParametricPathBase>();
            case ParametricPathBase::originXPropertyKey:
                return object->is<ParametricPathBase>();
            case ParametricPathBase::originYPropertyKey:
                return object->is<ParametricPathBase>();
            case RectangleBase::cornerRadiusTLPropertyKey:
                return object->is<RectangleBase>();
            case RectangleBase::cornerRadiusTRPropertyKey:
                return object->is<RectangleBase>();
            case RectangleBase::cornerRadiusBLPropertyKey:
                return object->is<RectangleBase>();
            case RectangleBase::cornerRadiusBRPropertyKey:
                return object->is<RectangleBase>();
            case CubicMirroredVertexBase::rotationPropertyKey:
                return object->is<CubicMirroredVertexBase>();
            case CubicMirroredVertexBase::distancePropertyKey:
                return object->is<CubicMirroredVertexBase>();
            case PolygonBase::cornerRadiusPropertyKey:
                return object->is<PolygonBase>();
            case StarBase::innerRadiusPropertyKey:
                return object->is<StarBase>();
            case ImageBase::originXPropertyKey:
                return object->is<ImageBase>();
            case ImageBase::originYPropertyKey:
                return object->is<ImageBase>();
            case CubicDetachedVertexBase::inRotationPropertyKey:
                return object->is<CubicDetachedVertexBase>();
            case CubicDetachedVertexBase::inDistancePropertyKey:
                return object->is<CubicDetachedVertexBase>();
            case CubicDetachedVertexBase::outRotationPropertyKey:
                return object->is<CubicDetachedVertexBase>();
            case CubicDetachedVertexBase::outDistancePropertyKey:
                return object->is<CubicDetachedVertexBase>();
            case LayoutComponentBase::widthPropertyKey:
                return object->is<LayoutComponentBase>();
            case LayoutComponentBase::heightPropertyKey:
                return object->is<LayoutComponentBase>();
            case LayoutComponentBase::fractionalWidthPropertyKey:
                return object->is<LayoutComponentBase>();
            case LayoutComponentBase::fractionalHeightPropertyKey:
                return object->is<LayoutComponentBase>();
            case ArtboardBase::originXPropertyKey:
                return object->is<ArtboardBase>();
            case ArtboardBase::originYPropertyKey:
                return object->is<ArtboardBase>();
            case JoystickBase::xPropertyKey:
                return object->is<JoystickBase>();
            case JoystickBase::yPropertyKey:
                return object->is<JoystickBase>();
            case JoystickBase::posXPropertyKey:
                return object->is<JoystickBase>();
            case JoystickBase::posYPropertyKey:
                return object->is<JoystickBase>();
            case JoystickBase::originXPropertyKey:
                return object->is<JoystickBase>();
            case JoystickBase::originYPropertyKey:
                return object->is<JoystickBase>();
            case JoystickBase::widthPropertyKey:
                return object->is<JoystickBase>();
            case JoystickBase::heightPropertyKey:
                return object->is<JoystickBase>();
            case DataConverterOperationValueBase::operationValuePropertyKey:
                return object->is<DataConverterOperationValueBase>();
            case DataConverterRangeMapperBase::minInputPropertyKey:
                return object->is<DataConverterRangeMapperBase>();
            case DataConverterRangeMapperBase::maxInputPropertyKey:
                return object->is<DataConverterRangeMapperBase>();
            case DataConverterRangeMapperBase::minOutputPropertyKey:
                return object->is<DataConverterRangeMapperBase>();
            case DataConverterRangeMapperBase::maxOutputPropertyKey:
                return object->is<DataConverterRangeMapperBase>();
            case DataConverterInterpolatorBase::durationPropertyKey:
                return object->is<DataConverterInterpolatorBase>();
            case FormulaTokenValueBase::operationValuePropertyKey:
                return object->is<FormulaTokenValueBase>();
            case BindablePropertyNumberBase::propertyValuePropertyKey:
                return object->is<BindablePropertyNumberBase>();
            case NestedArtboardLeafBase::alignmentXPropertyKey:
                return object->is<NestedArtboardLeafBase>();
            case NestedArtboardLeafBase::alignmentYPropertyKey:
                return object->is<NestedArtboardLeafBase>();
            case BoneBase::lengthPropertyKey:
                return object->is<BoneBase>();
            case RootBoneBase::xPropertyKey:
                return object->is<RootBoneBase>();
            case RootBoneBase::yPropertyKey:
                return object->is<RootBoneBase>();
            case SkinBase::xxPropertyKey:
                return object->is<SkinBase>();
            case SkinBase::yxPropertyKey:
                return object->is<SkinBase>();
            case SkinBase::xyPropertyKey:
                return object->is<SkinBase>();
            case SkinBase::yyPropertyKey:
                return object->is<SkinBase>();
            case SkinBase::txPropertyKey:
                return object->is<SkinBase>();
            case SkinBase::tyPropertyKey:
                return object->is<SkinBase>();
            case TendonBase::xxPropertyKey:
                return object->is<TendonBase>();
            case TendonBase::yxPropertyKey:
                return object->is<TendonBase>();
            case TendonBase::xyPropertyKey:
                return object->is<TendonBase>();
            case TendonBase::yyPropertyKey:
                return object->is<TendonBase>();
            case TendonBase::txPropertyKey:
                return object->is<TendonBase>();
            case TendonBase::tyPropertyKey:
                return object->is<TendonBase>();
            case TextModifierRangeBase::modifyFromPropertyKey:
                return object->is<TextModifierRangeBase>();
            case TextModifierRangeBase::modifyToPropertyKey:
                return object->is<TextModifierRangeBase>();
            case TextModifierRangeBase::strengthPropertyKey:
                return object->is<TextModifierRangeBase>();
            case TextModifierRangeBase::falloffFromPropertyKey:
                return object->is<TextModifierRangeBase>();
            case TextModifierRangeBase::falloffToPropertyKey:
                return object->is<TextModifierRangeBase>();
            case TextModifierRangeBase::offsetPropertyKey:
                return object->is<TextModifierRangeBase>();
            case TextFollowPathModifierBase::startPropertyKey:
                return object->is<TextFollowPathModifierBase>();
            case TextFollowPathModifierBase::endPropertyKey:
                return object->is<TextFollowPathModifierBase>();
            case TextFollowPathModifierBase::strengthPropertyKey:
                return object->is<TextFollowPathModifierBase>();
            case TextFollowPathModifierBase::offsetPropertyKey:
                return object->is<TextFollowPathModifierBase>();
            case TextVariationModifierBase::axisValuePropertyKey:
                return object->is<TextVariationModifierBase>();
            case TextModifierGroupBase::originXPropertyKey:
                return object->is<TextModifierGroupBase>();
            case TextModifierGroupBase::originYPropertyKey:
                return object->is<TextModifierGroupBase>();
            case TextModifierGroupBase::opacityPropertyKey:
                return object->is<TextModifierGroupBase>();
            case TextModifierGroupBase::xPropertyKey:
                return object->is<TextModifierGroupBase>();
            case TextModifierGroupBase::yPropertyKey:
                return object->is<TextModifierGroupBase>();
            case TextModifierGroupBase::rotationPropertyKey:
                return object->is<TextModifierGroupBase>();
            case TextModifierGroupBase::scaleXPropertyKey:
                return object->is<TextModifierGroupBase>();
            case TextModifierGroupBase::scaleYPropertyKey:
                return object->is<TextModifierGroupBase>();
            case TextStyleBase::fontSizePropertyKey:
                return object->is<TextStyleBase>();
            case TextStyleBase::lineHeightPropertyKey:
                return object->is<TextStyleBase>();
            case TextStyleBase::letterSpacingPropertyKey:
                return object->is<TextStyleBase>();
            case TextInputBase::selectionRadiusPropertyKey:
                return object->is<TextInputBase>();
            case TextStyleAxisBase::axisValuePropertyKey:
                return object->is<TextStyleAxisBase>();
            case TextBase::widthPropertyKey:
                return object->is<TextBase>();
            case TextBase::heightPropertyKey:
                return object->is<TextBase>();
            case TextBase::originXPropertyKey:
                return object->is<TextBase>();
            case TextBase::originYPropertyKey:
                return object->is<TextBase>();
            case TextBase::paragraphSpacingPropertyKey:
                return object->is<TextBase>();
            case DrawableAssetBase::heightPropertyKey:
                return object->is<DrawableAssetBase>();
            case DrawableAssetBase::widthPropertyKey:
                return object->is<DrawableAssetBase>();
            case ExportAudioBase::volumePropertyKey:
                return object->is<ExportAudioBase>();
            case NestedTriggerBase::firePropertyKey:
                return object->is<NestedTriggerBase>();
            case EventBase::triggerPropertyKey:
                return object->is<EventBase>();
            case CustomPropertyTriggerBase::firePropertyKey:
                return object->is<CustomPropertyTriggerBase>();
        }
        return false;
    }
};
} // namespace rive

#endif