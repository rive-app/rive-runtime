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
#include "rive/animation/focus_action.hpp"
#include "rive/animation/focus_action_clear.hpp"
#include "rive/animation/focus_action_target.hpp"
#include "rive/animation/focus_action_traversal.hpp"
#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe.hpp"
#include "rive/animation/keyframe_bool.hpp"
#include "rive/animation/keyframe_callback.hpp"
#include "rive/animation/keyframe_color.hpp"
#include "rive/animation/keyframe_double.hpp"
#include "rive/animation/keyframe_id.hpp"
#include "rive/animation/keyframe_int.hpp"
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
#include "rive/animation/listener_types/listener_input_type.hpp"
#include "rive/animation/listener_types/listener_input_type_event.hpp"
#include "rive/animation/listener_types/listener_input_type_gamepad.hpp"
#include "rive/animation/listener_types/listener_input_type_keyboard.hpp"
#include "rive/animation/listener_types/listener_input_type_pointer_button.hpp"
#include "rive/animation/listener_types/listener_input_type_semantic.hpp"
#include "rive/animation/listener_types/listener_input_type_text.hpp"
#include "rive/animation/listener_types/listener_input_type_viewmodel.hpp"
#include "rive/animation/listener_viewmodel_change.hpp"
#include "rive/animation/nested_bool.hpp"
#include "rive/animation/nested_input.hpp"
#include "rive/animation/nested_linear_animation.hpp"
#include "rive/animation/nested_number.hpp"
#include "rive/animation/nested_remap_animation.hpp"
#include "rive/animation/nested_simple_animation.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/nested_trigger.hpp"
#include "rive/animation/scripted_listener_action.hpp"
#include "rive/animation/scripted_transition_condition.hpp"
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
#include "rive/animation/state_machine_listener_single.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/animation/state_transition.hpp"
#include "rive/animation/transition_artboard_condition.hpp"
#include "rive/animation/transition_bool_condition.hpp"
#include "rive/animation/transition_comparator.hpp"
#include "rive/animation/transition_condition.hpp"
#include "rive/animation/transition_focus_condition.hpp"
#include "rive/animation/transition_input_condition.hpp"
#include "rive/animation/transition_number_condition.hpp"
#include "rive/animation/transition_property_artboard_comparator.hpp"
#include "rive/animation/transition_property_comparator.hpp"
#include "rive/animation/transition_property_component_comparator.hpp"
#include "rive/animation/transition_property_viewmodel_comparator.hpp"
#include "rive/animation/transition_self_comparator.hpp"
#include "rive/animation/transition_trigger_condition.hpp"
#include "rive/animation/transition_value_artboard_comparator.hpp"
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
#include "rive/artboard_list_map_rule.hpp"
#include "rive/assets/asset.hpp"
#include "rive/assets/audio_asset.hpp"
#include "rive/assets/blob_asset.hpp"
#include "rive/assets/drawable_asset.hpp"
#include "rive/assets/export_audio.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/assets/manifest_asset.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/assets/script_module_asset.hpp"
#include "rive/assets/shader_asset.hpp"
#include "rive/assets/text_asset.hpp"
#include "rive/audio_event.hpp"
#include "rive/backboard.hpp"
#include "rive/bitmap_cache.hpp"
#include "rive/bones/bone.hpp"
#include "rive/bones/cubic_weight.hpp"
#include "rive/bones/root_bone.hpp"
#include "rive/bones/skeletal_component.hpp"
#include "rive/bones/skin.hpp"
#include "rive/bones/tendon.hpp"
#include "rive/bones/weight.hpp"
#include "rive/component.hpp"
#include "rive/component_origin.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/constraints/distance_constraint.hpp"
#include "rive/constraints/draggable_constraint.hpp"
#include "rive/constraints/follow_path_constraint.hpp"
#include "rive/constraints/ik_constraint.hpp"
#include "rive/constraints/list_follow_path_constraint.hpp"
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
#include "rive/data_bind/bindable_property_viewmodel.hpp"
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
#include "rive/data_bind/data_bind_path.hpp"
#include "rive/draw_rules.hpp"
#include "rive/draw_target.hpp"
#include "rive/drawable.hpp"
#include "rive/event.hpp"
#include "rive/focus_data.hpp"
#include "rive/foreground_layout_drawable.hpp"
#include "rive/generated/shapes/paint/color_channels_base.hpp"
#include "rive/inputs/gamepad_input.hpp"
#include "rive/inputs/keyboard_input.hpp"
#include "rive/inputs/semantic_input.hpp"
#include "rive/inputs/user_input.hpp"
#include "rive/joystick.hpp"
#include "rive/layer_mask.hpp"
#include "rive/layout/artboard_component_list_override.hpp"
#include "rive/layout/axis.hpp"
#include "rive/layout/axis_x.hpp"
#include "rive/layout/axis_y.hpp"
#include "rive/layout/grid_item_placement.hpp"
#include "rive/layout/grid_track.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_node_style.hpp"
#include "rive/layout/layout_participant.hpp"
#include "rive/layout/layout_sizing_style.hpp"
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
#include "rive/script_input_artboard.hpp"
#include "rive/script_input_boolean.hpp"
#include "rive/script_input_color.hpp"
#include "rive/script_input_number.hpp"
#include "rive/script_input_string.hpp"
#include "rive/script_input_trigger.hpp"
#include "rive/script_input_viewmodel_property.hpp"
#include "rive/scripted/scripted_data_converter.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/scripted/scripted_interpolator.hpp"
#include "rive/scripted/scripted_layout.hpp"
#include "rive/scripted/scripted_path_effect.hpp"
#include "rive/scripted/scripted_transition.hpp"
#include "rive/selection_style.hpp"
#include "rive/semantic/semantic_data.hpp"
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
#include "rive/shapes/paint/group_effect.hpp"
#include "rive/shapes/paint/linear_gradient.hpp"
#include "rive/shapes/paint/paint_image.hpp"
#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/target_effect.hpp"
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
#include "rive/text/text_style_background.hpp"
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
#include "rive/viewmodel/viewmodel_instance_asset_blob.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_font.hpp"
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
#include "rive/viewmodel/viewmodel_property_asset_blob.hpp"
#include "rive/viewmodel/viewmodel_property_asset_font.hpp"
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
            case ViewModelInstanceValueBase::typeKey:
                return new ViewModelInstanceValue();
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
            case ViewModelInstanceAssetBase::typeKey:
                return new ViewModelInstanceAsset();
            case ViewModelInstanceAssetBlobBase::typeKey:
                return new ViewModelInstanceAssetBlob();
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
            case ViewModelPropertyAssetFontBase::typeKey:
                return new ViewModelPropertyAssetFont();
            case ViewModelPropertyViewModelBase::typeKey:
                return new ViewModelPropertyViewModel();
            case ViewModelPropertyAssetBlobBase::typeKey:
                return new ViewModelPropertyAssetBlob();
            case ViewModelPropertyAssetImageBase::typeKey:
                return new ViewModelPropertyAssetImage();
            case DataEnumValueBase::typeKey:
                return new DataEnumValue();
            case ViewModelPropertyTriggerBase::typeKey:
                return new ViewModelPropertyTrigger();
            case ViewModelPropertyStringBase::typeKey:
                return new ViewModelPropertyString();
            case ViewModelPropertyColorBase::typeKey:
                return new ViewModelPropertyColor();
            case ViewModelPropertyBooleanBase::typeKey:
                return new ViewModelPropertyBoolean();
            case ViewModelInstanceBase::typeKey:
                return new ViewModelInstance();
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
            case ViewModelInstanceAssetFontBase::typeKey:
                return new ViewModelInstanceAssetFont();
            case ViewModelInstanceViewModelBase::typeKey:
                return new ViewModelInstanceViewModel();
            case ViewModelInstanceAssetImageBase::typeKey:
                return new ViewModelInstanceAssetImage();
            case CustomPropertyTriggerBase::typeKey:
                return new CustomPropertyTrigger();
            case ScriptInputTriggerBase::typeKey:
                return new ScriptInputTrigger();
            case DrawTargetBase::typeKey:
                return new DrawTarget();
            case LayerMaskBase::typeKey:
                return new LayerMask();
            case CustomPropertyNumberBase::typeKey:
                return new CustomPropertyNumber();
            case ScriptInputViewModelPropertyBase::typeKey:
                return new ScriptInputViewModelProperty();
            case DistanceConstraintBase::typeKey:
                return new DistanceConstraint();
            case FollowPathConstraintBase::typeKey:
                return new FollowPathConstraint();
            case ListFollowPathConstraintBase::typeKey:
                return new ListFollowPathConstraint();
            case IKConstraintBase::typeKey:
                return new IKConstraint();
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
            case ScriptedDrawableBase::typeKey:
                return new ScriptedDrawable();
            case ScriptedDataConverterBase::typeKey:
                return new ScriptedDataConverter();
            case ScriptedTransitionBase::typeKey:
                return new ScriptedTransition();
            case ScriptedInterpolatorBase::typeKey:
                return new ScriptedInterpolator();
            case ScriptedLayoutBase::typeKey:
                return new ScriptedLayout();
            case ScriptedPathEffectBase::typeKey:
                return new ScriptedPathEffect();
            case ScriptInputNumberBase::typeKey:
                return new ScriptInputNumber();
            case NestedArtboardLayoutBase::typeKey:
                return new NestedArtboardLayout();
            case NSlicerTileModeBase::typeKey:
                return new NSlicerTileMode();
            case GridTrackBase::typeKey:
                return new GridTrack();
            case GridItemPlacementBase::typeKey:
                return new GridItemPlacement();
            case LayoutNodeStyleBase::typeKey:
                return new LayoutNodeStyle();
            case LayoutParticipantBase::typeKey:
                return new LayoutParticipant();
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
            case ComponentOriginBase::typeKey:
                return new ComponentOrigin();
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
            case FocusActionClearBase::typeKey:
                return new FocusActionClear();
            case NestedTriggerBase::typeKey:
                return new NestedTrigger();
            case ScriptedListenerActionBase::typeKey:
                return new ScriptedListenerAction();
            case KeyedObjectBase::typeKey:
                return new KeyedObject();
            case AnimationBase::typeKey:
                return new Animation();
            case KeyFrameIntBase::typeKey:
                return new KeyFrameInt();
            case BlendAnimationDirectBase::typeKey:
                return new BlendAnimationDirect();
            case StateMachineNumberBase::typeKey:
                return new StateMachineNumber();
            case StateMachineListenerBase::typeKey:
                return new StateMachineListener();
            case StateMachineListenerSingleBase::typeKey:
                return new StateMachineListenerSingle();
            case CubicValueInterpolatorBase::typeKey:
                return new CubicValueInterpolator();
            case TransitionTriggerConditionBase::typeKey:
                return new TransitionTriggerCondition();
            case KeyedPropertyBase::typeKey:
                return new KeyedProperty();
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
            case ScriptedTransitionConditionBase::typeKey:
                return new ScriptedTransitionCondition();
            case TransitionViewModelConditionBase::typeKey:
                return new TransitionViewModelCondition();
            case TransitionFocusConditionBase::typeKey:
                return new TransitionFocusCondition();
            case TransitionNumberConditionBase::typeKey:
                return new TransitionNumberCondition();
            case TransitionValueBooleanComparatorBase::typeKey:
                return new TransitionValueBooleanComparator();
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
            case FocusActionTargetBase::typeKey:
                return new FocusActionTarget();
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
            case FocusActionTraversalBase::typeKey:
                return new FocusActionTraversal();
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
            case TransitionPropertyComponentComparatorBase::typeKey:
                return new TransitionPropertyComponentComparator();
            case NestedStateMachineBase::typeKey:
                return new NestedStateMachine();
            case ElasticInterpolatorBase::typeKey:
                return new ElasticInterpolator();
            case ListenerInputTypeBase::typeKey:
                return new ListenerInputType();
            case ListenerInputTypeEventBase::typeKey:
                return new ListenerInputTypeEvent();
            case ListenerInputTypeGamepadBase::typeKey:
                return new ListenerInputTypeGamepad();
            case ListenerInputTypePointerButtonBase::typeKey:
                return new ListenerInputTypePointerButton();
            case ListenerInputTypeKeyboardBase::typeKey:
                return new ListenerInputTypeKeyboard();
            case ListenerInputTypeTextBase::typeKey:
                return new ListenerInputTypeText();
            case ListenerInputTypeSemanticBase::typeKey:
                return new ListenerInputTypeSemantic();
            case ListenerInputTypeViewModelBase::typeKey:
                return new ListenerInputTypeViewModel();
            case ExitStateBase::typeKey:
                return new ExitState();
            case NestedNumberBase::typeKey:
                return new NestedNumber();
            case TransitionValueEnumComparatorBase::typeKey:
                return new TransitionValueEnumComparator();
            case KeyFrameCallbackBase::typeKey:
                return new KeyFrameCallback();
            case TransitionValueArtboardComparatorBase::typeKey:
                return new TransitionValueArtboardComparator();
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
            case GroupEffectBase::typeKey:
                return new GroupEffect();
            case TargetEffectBase::typeKey:
                return new TargetEffect();
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
            case PaintImageBase::typeKey:
                return new PaintImage();
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
            case FocusDataBase::typeKey:
                return new FocusData();
            case CustomPropertyBooleanBase::typeKey:
                return new CustomPropertyBoolean();
            case ScriptInputBooleanBase::typeKey:
                return new ScriptInputBoolean();
            case ScriptInputColorBase::typeKey:
                return new ScriptInputColor();
            case DrawRulesBase::typeKey:
                return new DrawRules();
            case LayoutComponentBase::typeKey:
                return new LayoutComponent();
            case ArtboardBase::typeKey:
                return new Artboard();
            case JoystickBase::typeKey:
                return new Joystick();
            case SelectionStyleBase::typeKey:
                return new SelectionStyle();
            case BackboardBase::typeKey:
                return new Backboard();
            case OpenUrlEventBase::typeKey:
                return new OpenUrlEvent();
            case SemanticDataBase::typeKey:
                return new SemanticData();
            case CustomPropertyStringBase::typeKey:
                return new CustomPropertyString();
            case ScriptInputStringBase::typeKey:
                return new ScriptInputString();
            case BindablePropertyArtboardBase::typeKey:
                return new BindablePropertyArtboard();
            case DataBindPathBase::typeKey:
                return new DataBindPath();
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
            case BindablePropertyViewModelBase::typeKey:
                return new BindablePropertyViewModel();
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
            case TextStyleBackgroundBase::typeKey:
                return new TextStyleBackground();
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
            case ArtboardListMapRuleBase::typeKey:
                return new ArtboardListMapRule();
            case CustomPropertyEnumBase::typeKey:
                return new CustomPropertyEnum();
            case BlobAssetBase::typeKey:
                return new BlobAsset();
            case ScriptAssetBase::typeKey:
                return new ScriptAsset();
            case ManifestAssetBase::typeKey:
                return new ManifestAsset();
            case ImageAssetBase::typeKey:
                return new ImageAsset();
            case ShaderAssetBase::typeKey:
                return new ShaderAsset();
            case FontAssetBase::typeKey:
                return new FontAsset();
            case AudioAssetBase::typeKey:
                return new AudioAsset();
            case FileAssetContentsBase::typeKey:
                return new FileAssetContents();
            case ScriptModuleAssetBase::typeKey:
                return new ScriptModuleAsset();
            case BitmapCacheBase::typeKey:
                return new BitmapCache();
            case AudioEventBase::typeKey:
                return new AudioEvent();
            case UserInputBase::typeKey:
                return new UserInput();
            case GamepadInputBase::typeKey:
                return new GamepadInput();
            case KeyboardInputBase::typeKey:
                return new KeyboardInput();
            case SemanticInputBase::typeKey:
                return new SemanticInput();
            case ScriptInputArtboardBase::typeKey:
                return new ScriptInputArtboard();
        }
        return nullptr;
    }
    static void setId(Core* object, int propertyKey, Id value)
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
            case ComponentBase::parentIdPropertyKey:
                object->as<ComponentBase>()->parentId(value);
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
            case ViewModelInstanceAssetBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceAssetBase>()->propertyValue(value);
                break;
            case ViewModelInstanceArtboardBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceArtboardBase>()->propertyValue(
                    value);
                break;
            case ViewModelPropertyViewModelBase::
                viewModelReferenceIdPropertyKey:
                object->as<ViewModelPropertyViewModelBase>()
                    ->viewModelReferenceId(value);
                break;
            case ViewModelInstanceBase::viewModelIdPropertyKey:
                object->as<ViewModelInstanceBase>()->viewModelId(value);
                break;
            case ViewModelInstanceListBase::listSourcePropertyKey:
                object->as<ViewModelInstanceListBase>()->listSource(value);
                break;
            case ViewModelInstanceViewModelBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceViewModelBase>()->propertyValue(
                    value);
                break;
            case DrawTargetBase::drawableIdPropertyKey:
                object->as<DrawTargetBase>()->drawableId(value);
                break;
            case LayerMaskBase::sourceIdPropertyKey:
                object->as<LayerMaskBase>()->sourceId(value);
                break;
            case TargetedConstraintBase::targetIdPropertyKey:
                object->as<TargetedConstraintBase>()->targetId(value);
                break;
            case ScrollPhysicsBase::constraintIdPropertyKey:
                object->as<ScrollPhysicsBase>()->constraintId(value);
                break;
            case ScrollConstraintBase::physicsIdPropertyKey:
                object->as<ScrollConstraintBase>()->physicsId(value);
                break;
            case ScrollBarConstraintBase::scrollConstraintIdPropertyKey:
                object->as<ScrollBarConstraintBase>()->scrollConstraintId(
                    value);
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
            case ScriptedDrawableBase::scriptAssetIdPropertyKey:
                object->as<ScriptedDrawableBase>()->scriptAssetId(value);
                break;
            case ScriptedDataConverterBase::scriptAssetIdPropertyKey:
                object->as<ScriptedDataConverterBase>()->scriptAssetId(value);
                break;
            case ScriptedTransitionBase::activeComponentIdPropertyKey:
                object->as<ScriptedTransitionBase>()->activeComponentId(value);
                break;
            case ScriptedTransitionBase::listSourcePropertyKey:
                object->as<ScriptedTransitionBase>()->listSource(value);
                break;
            case ScriptedInterpolatorBase::scriptAssetIdPropertyKey:
                object->as<ScriptedInterpolatorBase>()->scriptAssetId(value);
                break;
            case ScriptedPathEffectBase::scriptAssetIdPropertyKey:
                object->as<ScriptedPathEffectBase>()->scriptAssetId(value);
                break;
            case LayoutComponentStyleBase::interpolatorIdPropertyKey:
                object->as<LayoutComponentStyleBase>()->interpolatorId(value);
                break;
            case ArtboardComponentListOverrideBase::artboardIdPropertyKey:
                object->as<ArtboardComponentListOverrideBase>()->artboardId(
                    value);
                break;
            case ListenerFireEventBase::eventIdPropertyKey:
                object->as<ListenerFireEventBase>()->eventId(value);
                break;
            case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
                object->as<InterpolatingKeyFrameBase>()->interpolatorId(value);
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
            case ScriptedListenerActionBase::scriptAssetIdPropertyKey:
                object->as<ScriptedListenerActionBase>()->scriptAssetId(value);
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
            case StateMachineListenerBase::targetIdPropertyKey:
                object->as<StateMachineListenerBase>()->targetId(value);
                break;
            case StateMachineListenerSingleBase::eventIdPropertyKey:
                object->as<StateMachineListenerSingleBase>()->eventId(value);
                break;
            case TransitionInputConditionBase::inputIdPropertyKey:
                object->as<TransitionInputConditionBase>()->inputId(value);
                break;
            case KeyFrameIdBase::valuePropertyKey:
                object->as<KeyFrameIdBase>()->value(value);
                break;
            case ListenerAlignTargetBase::targetIdPropertyKey:
                object->as<ListenerAlignTargetBase>()->targetId(value);
                break;
            case ScriptedTransitionConditionBase::scriptAssetIdPropertyKey:
                object->as<ScriptedTransitionConditionBase>()->scriptAssetId(
                    value);
                break;
            case BlendState1DInputBase::inputIdPropertyKey:
                object->as<BlendState1DInputBase>()->inputId(value);
                break;
            case FocusActionTargetBase::targetIdPropertyKey:
                object->as<FocusActionTargetBase>()->targetId(value);
                break;
            case TransitionValueIdComparatorBase::valuePropertyKey:
                object->as<TransitionValueIdComparatorBase>()->value(value);
                break;
            case StateTransitionBase::stateToIdPropertyKey:
                object->as<StateTransitionBase>()->stateToId(value);
                break;
            case StateTransitionBase::interpolatorIdPropertyKey:
                object->as<StateTransitionBase>()->interpolatorId(value);
                break;
            case StateMachineFireEventBase::eventIdPropertyKey:
                object->as<StateMachineFireEventBase>()->eventId(value);
                break;
            case TransitionPropertyComponentComparatorBase::objectIdPropertyKey:
                object->as<TransitionPropertyComponentComparatorBase>()
                    ->objectId(value);
                break;
            case ListenerInputTypeEventBase::eventIdPropertyKey:
                object->as<ListenerInputTypeEventBase>()->eventId(value);
                break;
            case BlendStateTransitionBase::exitBlendAnimationIdPropertyKey:
                object->as<BlendStateTransitionBase>()->exitBlendAnimationId(
                    value);
                break;
            case TargetEffectBase::targetIdPropertyKey:
                object->as<TargetEffectBase>()->targetId(value);
                break;
            case PaintImageBase::imageAssetIdPropertyKey:
                object->as<PaintImageBase>()->imageAssetId(value);
                break;
            case ListPathBase::listSourcePropertyKey:
                object->as<ListPathBase>()->listSource(value);
                break;
            case ClippingShapeBase::sourceIdPropertyKey:
                object->as<ClippingShapeBase>()->sourceId(value);
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
            case JoystickBase::handleSourceIdPropertyKey:
                object->as<JoystickBase>()->handleSourceId(value);
                break;
            case BindablePropertyIdBase::propertyValuePropertyKey:
                object->as<BindablePropertyIdBase>()->propertyValue(value);
                break;
            case DataBindBase::converterIdPropertyKey:
                object->as<DataBindBase>()->converterId(value);
                break;
            case DataConverterNumberToListBase::viewModelIdPropertyKey:
                object->as<DataConverterNumberToListBase>()->viewModelId(value);
                break;
            case DataConverterRangeMapperBase::interpolatorIdPropertyKey:
                object->as<DataConverterRangeMapperBase>()->interpolatorId(
                    value);
                break;
            case DataConverterInterpolatorBase::interpolatorIdPropertyKey:
                object->as<DataConverterInterpolatorBase>()->interpolatorId(
                    value);
                break;
            case DataConverterGroupItemBase::converterIdPropertyKey:
                object->as<DataConverterGroupItemBase>()->converterId(value);
                break;
            case BindablePropertyListBase::propertyValuePropertyKey:
                object->as<BindablePropertyListBase>()->propertyValue(value);
                break;
            case BindablePropertyEnumBase::propertyValuePropertyKey:
                object->as<BindablePropertyEnumBase>()->propertyValue(value);
                break;
            case TendonBase::boneIdPropertyKey:
                object->as<TendonBase>()->boneId(value);
                break;
            case TextModifierRangeBase::runIdPropertyKey:
                object->as<TextModifierRangeBase>()->runId(value);
                break;
            case TextTargetModifierBase::targetIdPropertyKey:
                object->as<TextTargetModifierBase>()->targetId(value);
                break;
            case TextStyleBase::fontAssetIdPropertyKey:
                object->as<TextStyleBase>()->fontAssetId(value);
                break;
            case TextBase::textRunListSourcePropertyKey:
                object->as<TextBase>()->textRunListSource(value);
                break;
            case TextValueRunBase::styleIdPropertyKey:
                object->as<TextValueRunBase>()->styleId(value);
                break;
            case ArtboardListMapRuleBase::artboardIdPropertyKey:
                object->as<ArtboardListMapRuleBase>()->artboardId(value);
                break;
            case ArtboardListMapRuleBase::viewModelIdPropertyKey:
                object->as<ArtboardListMapRuleBase>()->viewModelId(value);
                break;
            case CustomPropertyEnumBase::propertyValuePropertyKey:
                object->as<CustomPropertyEnumBase>()->propertyValue(value);
                break;
            case CustomPropertyEnumBase::enumIdPropertyKey:
                object->as<CustomPropertyEnumBase>()->enumId(value);
                break;
            case AudioEventBase::assetIdPropertyKey:
                object->as<AudioEventBase>()->assetId(value);
                break;
            case ScriptInputArtboardBase::artboardIdPropertyKey:
                object->as<ScriptInputArtboardBase>()->artboardId(value);
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
            case ComponentBase::namePropertyKey:
                object->as<ComponentBase>()->name(value);
                break;
            case DataEnumCustomBase::namePropertyKey:
                object->as<DataEnumCustomBase>()->name(value);
                break;
            case ViewModelInstanceStringBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceStringBase>()->propertyValue(value);
                break;
            case DataEnumValueBase::keyPropertyKey:
                object->as<DataEnumValueBase>()->key(value);
                break;
            case DataEnumValueBase::valuePropertyKey:
                object->as<DataEnumValueBase>()->value(value);
                break;
            case AssetBase::namePropertyKey:
                object->as<AssetBase>()->name(value);
                break;
            case DataConverterBase::namePropertyKey:
                object->as<DataConverterBase>()->name(value);
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
            case SemanticDataBase::labelPropertyKey:
                object->as<SemanticDataBase>()->label(value);
                break;
            case SemanticDataBase::valuePropertyKey:
                object->as<SemanticDataBase>()->value(value);
                break;
            case SemanticDataBase::hintPropertyKey:
                object->as<SemanticDataBase>()->hint(value);
                break;
            case CustomPropertyStringBase::propertyValuePropertyKey:
                object->as<CustomPropertyStringBase>()->propertyValue(value);
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
            case FileAssetBase::cdnBaseUrlPropertyKey:
                object->as<FileAssetBase>()->cdnBaseUrl(value);
                break;
            case TextAssetBase::folderPathPropertyKey:
                object->as<TextAssetBase>()->folderPath(value);
                break;
        }
    }
    static void setUint(Core* object, int propertyKey, uint32_t value)
    {
        switch (propertyKey)
        {
            case ViewModelPropertyBase::symbolTypeValuePropertyKey:
                object->as<ViewModelPropertyBase>()->symbolTypeValue(value);
                break;
            case ViewModelPropertyBase::componentPropsPropertyKey:
                object->as<ViewModelPropertyBase>()->componentProps(value);
                break;
            case ViewModelPropertyEnumSystemBase::enumTypePropertyKey:
                object->as<ViewModelPropertyEnumSystemBase>()->enumType(value);
                break;
            case ViewModelBase::viewModelTypePropertyKey:
                object->as<ViewModelBase>()->viewModelType(value);
                break;
            case DataEnumSystemBase::enumTypePropertyKey:
                object->as<DataEnumSystemBase>()->enumType(value);
                break;
            case ViewModelInstanceTriggerBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceTriggerBase>()->propertyValue(
                    value);
                break;
            case ViewModelInstanceSymbolListIndexBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceSymbolListIndexBase>()
                    ->propertyValue(value);
                break;
            case CustomPropertyBase::nameIdPropertyKey:
                object->as<CustomPropertyBase>()->nameId(value);
                break;
            case CustomPropertyTriggerBase::propertyValuePropertyKey:
                object->as<CustomPropertyTriggerBase>()->propertyValue(value);
                break;
            case DrawTargetBase::placementValuePropertyKey:
                object->as<DrawTargetBase>()->placementValue(value);
                break;
            case LayerMaskBase::maskFlagsPropertyKey:
                object->as<LayerMaskBase>()->maskFlags(value);
                break;
            case LayerMaskBase::maskModeValuePropertyKey:
                object->as<LayerMaskBase>()->maskModeValue(value);
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
            case DraggableConstraintBase::directionValuePropertyKey:
                object->as<DraggableConstraintBase>()->directionValue(value);
                break;
            case ScrollConstraintBase::physicsTypeValuePropertyKey:
                object->as<ScrollConstraintBase>()->physicsTypeValue(value);
                break;
            case ScrollConstraintBase::virtualizeBufferPropertyKey:
                object->as<ScrollConstraintBase>()->virtualizeBuffer(value);
                break;
            case ScrollConstraintBase::scrollFlagsPropertyKey:
                object->as<ScrollConstraintBase>()->scrollFlags(value);
                break;
            case DrawableBase::blendModeValuePropertyKey:
                object->as<DrawableBase>()->blendModeValue(value);
                break;
            case DrawableBase::additiveAmountPropertyKey:
                object->as<DrawableBase>()->additiveAmount(value);
                break;
            case DrawableBase::drawableFlagsPropertyKey:
                object->as<DrawableBase>()->drawableFlags(value);
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
            case GridTrackBase::collectionPropertyKey:
                object->as<GridTrackBase>()->collection(value);
                break;
            case GridTrackBase::trackTypePropertyKey:
                object->as<GridTrackBase>()->trackType(value);
                break;
            case GridTrackBase::trackMaxTypePropertyKey:
                object->as<GridTrackBase>()->trackMaxType(value);
                break;
            case GridItemPlacementBase::gridColumnSpanPropertyKey:
                object->as<GridItemPlacementBase>()->gridColumnSpan(value);
                break;
            case GridItemPlacementBase::gridRowSpanPropertyKey:
                object->as<GridItemPlacementBase>()->gridRowSpan(value);
                break;
            case LayoutSizingStyleBase::minWidthUnitsValuePropertyKey:
                object->as<LayoutSizingStyleBase>()->minWidthUnitsValue(value);
                break;
            case LayoutSizingStyleBase::maxWidthUnitsValuePropertyKey:
                object->as<LayoutSizingStyleBase>()->maxWidthUnitsValue(value);
                break;
            case LayoutSizingStyleBase::minHeightUnitsValuePropertyKey:
                object->as<LayoutSizingStyleBase>()->minHeightUnitsValue(value);
                break;
            case LayoutSizingStyleBase::maxHeightUnitsValuePropertyKey:
                object->as<LayoutSizingStyleBase>()->maxHeightUnitsValue(value);
                break;
            case LayoutSizingStyleBase::layoutWidthScaleTypePropertyKey:
                object->as<LayoutSizingStyleBase>()->layoutWidthScaleType(
                    value);
                break;
            case LayoutSizingStyleBase::layoutHeightScaleTypePropertyKey:
                object->as<LayoutSizingStyleBase>()->layoutHeightScaleType(
                    value);
                break;
            case LayoutSizingStyleBase::widthUnitsValuePropertyKey:
                object->as<LayoutSizingStyleBase>()->widthUnitsValue(value);
                break;
            case LayoutSizingStyleBase::heightUnitsValuePropertyKey:
                object->as<LayoutSizingStyleBase>()->heightUnitsValue(value);
                break;
            case LayoutSizingStyleBase::justifySelfValuePropertyKey:
                object->as<LayoutSizingStyleBase>()->justifySelfValue(value);
                break;
            case LayoutSizingStyleBase::displayValuePropertyKey:
                object->as<LayoutSizingStyleBase>()->displayValue(value);
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
            case LayoutComponentStyleBase::flexBasisUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->flexBasisUnitsValue(
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
            case LayoutComponentStyleBase::flexWrapValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->flexWrapValue(value);
                break;
            case LayoutComponentStyleBase::overflowValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->overflowValue(value);
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
            case LayoutComponentStyleBase::gapHorizontalUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->gapHorizontalUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::gapVerticalUnitsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->gapVerticalUnitsValue(
                    value);
                break;
            case LayoutComponentStyleBase::justifyItemsValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->justifyItemsValue(
                    value);
                break;
            case LayoutComponentStyleBase::layoutTypeValuePropertyKey:
                object->as<LayoutComponentStyleBase>()->layoutTypeValue(value);
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
            case ListenerActionBase::flagsPropertyKey:
                object->as<ListenerActionBase>()->flags(value);
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
            case KeyFrameUintBase::valuePropertyKey:
                object->as<KeyFrameUintBase>()->value(value);
                break;
            case BlendAnimationDirectBase::blendSourcePropertyKey:
                object->as<BlendAnimationDirectBase>()->blendSource(value);
                break;
            case StateMachineListenerSingleBase::listenerTypeValuePropertyKey:
                object->as<StateMachineListenerSingleBase>()->listenerTypeValue(
                    value);
                break;
            case KeyedPropertyBase::propertyKeyPropertyKey:
                object->as<KeyedPropertyBase>()->propertyKey(value);
                break;
            case TransitionPropertyArtboardComparatorBase::
                propertyTypePropertyKey:
                object->as<TransitionPropertyArtboardComparatorBase>()
                    ->propertyType(value);
                break;
            case ListenerBoolChangeBase::valuePropertyKey:
                object->as<ListenerBoolChangeBase>()->value(value);
                break;
            case TransitionViewModelConditionBase::opValuePropertyKey:
                object->as<TransitionViewModelConditionBase>()->opValue(value);
                break;
            case TransitionValueConditionBase::opValuePropertyKey:
                object->as<TransitionValueConditionBase>()->opValue(value);
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
            case StateTransitionBase::randomWeightPropertyKey:
                object->as<StateTransitionBase>()->randomWeight(value);
                break;
            case FocusActionTraversalBase::traversalKindPropertyKey:
                object->as<FocusActionTraversalBase>()->traversalKind(value);
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
            case ListenerViewModelChangeBase::inputValuePropertyKey:
                object->as<ListenerViewModelChangeBase>()->inputValue(value);
                break;
            case ListenerViewModelChangeBase::inputValueIndexPropertyKey:
                object->as<ListenerViewModelChangeBase>()->inputValueIndex(
                    value);
                break;
            case TransitionPropertyComponentComparatorBase::
                propertyKeyPropertyKey:
                object->as<TransitionPropertyComponentComparatorBase>()
                    ->propertyKey(value);
                break;
            case ElasticInterpolatorBase::easingValuePropertyKey:
                object->as<ElasticInterpolatorBase>()->easingValue(value);
                break;
            case ListenerInputTypeBase::listenerTypeValuePropertyKey:
                object->as<ListenerInputTypeBase>()->listenerTypeValue(value);
                break;
            case ListenerInputTypePointerButtonBase::
                pointerButtonValuePropertyKey:
                object->as<ListenerInputTypePointerButtonBase>()
                    ->pointerButtonValue(value);
                break;
            case ShapePaintBase::blendModeValuePropertyKey:
                object->as<ShapePaintBase>()->blendModeValue(value);
                break;
            case ShapePaintBase::additiveAmountPropertyKey:
                object->as<ShapePaintBase>()->additiveAmount(value);
                break;
            case ColorChannelsBase::colorRedPropertyKey:
            {
                if (auto* _c = ColorChannelsBase::from(object))
                {
                    _c->colorRed(value);
                }
                break;
            }
            case ColorChannelsBase::colorGreenPropertyKey:
            {
                if (auto* _c = ColorChannelsBase::from(object))
                {
                    _c->colorGreen(value);
                }
                break;
            }
            case ColorChannelsBase::colorBluePropertyKey:
            {
                if (auto* _c = ColorChannelsBase::from(object))
                {
                    _c->colorBlue(value);
                }
                break;
            }
            case ColorChannelsBase::colorAlphaPropertyKey:
            {
                if (auto* _c = ColorChannelsBase::from(object))
                {
                    _c->colorAlpha(value);
                }
                break;
            }
            case StrokeBase::capPropertyKey:
                object->as<StrokeBase>()->cap(value);
                break;
            case StrokeBase::joinPropertyKey:
                object->as<StrokeBase>()->join(value);
                break;
            case StrokeBase::positionPropertyKey:
                object->as<StrokeBase>()->position(value);
                break;
            case PaintImageBase::imageSamplerFilterPropertyKey:
                object->as<PaintImageBase>()->imageSamplerFilter(value);
                break;
            case PaintImageBase::imageSamplerWrapXPropertyKey:
                object->as<PaintImageBase>()->imageSamplerWrapX(value);
                break;
            case PaintImageBase::imageSamplerWrapYPropertyKey:
                object->as<PaintImageBase>()->imageSamplerWrapY(value);
                break;
            case PaintImageBase::imageSizeModePropertyKey:
                object->as<PaintImageBase>()->imageSizeMode(value);
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
            case ClippingShapeBase::fillRulePropertyKey:
                object->as<ClippingShapeBase>()->fillRule(value);
                break;
            case PolygonBase::pointsPropertyKey:
                object->as<PolygonBase>()->points(value);
                break;
            case ImageBase::fitPropertyKey:
                object->as<ImageBase>()->fit(value);
                break;
            case ImageBase::samplerFilterPropertyKey:
                object->as<ImageBase>()->samplerFilter(value);
                break;
            case ImageBase::samplerWrapXPropertyKey:
                object->as<ImageBase>()->samplerWrapX(value);
                break;
            case ImageBase::samplerWrapYPropertyKey:
                object->as<ImageBase>()->samplerWrapY(value);
                break;
            case FocusDataBase::focusFlagsPropertyKey:
                object->as<FocusDataBase>()->focusFlags(value);
                break;
            case FocusDataBase::edgeBehaviorValuePropertyKey:
                object->as<FocusDataBase>()->edgeBehaviorValue(value);
                break;
            case JoystickBase::joystickFlagsPropertyKey:
                object->as<JoystickBase>()->joystickFlags(value);
                break;
            case OpenUrlEventBase::targetValuePropertyKey:
                object->as<OpenUrlEventBase>()->targetValue(value);
                break;
            case SemanticDataBase::rolePropertyKey:
                object->as<SemanticDataBase>()->role(value);
                break;
            case SemanticDataBase::headingLevelPropertyKey:
                object->as<SemanticDataBase>()->headingLevel(value);
                break;
            case SemanticDataBase::traitFlagsPropertyKey:
                object->as<SemanticDataBase>()->traitFlags(value);
                break;
            case SemanticDataBase::stateFlagsPropertyKey:
                object->as<SemanticDataBase>()->stateFlags(value);
                break;
            case SemanticDataBase::isCheckedPropertyKey:
                object->as<SemanticDataBase>()->isChecked(value);
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
            case DataConverterFormulaBase::randomModeValuePropertyKey:
                object->as<DataConverterFormulaBase>()->randomModeValue(value);
                break;
            case DataConverterOperationBase::operationTypePropertyKey:
                object->as<DataConverterOperationBase>()->operationType(value);
                break;
            case DataConverterRangeMapperBase::interpolationTypePropertyKey:
                object->as<DataConverterRangeMapperBase>()->interpolationType(
                    value);
                break;
            case DataConverterRangeMapperBase::flagsPropertyKey:
                object->as<DataConverterRangeMapperBase>()->flags(value);
                break;
            case DataConverterInterpolatorBase::interpolationTypePropertyKey:
                object->as<DataConverterInterpolatorBase>()->interpolationType(
                    value);
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
            case NestedArtboardLeafBase::fitPropertyKey:
                object->as<NestedArtboardLeafBase>()->fit(value);
                break;
            case WeightBase::valuesPropertyKey:
                object->as<WeightBase>()->values(value);
                break;
            case WeightBase::indicesPropertyKey:
                object->as<WeightBase>()->indices(value);
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
            case TextInputBase::alignValuePropertyKey:
                object->as<TextInputBase>()->alignValue(value);
                break;
            case TextInputBase::verticalAlignValuePropertyKey:
                object->as<TextInputBase>()->verticalAlignValue(value);
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
            case TextBase::wordBreakValuePropertyKey:
                object->as<TextBase>()->wordBreakValue(value);
                break;
            case TextBase::verticalAlignValuePropertyKey:
                object->as<TextBase>()->verticalAlignValue(value);
                break;
            case TextBase::verticalTrimValuePropertyKey:
                object->as<TextBase>()->verticalTrimValue(value);
                break;
            case TextBase::verticalTrimTopValuePropertyKey:
                object->as<TextBase>()->verticalTrimTopValue(value);
                break;
            case TextBase::verticalTrimBottomValuePropertyKey:
                object->as<TextBase>()->verticalTrimBottomValue(value);
                break;
            case FileAssetBase::assetIdPropertyKey:
                object->as<FileAssetBase>()->assetId(value);
                break;
            case ScriptAssetBase::generatorFunctionRefPropertyKey:
                object->as<ScriptAssetBase>()->generatorFunctionRef(value);
                break;
            case ScriptAssetBase::serializedImplementedMethodsPropertyKey:
                object->as<ScriptAssetBase>()->serializedImplementedMethods(
                    value);
                break;
            case ImageAssetBase::samplerFilterPropertyKey:
                object->as<ImageAssetBase>()->samplerFilter(value);
                break;
            case ImageAssetBase::samplerWrapXPropertyKey:
                object->as<ImageAssetBase>()->samplerWrapX(value);
                break;
            case ImageAssetBase::samplerWrapYPropertyKey:
                object->as<ImageAssetBase>()->samplerWrapY(value);
                break;
            case ScriptModuleAssetBase::languagePropertyKey:
                object->as<ScriptModuleAssetBase>()->language(value);
                break;
            case BitmapCacheBase::cacheFlagsPropertyKey:
                object->as<BitmapCacheBase>()->cacheFlags(value);
                break;
            case GamepadInputBase::kindPropertyKey:
                object->as<GamepadInputBase>()->kind(value);
                break;
            case GamepadInputBase::mappingPropertyKey:
                object->as<GamepadInputBase>()->mapping(value);
                break;
            case GamepadInputBase::inputIndexPropertyKey:
                object->as<GamepadInputBase>()->inputIndex(value);
                break;
            case GamepadInputBase::buttonPhasePropertyKey:
                object->as<GamepadInputBase>()->buttonPhase(value);
                break;
            case KeyboardInputBase::keyTypePropertyKey:
                object->as<KeyboardInputBase>()->keyType(value);
                break;
            case KeyboardInputBase::keyPhasePropertyKey:
                object->as<KeyboardInputBase>()->keyPhase(value);
                break;
            case KeyboardInputBase::modifiersPropertyKey:
                object->as<KeyboardInputBase>()->modifiers(value);
                break;
            case SemanticInputBase::actionTypePropertyKey:
                object->as<SemanticInputBase>()->actionType(value);
                break;
            case ViewModelInstanceListItemBase::viewModelIdPropertyKey:
                object->as<ViewModelInstanceListItemBase>()->viewModelId(value);
                break;
            case ViewModelInstanceListItemBase::viewModelInstanceIdPropertyKey:
                object->as<ViewModelInstanceListItemBase>()
                    ->viewModelInstanceId(value);
                break;
            case ComponentBase::parentIdPropertyKey:
                object->as<ComponentBase>()->parentId(value);
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
            case ViewModelInstanceAssetBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceAssetBase>()->propertyValue(value);
                break;
            case ViewModelInstanceArtboardBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceArtboardBase>()->propertyValue(
                    value);
                break;
            case ViewModelPropertyViewModelBase::
                viewModelReferenceIdPropertyKey:
                object->as<ViewModelPropertyViewModelBase>()
                    ->viewModelReferenceId(value);
                break;
            case ViewModelInstanceBase::viewModelIdPropertyKey:
                object->as<ViewModelInstanceBase>()->viewModelId(value);
                break;
            case ViewModelInstanceListBase::listSourcePropertyKey:
                object->as<ViewModelInstanceListBase>()->listSource(value);
                break;
            case ViewModelInstanceViewModelBase::propertyValuePropertyKey:
                object->as<ViewModelInstanceViewModelBase>()->propertyValue(
                    value);
                break;
            case DrawTargetBase::drawableIdPropertyKey:
                object->as<DrawTargetBase>()->drawableId(value);
                break;
            case LayerMaskBase::sourceIdPropertyKey:
                object->as<LayerMaskBase>()->sourceId(value);
                break;
            case TargetedConstraintBase::targetIdPropertyKey:
                object->as<TargetedConstraintBase>()->targetId(value);
                break;
            case ScrollPhysicsBase::constraintIdPropertyKey:
                object->as<ScrollPhysicsBase>()->constraintId(value);
                break;
            case ScrollConstraintBase::physicsIdPropertyKey:
                object->as<ScrollConstraintBase>()->physicsId(value);
                break;
            case ScrollBarConstraintBase::scrollConstraintIdPropertyKey:
                object->as<ScrollBarConstraintBase>()->scrollConstraintId(
                    value);
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
            case ScriptedDrawableBase::scriptAssetIdPropertyKey:
                object->as<ScriptedDrawableBase>()->scriptAssetId(value);
                break;
            case ScriptedDataConverterBase::scriptAssetIdPropertyKey:
                object->as<ScriptedDataConverterBase>()->scriptAssetId(value);
                break;
            case ScriptedTransitionBase::activeComponentIdPropertyKey:
                object->as<ScriptedTransitionBase>()->activeComponentId(value);
                break;
            case ScriptedTransitionBase::listSourcePropertyKey:
                object->as<ScriptedTransitionBase>()->listSource(value);
                break;
            case ScriptedInterpolatorBase::scriptAssetIdPropertyKey:
                object->as<ScriptedInterpolatorBase>()->scriptAssetId(value);
                break;
            case ScriptedPathEffectBase::scriptAssetIdPropertyKey:
                object->as<ScriptedPathEffectBase>()->scriptAssetId(value);
                break;
            case LayoutComponentStyleBase::interpolatorIdPropertyKey:
                object->as<LayoutComponentStyleBase>()->interpolatorId(value);
                break;
            case ArtboardComponentListOverrideBase::artboardIdPropertyKey:
                object->as<ArtboardComponentListOverrideBase>()->artboardId(
                    value);
                break;
            case ListenerFireEventBase::eventIdPropertyKey:
                object->as<ListenerFireEventBase>()->eventId(value);
                break;
            case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
                object->as<InterpolatingKeyFrameBase>()->interpolatorId(value);
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
            case ScriptedListenerActionBase::scriptAssetIdPropertyKey:
                object->as<ScriptedListenerActionBase>()->scriptAssetId(value);
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
            case StateMachineListenerBase::targetIdPropertyKey:
                object->as<StateMachineListenerBase>()->targetId(value);
                break;
            case StateMachineListenerSingleBase::eventIdPropertyKey:
                object->as<StateMachineListenerSingleBase>()->eventId(value);
                break;
            case TransitionInputConditionBase::inputIdPropertyKey:
                object->as<TransitionInputConditionBase>()->inputId(value);
                break;
            case KeyFrameIdBase::valuePropertyKey:
                object->as<KeyFrameIdBase>()->value(value);
                break;
            case ListenerAlignTargetBase::targetIdPropertyKey:
                object->as<ListenerAlignTargetBase>()->targetId(value);
                break;
            case ScriptedTransitionConditionBase::scriptAssetIdPropertyKey:
                object->as<ScriptedTransitionConditionBase>()->scriptAssetId(
                    value);
                break;
            case BlendState1DInputBase::inputIdPropertyKey:
                object->as<BlendState1DInputBase>()->inputId(value);
                break;
            case FocusActionTargetBase::targetIdPropertyKey:
                object->as<FocusActionTargetBase>()->targetId(value);
                break;
            case TransitionValueIdComparatorBase::valuePropertyKey:
                object->as<TransitionValueIdComparatorBase>()->value(value);
                break;
            case StateTransitionBase::stateToIdPropertyKey:
                object->as<StateTransitionBase>()->stateToId(value);
                break;
            case StateTransitionBase::interpolatorIdPropertyKey:
                object->as<StateTransitionBase>()->interpolatorId(value);
                break;
            case StateMachineFireEventBase::eventIdPropertyKey:
                object->as<StateMachineFireEventBase>()->eventId(value);
                break;
            case TransitionPropertyComponentComparatorBase::objectIdPropertyKey:
                object->as<TransitionPropertyComponentComparatorBase>()
                    ->objectId(value);
                break;
            case ListenerInputTypeEventBase::eventIdPropertyKey:
                object->as<ListenerInputTypeEventBase>()->eventId(value);
                break;
            case BlendStateTransitionBase::exitBlendAnimationIdPropertyKey:
                object->as<BlendStateTransitionBase>()->exitBlendAnimationId(
                    value);
                break;
            case TargetEffectBase::targetIdPropertyKey:
                object->as<TargetEffectBase>()->targetId(value);
                break;
            case PaintImageBase::imageAssetIdPropertyKey:
                object->as<PaintImageBase>()->imageAssetId(value);
                break;
            case ListPathBase::listSourcePropertyKey:
                object->as<ListPathBase>()->listSource(value);
                break;
            case ClippingShapeBase::sourceIdPropertyKey:
                object->as<ClippingShapeBase>()->sourceId(value);
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
            case JoystickBase::handleSourceIdPropertyKey:
                object->as<JoystickBase>()->handleSourceId(value);
                break;
            case BindablePropertyIdBase::propertyValuePropertyKey:
                object->as<BindablePropertyIdBase>()->propertyValue(value);
                break;
            case DataBindBase::converterIdPropertyKey:
                object->as<DataBindBase>()->converterId(value);
                break;
            case DataConverterNumberToListBase::viewModelIdPropertyKey:
                object->as<DataConverterNumberToListBase>()->viewModelId(value);
                break;
            case DataConverterRangeMapperBase::interpolatorIdPropertyKey:
                object->as<DataConverterRangeMapperBase>()->interpolatorId(
                    value);
                break;
            case DataConverterInterpolatorBase::interpolatorIdPropertyKey:
                object->as<DataConverterInterpolatorBase>()->interpolatorId(
                    value);
                break;
            case DataConverterGroupItemBase::converterIdPropertyKey:
                object->as<DataConverterGroupItemBase>()->converterId(value);
                break;
            case BindablePropertyListBase::propertyValuePropertyKey:
                object->as<BindablePropertyListBase>()->propertyValue(value);
                break;
            case BindablePropertyEnumBase::propertyValuePropertyKey:
                object->as<BindablePropertyEnumBase>()->propertyValue(value);
                break;
            case TendonBase::boneIdPropertyKey:
                object->as<TendonBase>()->boneId(value);
                break;
            case TextModifierRangeBase::runIdPropertyKey:
                object->as<TextModifierRangeBase>()->runId(value);
                break;
            case TextTargetModifierBase::targetIdPropertyKey:
                object->as<TextTargetModifierBase>()->targetId(value);
                break;
            case TextStyleBase::fontAssetIdPropertyKey:
                object->as<TextStyleBase>()->fontAssetId(value);
                break;
            case TextBase::textRunListSourcePropertyKey:
                object->as<TextBase>()->textRunListSource(value);
                break;
            case TextValueRunBase::styleIdPropertyKey:
                object->as<TextValueRunBase>()->styleId(value);
                break;
            case ArtboardListMapRuleBase::artboardIdPropertyKey:
                object->as<ArtboardListMapRuleBase>()->artboardId(value);
                break;
            case ArtboardListMapRuleBase::viewModelIdPropertyKey:
                object->as<ArtboardListMapRuleBase>()->viewModelId(value);
                break;
            case CustomPropertyEnumBase::propertyValuePropertyKey:
                object->as<CustomPropertyEnumBase>()->propertyValue(value);
                break;
            case CustomPropertyEnumBase::enumIdPropertyKey:
                object->as<CustomPropertyEnumBase>()->enumId(value);
                break;
            case AudioEventBase::assetIdPropertyKey:
                object->as<AudioEventBase>()->assetId(value);
                break;
            case ScriptInputArtboardBase::artboardIdPropertyKey:
                object->as<ScriptInputArtboardBase>()->artboardId(value);
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
            case SelectionStyleBase::highlightColorPropertyKey:
                object->as<SelectionStyleBase>()->highlightColor(value);
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
            case LayerMaskBase::isVisiblePropertyKey:
                object->as<LayerMaskBase>()->isVisible(value);
                break;
            case LayerMaskBase::sourceDrawsPropertyKey:
                object->as<LayerMaskBase>()->sourceDraws(value);
                break;
            case LayerMaskBase::useCustomBoundsPropertyKey:
                object->as<LayerMaskBase>()->useCustomBounds(value);
                break;
            case FollowPathConstraintBase::orientPropertyKey:
                object->as<FollowPathConstraintBase>()->orient(value);
                break;
            case FollowPathConstraintBase::offsetPropertyKey:
                object->as<FollowPathConstraintBase>()->offset(value);
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
            case ScrollConstraintBase::snapPropertyKey:
                object->as<ScrollConstraintBase>()->snap(value);
                break;
            case ScrollConstraintBase::virtualizePropertyKey:
                object->as<ScrollConstraintBase>()->virtualize(value);
                break;
            case ScrollConstraintBase::infinitePropertyKey:
                object->as<ScrollConstraintBase>()->infinite(value);
                break;
            case ScrollConstraintBase::interactivePropertyKey:
                object->as<ScrollConstraintBase>()->interactive(value);
                break;
            case ScrollConstraintBase::scrollActivePropertyKey:
                object->as<ScrollConstraintBase>()->scrollActive(value);
                break;
            case ScrollConstraintBase::wheelInteractivePropertyKey:
                object->as<ScrollConstraintBase>()->wheelInteractive(value);
                break;
            case ScrollBarConstraintBase::autoSizePropertyKey:
                object->as<ScrollBarConstraintBase>()->autoSize(value);
                break;
            case NestedArtboardBase::isPausedPropertyKey:
                object->as<NestedArtboardBase>()->isPaused(value);
                break;
            case NestedArtboardBase::isStatefulPropertyKey:
                object->as<NestedArtboardBase>()->isStateful(value);
                break;
            case LayoutSizingStyleBase::hugUnboundedPropertyKey:
                object->as<LayoutSizingStyleBase>()->hugUnbounded(value);
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
            case FocusDataBase::canFocusPropertyKey:
                object->as<FocusDataBase>()->canFocus(value);
                break;
            case FocusDataBase::canTouchPropertyKey:
                object->as<FocusDataBase>()->canTouch(value);
                break;
            case FocusDataBase::canTraversePropertyKey:
                object->as<FocusDataBase>()->canTraverse(value);
                break;
            case CustomPropertyBooleanBase::propertyValuePropertyKey:
                object->as<CustomPropertyBooleanBase>()->propertyValue(value);
                break;
            case LayoutComponentBase::clipPropertyKey:
                object->as<LayoutComponentBase>()->clip(value);
                break;
            case SemanticDataBase::isExpandablePropertyKey:
                object->as<SemanticDataBase>()->isExpandable(value);
                break;
            case SemanticDataBase::isSelectablePropertyKey:
                object->as<SemanticDataBase>()->isSelectable(value);
                break;
            case SemanticDataBase::isCheckablePropertyKey:
                object->as<SemanticDataBase>()->isCheckable(value);
                break;
            case SemanticDataBase::isToggleablePropertyKey:
                object->as<SemanticDataBase>()->isToggleable(value);
                break;
            case SemanticDataBase::isRequirablePropertyKey:
                object->as<SemanticDataBase>()->isRequirable(value);
                break;
            case SemanticDataBase::isEnablablePropertyKey:
                object->as<SemanticDataBase>()->isEnablable(value);
                break;
            case SemanticDataBase::isFocusablePropertyKey:
                object->as<SemanticDataBase>()->isFocusable(value);
                break;
            case SemanticDataBase::isExpandedPropertyKey:
                object->as<SemanticDataBase>()->isExpanded(value);
                break;
            case SemanticDataBase::isSelectedPropertyKey:
                object->as<SemanticDataBase>()->isSelected(value);
                break;
            case SemanticDataBase::isToggledPropertyKey:
                object->as<SemanticDataBase>()->isToggled(value);
                break;
            case SemanticDataBase::isRequiredPropertyKey:
                object->as<SemanticDataBase>()->isRequired(value);
                break;
            case SemanticDataBase::isDisabledPropertyKey:
                object->as<SemanticDataBase>()->isDisabled(value);
                break;
            case SemanticDataBase::isFocusedPropertyKey:
                object->as<SemanticDataBase>()->isFocused(value);
                break;
            case SemanticDataBase::isHiddenPropertyKey:
                object->as<SemanticDataBase>()->isHidden(value);
                break;
            case SemanticDataBase::isLiveRegionPropertyKey:
                object->as<SemanticDataBase>()->isLiveRegion(value);
                break;
            case SemanticDataBase::isReadOnlyPropertyKey:
                object->as<SemanticDataBase>()->isReadOnly(value);
                break;
            case SemanticDataBase::isModalPropertyKey:
                object->as<SemanticDataBase>()->isModal(value);
                break;
            case SemanticDataBase::isObscuredPropertyKey:
                object->as<SemanticDataBase>()->isObscured(value);
                break;
            case SemanticDataBase::isMultilinePropertyKey:
                object->as<SemanticDataBase>()->isMultiline(value);
                break;
            case DataBindPathBase::isRelativePropertyKey:
                object->as<DataBindPathBase>()->isRelative(value);
                break;
            case BindablePropertyBooleanBase::propertyValuePropertyKey:
                object->as<BindablePropertyBooleanBase>()->propertyValue(value);
                break;
            case NestedArtboardLeafBase::fitToLayoutParentPropertyKey:
                object->as<NestedArtboardLeafBase>()->fitToLayoutParent(value);
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
            case TextInputBase::multilinePropertyKey:
                object->as<TextInputBase>()->multiline(value);
                break;
            case TextInputBase::obscuredPropertyKey:
                object->as<TextInputBase>()->obscured(value);
                break;
            case TextInputBase::selectAllOnFocusPropertyKey:
                object->as<TextInputBase>()->selectAllOnFocus(value);
                break;
            case TextBase::fitFromBaselinePropertyKey:
                object->as<TextBase>()->fitFromBaseline(value);
                break;
            case TextBase::fitFontSizeResizesBoxPropertyKey:
                object->as<TextBase>()->fitFontSizeResizesBox(value);
                break;
            case ScriptAssetBase::isModulePropertyKey:
                object->as<ScriptAssetBase>()->isModule(value);
                break;
            case BitmapCacheBase::cacheEnabledPropertyKey:
                object->as<BitmapCacheBase>()->cacheEnabled(value);
                break;
            case BitmapCacheBase::ditherPropertyKey:
                object->as<BitmapCacheBase>()->dither(value);
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
            case LayerMaskBase::resolutionPropertyKey:
                object->as<LayerMaskBase>()->resolution(value);
                break;
            case LayerMaskBase::boundsXPropertyKey:
                object->as<LayerMaskBase>()->boundsX(value);
                break;
            case LayerMaskBase::boundsYPropertyKey:
                object->as<LayerMaskBase>()->boundsY(value);
                break;
            case LayerMaskBase::boundsWidthPropertyKey:
                object->as<LayerMaskBase>()->boundsWidth(value);
                break;
            case LayerMaskBase::boundsHeightPropertyKey:
                object->as<LayerMaskBase>()->boundsHeight(value);
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
            case FollowPathConstraintBase::distancePropertyKey:
                object->as<FollowPathConstraintBase>()->distance(value);
                break;
            case ListFollowPathConstraintBase::distanceEndPropertyKey:
                object->as<ListFollowPathConstraintBase>()->distanceEnd(value);
                break;
            case ListFollowPathConstraintBase::distanceOffsetPropertyKey:
                object->as<ListFollowPathConstraintBase>()->distanceOffset(
                    value);
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
            case ScrollConstraintBase::thresholdPropertyKey:
                object->as<ScrollConstraintBase>()->threshold(value);
                break;
            case ScrollConstraintBase::velocityXPropertyKey:
                object->as<ScrollConstraintBase>()->velocityX(value);
                break;
            case ScrollConstraintBase::velocityYPropertyKey:
                object->as<ScrollConstraintBase>()->velocityY(value);
                break;
            case ScrollConstraintBase::dragMultiplierPropertyKey:
                object->as<ScrollConstraintBase>()->dragMultiplier(value);
                break;
            case ScrollConstraintBase::computedContentWidthPropertyKey:
                object->as<ScrollConstraintBase>()->computedContentWidth(value);
                break;
            case ScrollConstraintBase::computedContentHeightPropertyKey:
                object->as<ScrollConstraintBase>()->computedContentHeight(
                    value);
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
            case NestedArtboardBase::speedPropertyKey:
                object->as<NestedArtboardBase>()->speed(value);
                break;
            case NestedArtboardBase::quantizePropertyKey:
                object->as<NestedArtboardBase>()->quantize(value);
                break;
            case NestedArtboardLayoutBase::instanceWidthPropertyKey:
                object->as<NestedArtboardLayoutBase>()->instanceWidth(value);
                break;
            case NestedArtboardLayoutBase::instanceHeightPropertyKey:
                object->as<NestedArtboardLayoutBase>()->instanceHeight(value);
                break;
            case GridTrackBase::trackValuePropertyKey:
                object->as<GridTrackBase>()->trackValue(value);
                break;
            case GridTrackBase::trackMaxValuePropertyKey:
                object->as<GridTrackBase>()->trackMaxValue(value);
                break;
            case LayoutSizingStyleBase::minWidthPropertyKey:
                object->as<LayoutSizingStyleBase>()->minWidth(value);
                break;
            case LayoutSizingStyleBase::maxWidthPropertyKey:
                object->as<LayoutSizingStyleBase>()->maxWidth(value);
                break;
            case LayoutSizingStyleBase::minHeightPropertyKey:
                object->as<LayoutSizingStyleBase>()->minHeight(value);
                break;
            case LayoutSizingStyleBase::maxHeightPropertyKey:
                object->as<LayoutSizingStyleBase>()->maxHeight(value);
                break;
            case LayoutNodeStyleBase::widthPropertyKey:
                object->as<LayoutNodeStyleBase>()->width(value);
                break;
            case LayoutNodeStyleBase::heightPropertyKey:
                object->as<LayoutNodeStyleBase>()->height(value);
                break;
            case LayoutNodeStyleBase::fractionalWidthPropertyKey:
                object->as<LayoutNodeStyleBase>()->fractionalWidth(value);
                break;
            case LayoutNodeStyleBase::fractionalHeightPropertyKey:
                object->as<LayoutNodeStyleBase>()->fractionalHeight(value);
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
            case ComponentOriginBase::originXPropertyKey:
                object->as<ComponentOriginBase>()->originX(value);
                break;
            case ComponentOriginBase::originYPropertyKey:
                object->as<ComponentOriginBase>()->originY(value);
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
            case PaintImageBase::imageScaleXPropertyKey:
                object->as<PaintImageBase>()->imageScaleX(value);
                break;
            case PaintImageBase::imageScaleYPropertyKey:
                object->as<PaintImageBase>()->imageScaleY(value);
                break;
            case PaintImageBase::imageOffsetXPropertyKey:
                object->as<PaintImageBase>()->imageOffsetX(value);
                break;
            case PaintImageBase::imageOffsetYPropertyKey:
                object->as<PaintImageBase>()->imageOffsetY(value);
                break;
            case PaintImageBase::imageRotationPropertyKey:
                object->as<PaintImageBase>()->imageRotation(value);
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
            case ImageBase::alignmentXPropertyKey:
                object->as<ImageBase>()->alignmentX(value);
                break;
            case ImageBase::alignmentYPropertyKey:
                object->as<ImageBase>()->alignmentY(value);
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
            case SelectionStyleBase::cornerRadiusPropertyKey:
                object->as<SelectionStyleBase>()->cornerRadius(value);
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
            case TextStyleBackgroundBase::cornerRadiusPropertyKey:
                object->as<TextStyleBackgroundBase>()->cornerRadius(value);
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
            case ExportAudioBase::volumePropertyKey:
                object->as<ExportAudioBase>()->volume(value);
                break;
            case DrawableAssetBase::heightPropertyKey:
                object->as<DrawableAssetBase>()->height(value);
                break;
            case DrawableAssetBase::widthPropertyKey:
                object->as<DrawableAssetBase>()->width(value);
                break;
            case BitmapCacheBase::resolutionPropertyKey:
                object->as<BitmapCacheBase>()->resolution(value);
                break;
        }
    }
    static void setCallback(Core* object, int propertyKey, CallbackData value)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceTriggerBase::firePropertyKey:
                object->as<ViewModelInstanceTriggerBase>()->fire(value);
                break;
            case CustomPropertyTriggerBase::firePropertyKey:
                object->as<CustomPropertyTriggerBase>()->fire(value);
                break;
            case NestedTriggerBase::firePropertyKey:
                object->as<NestedTriggerBase>()->fire(value);
                break;
            case EventBase::triggerPropertyKey:
                object->as<EventBase>()->trigger(value);
                break;
        }
    }
    static void setInt(Core* object, int propertyKey, int32_t value)
    {
        switch (propertyKey)
        {
            case GridItemPlacementBase::gridColumnPropertyKey:
                object->as<GridItemPlacementBase>()->gridColumn(value);
                break;
            case GridItemPlacementBase::gridRowPropertyKey:
                object->as<GridItemPlacementBase>()->gridRow(value);
                break;
            case KeyFrameIntBase::valuePropertyKey:
                object->as<KeyFrameIntBase>()->value(value);
                break;
        }
    }
    static Id getId(Core* object, int propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceListItemBase::viewModelIdPropertyKey:
                return object->as<ViewModelInstanceListItemBase>()
                    ->viewModelId();
            case ViewModelInstanceListItemBase::viewModelInstanceIdPropertyKey:
                return object->as<ViewModelInstanceListItemBase>()
                    ->viewModelInstanceId();
            case ComponentBase::parentIdPropertyKey:
                return object->as<ComponentBase>()->parentId();
            case ViewModelInstanceValueBase::viewModelPropertyIdPropertyKey:
                return object->as<ViewModelInstanceValueBase>()
                    ->viewModelPropertyId();
            case ViewModelPropertyEnumCustomBase::enumIdPropertyKey:
                return object->as<ViewModelPropertyEnumCustomBase>()->enumId();
            case ViewModelInstanceEnumBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceEnumBase>()->propertyValue();
            case ViewModelInstanceAssetBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceAssetBase>()
                    ->propertyValue();
            case ViewModelInstanceArtboardBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceArtboardBase>()
                    ->propertyValue();
            case ViewModelPropertyViewModelBase::
                viewModelReferenceIdPropertyKey:
                return object->as<ViewModelPropertyViewModelBase>()
                    ->viewModelReferenceId();
            case ViewModelInstanceBase::viewModelIdPropertyKey:
                return object->as<ViewModelInstanceBase>()->viewModelId();
            case ViewModelInstanceListBase::listSourcePropertyKey:
                return object->as<ViewModelInstanceListBase>()->listSource();
            case ViewModelInstanceViewModelBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceViewModelBase>()
                    ->propertyValue();
            case DrawTargetBase::drawableIdPropertyKey:
                return object->as<DrawTargetBase>()->drawableId();
            case LayerMaskBase::sourceIdPropertyKey:
                return object->as<LayerMaskBase>()->sourceId();
            case TargetedConstraintBase::targetIdPropertyKey:
                return object->as<TargetedConstraintBase>()->targetId();
            case ScrollPhysicsBase::constraintIdPropertyKey:
                return object->as<ScrollPhysicsBase>()->constraintId();
            case ScrollConstraintBase::physicsIdPropertyKey:
                return object->as<ScrollConstraintBase>()->physicsId();
            case ScrollBarConstraintBase::scrollConstraintIdPropertyKey:
                return object->as<ScrollBarConstraintBase>()
                    ->scrollConstraintId();
            case NestedArtboardBase::artboardIdPropertyKey:
                return object->as<NestedArtboardBase>()->artboardId();
            case ArtboardComponentListBase::listSourcePropertyKey:
                return object->as<ArtboardComponentListBase>()->listSource();
            case NestedAnimationBase::animationIdPropertyKey:
                return object->as<NestedAnimationBase>()->animationId();
            case SoloBase::activeComponentIdPropertyKey:
                return object->as<SoloBase>()->activeComponentId();
            case ScriptedDrawableBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedDrawableBase>()->scriptAssetId();
            case ScriptedDataConverterBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedDataConverterBase>()->scriptAssetId();
            case ScriptedTransitionBase::activeComponentIdPropertyKey:
                return object->as<ScriptedTransitionBase>()
                    ->activeComponentId();
            case ScriptedTransitionBase::listSourcePropertyKey:
                return object->as<ScriptedTransitionBase>()->listSource();
            case ScriptedInterpolatorBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedInterpolatorBase>()->scriptAssetId();
            case ScriptedPathEffectBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedPathEffectBase>()->scriptAssetId();
            case LayoutComponentStyleBase::interpolatorIdPropertyKey:
                return object->as<LayoutComponentStyleBase>()->interpolatorId();
            case ArtboardComponentListOverrideBase::artboardIdPropertyKey:
                return object->as<ArtboardComponentListOverrideBase>()
                    ->artboardId();
            case ListenerFireEventBase::eventIdPropertyKey:
                return object->as<ListenerFireEventBase>()->eventId();
            case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
                return object->as<InterpolatingKeyFrameBase>()
                    ->interpolatorId();
            case ListenerInputChangeBase::inputIdPropertyKey:
                return object->as<ListenerInputChangeBase>()->inputId();
            case ListenerInputChangeBase::nestedInputIdPropertyKey:
                return object->as<ListenerInputChangeBase>()->nestedInputId();
            case AnimationStateBase::animationIdPropertyKey:
                return object->as<AnimationStateBase>()->animationId();
            case NestedInputBase::inputIdPropertyKey:
                return object->as<NestedInputBase>()->inputId();
            case ScriptedListenerActionBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedListenerActionBase>()
                    ->scriptAssetId();
            case KeyedObjectBase::objectIdPropertyKey:
                return object->as<KeyedObjectBase>()->objectId();
            case BlendAnimationBase::animationIdPropertyKey:
                return object->as<BlendAnimationBase>()->animationId();
            case BlendAnimationDirectBase::inputIdPropertyKey:
                return object->as<BlendAnimationDirectBase>()->inputId();
            case StateMachineListenerBase::targetIdPropertyKey:
                return object->as<StateMachineListenerBase>()->targetId();
            case StateMachineListenerSingleBase::eventIdPropertyKey:
                return object->as<StateMachineListenerSingleBase>()->eventId();
            case TransitionInputConditionBase::inputIdPropertyKey:
                return object->as<TransitionInputConditionBase>()->inputId();
            case KeyFrameIdBase::valuePropertyKey:
                return object->as<KeyFrameIdBase>()->value();
            case ListenerAlignTargetBase::targetIdPropertyKey:
                return object->as<ListenerAlignTargetBase>()->targetId();
            case ScriptedTransitionConditionBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedTransitionConditionBase>()
                    ->scriptAssetId();
            case BlendState1DInputBase::inputIdPropertyKey:
                return object->as<BlendState1DInputBase>()->inputId();
            case FocusActionTargetBase::targetIdPropertyKey:
                return object->as<FocusActionTargetBase>()->targetId();
            case TransitionValueIdComparatorBase::valuePropertyKey:
                return object->as<TransitionValueIdComparatorBase>()->value();
            case StateTransitionBase::stateToIdPropertyKey:
                return object->as<StateTransitionBase>()->stateToId();
            case StateTransitionBase::interpolatorIdPropertyKey:
                return object->as<StateTransitionBase>()->interpolatorId();
            case StateMachineFireEventBase::eventIdPropertyKey:
                return object->as<StateMachineFireEventBase>()->eventId();
            case TransitionPropertyComponentComparatorBase::objectIdPropertyKey:
                return object->as<TransitionPropertyComponentComparatorBase>()
                    ->objectId();
            case ListenerInputTypeEventBase::eventIdPropertyKey:
                return object->as<ListenerInputTypeEventBase>()->eventId();
            case BlendStateTransitionBase::exitBlendAnimationIdPropertyKey:
                return object->as<BlendStateTransitionBase>()
                    ->exitBlendAnimationId();
            case TargetEffectBase::targetIdPropertyKey:
                return object->as<TargetEffectBase>()->targetId();
            case PaintImageBase::imageAssetIdPropertyKey:
                return object->as<PaintImageBase>()->imageAssetId();
            case ListPathBase::listSourcePropertyKey:
                return object->as<ListPathBase>()->listSource();
            case ClippingShapeBase::sourceIdPropertyKey:
                return object->as<ClippingShapeBase>()->sourceId();
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
            case JoystickBase::handleSourceIdPropertyKey:
                return object->as<JoystickBase>()->handleSourceId();
            case BindablePropertyIdBase::propertyValuePropertyKey:
                return object->as<BindablePropertyIdBase>()->propertyValue();
            case DataBindBase::converterIdPropertyKey:
                return object->as<DataBindBase>()->converterId();
            case DataConverterNumberToListBase::viewModelIdPropertyKey:
                return object->as<DataConverterNumberToListBase>()
                    ->viewModelId();
            case DataConverterRangeMapperBase::interpolatorIdPropertyKey:
                return object->as<DataConverterRangeMapperBase>()
                    ->interpolatorId();
            case DataConverterInterpolatorBase::interpolatorIdPropertyKey:
                return object->as<DataConverterInterpolatorBase>()
                    ->interpolatorId();
            case DataConverterGroupItemBase::converterIdPropertyKey:
                return object->as<DataConverterGroupItemBase>()->converterId();
            case BindablePropertyListBase::propertyValuePropertyKey:
                return object->as<BindablePropertyListBase>()->propertyValue();
            case BindablePropertyEnumBase::propertyValuePropertyKey:
                return object->as<BindablePropertyEnumBase>()->propertyValue();
            case TendonBase::boneIdPropertyKey:
                return object->as<TendonBase>()->boneId();
            case TextModifierRangeBase::runIdPropertyKey:
                return object->as<TextModifierRangeBase>()->runId();
            case TextTargetModifierBase::targetIdPropertyKey:
                return object->as<TextTargetModifierBase>()->targetId();
            case TextStyleBase::fontAssetIdPropertyKey:
                return object->as<TextStyleBase>()->fontAssetId();
            case TextBase::textRunListSourcePropertyKey:
                return object->as<TextBase>()->textRunListSource();
            case TextValueRunBase::styleIdPropertyKey:
                return object->as<TextValueRunBase>()->styleId();
            case ArtboardListMapRuleBase::artboardIdPropertyKey:
                return object->as<ArtboardListMapRuleBase>()->artboardId();
            case ArtboardListMapRuleBase::viewModelIdPropertyKey:
                return object->as<ArtboardListMapRuleBase>()->viewModelId();
            case CustomPropertyEnumBase::propertyValuePropertyKey:
                return object->as<CustomPropertyEnumBase>()->propertyValue();
            case CustomPropertyEnumBase::enumIdPropertyKey:
                return object->as<CustomPropertyEnumBase>()->enumId();
            case AudioEventBase::assetIdPropertyKey:
                return object->as<AudioEventBase>()->assetId();
            case ScriptInputArtboardBase::artboardIdPropertyKey:
                return object->as<ScriptInputArtboardBase>()->artboardId();
        }
        return kEmptyId;
    }
    static std::string getString(Core* object, int propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelComponentBase::namePropertyKey:
                return object->as<ViewModelComponentBase>()->name();
            case ComponentBase::namePropertyKey:
                return object->as<ComponentBase>()->name();
            case DataEnumCustomBase::namePropertyKey:
                return object->as<DataEnumCustomBase>()->name();
            case ViewModelInstanceStringBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceStringBase>()
                    ->propertyValue();
            case DataEnumValueBase::keyPropertyKey:
                return object->as<DataEnumValueBase>()->key();
            case DataEnumValueBase::valuePropertyKey:
                return object->as<DataEnumValueBase>()->value();
            case AssetBase::namePropertyKey:
                return object->as<AssetBase>()->name();
            case DataConverterBase::namePropertyKey:
                return object->as<DataConverterBase>()->name();
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
            case SemanticDataBase::labelPropertyKey:
                return object->as<SemanticDataBase>()->label();
            case SemanticDataBase::valuePropertyKey:
                return object->as<SemanticDataBase>()->value();
            case SemanticDataBase::hintPropertyKey:
                return object->as<SemanticDataBase>()->hint();
            case CustomPropertyStringBase::propertyValuePropertyKey:
                return object->as<CustomPropertyStringBase>()->propertyValue();
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
            case FileAssetBase::cdnBaseUrlPropertyKey:
                return object->as<FileAssetBase>()->cdnBaseUrl();
            case TextAssetBase::folderPathPropertyKey:
                return object->as<TextAssetBase>()->folderPath();
        }
        return "";
    }
    static uint32_t getUint(Core* object, int propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelPropertyBase::symbolTypeValuePropertyKey:
                return object->as<ViewModelPropertyBase>()->symbolTypeValue();
            case ViewModelPropertyBase::componentPropsPropertyKey:
                return object->as<ViewModelPropertyBase>()->componentProps();
            case ViewModelPropertyEnumSystemBase::enumTypePropertyKey:
                return object->as<ViewModelPropertyEnumSystemBase>()
                    ->enumType();
            case ViewModelBase::viewModelTypePropertyKey:
                return object->as<ViewModelBase>()->viewModelType();
            case DataEnumSystemBase::enumTypePropertyKey:
                return object->as<DataEnumSystemBase>()->enumType();
            case ViewModelInstanceTriggerBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceTriggerBase>()
                    ->propertyValue();
            case ViewModelInstanceSymbolListIndexBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceSymbolListIndexBase>()
                    ->propertyValue();
            case CustomPropertyBase::nameIdPropertyKey:
                return object->as<CustomPropertyBase>()->nameId();
            case CustomPropertyTriggerBase::propertyValuePropertyKey:
                return object->as<CustomPropertyTriggerBase>()->propertyValue();
            case DrawTargetBase::placementValuePropertyKey:
                return object->as<DrawTargetBase>()->placementValue();
            case LayerMaskBase::maskFlagsPropertyKey:
                return object->as<LayerMaskBase>()->maskFlags();
            case LayerMaskBase::maskModeValuePropertyKey:
                return object->as<LayerMaskBase>()->maskModeValue();
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
            case DraggableConstraintBase::directionValuePropertyKey:
                return object->as<DraggableConstraintBase>()->directionValue();
            case ScrollConstraintBase::physicsTypeValuePropertyKey:
                return object->as<ScrollConstraintBase>()->physicsTypeValue();
            case ScrollConstraintBase::virtualizeBufferPropertyKey:
                return object->as<ScrollConstraintBase>()->virtualizeBuffer();
            case ScrollConstraintBase::scrollFlagsPropertyKey:
                return object->as<ScrollConstraintBase>()->scrollFlags();
            case DrawableBase::blendModeValuePropertyKey:
                return object->as<DrawableBase>()->blendModeValue();
            case DrawableBase::additiveAmountPropertyKey:
                return object->as<DrawableBase>()->additiveAmount();
            case DrawableBase::drawableFlagsPropertyKey:
                return object->as<DrawableBase>()->drawableFlags();
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
            case GridTrackBase::collectionPropertyKey:
                return object->as<GridTrackBase>()->collection();
            case GridTrackBase::trackTypePropertyKey:
                return object->as<GridTrackBase>()->trackType();
            case GridTrackBase::trackMaxTypePropertyKey:
                return object->as<GridTrackBase>()->trackMaxType();
            case GridItemPlacementBase::gridColumnSpanPropertyKey:
                return object->as<GridItemPlacementBase>()->gridColumnSpan();
            case GridItemPlacementBase::gridRowSpanPropertyKey:
                return object->as<GridItemPlacementBase>()->gridRowSpan();
            case LayoutSizingStyleBase::minWidthUnitsValuePropertyKey:
                return object->as<LayoutSizingStyleBase>()
                    ->minWidthUnitsValue();
            case LayoutSizingStyleBase::maxWidthUnitsValuePropertyKey:
                return object->as<LayoutSizingStyleBase>()
                    ->maxWidthUnitsValue();
            case LayoutSizingStyleBase::minHeightUnitsValuePropertyKey:
                return object->as<LayoutSizingStyleBase>()
                    ->minHeightUnitsValue();
            case LayoutSizingStyleBase::maxHeightUnitsValuePropertyKey:
                return object->as<LayoutSizingStyleBase>()
                    ->maxHeightUnitsValue();
            case LayoutSizingStyleBase::layoutWidthScaleTypePropertyKey:
                return object->as<LayoutSizingStyleBase>()
                    ->layoutWidthScaleType();
            case LayoutSizingStyleBase::layoutHeightScaleTypePropertyKey:
                return object->as<LayoutSizingStyleBase>()
                    ->layoutHeightScaleType();
            case LayoutSizingStyleBase::widthUnitsValuePropertyKey:
                return object->as<LayoutSizingStyleBase>()->widthUnitsValue();
            case LayoutSizingStyleBase::heightUnitsValuePropertyKey:
                return object->as<LayoutSizingStyleBase>()->heightUnitsValue();
            case LayoutSizingStyleBase::justifySelfValuePropertyKey:
                return object->as<LayoutSizingStyleBase>()->justifySelfValue();
            case LayoutSizingStyleBase::displayValuePropertyKey:
                return object->as<LayoutSizingStyleBase>()->displayValue();
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
            case LayoutComponentStyleBase::flexBasisUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->flexBasisUnitsValue();
            case LayoutComponentStyleBase::layoutAlignmentTypePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->layoutAlignmentType();
            case LayoutComponentStyleBase::animationStyleTypePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->animationStyleType();
            case LayoutComponentStyleBase::interpolationTypePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->interpolationType();
            case LayoutComponentStyleBase::positionTypeValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->positionTypeValue();
            case LayoutComponentStyleBase::flexDirectionValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->flexDirectionValue();
            case LayoutComponentStyleBase::directionValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()->directionValue();
            case LayoutComponentStyleBase::flexWrapValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()->flexWrapValue();
            case LayoutComponentStyleBase::overflowValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()->overflowValue();
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
            case LayoutComponentStyleBase::gapHorizontalUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->gapHorizontalUnitsValue();
            case LayoutComponentStyleBase::gapVerticalUnitsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->gapVerticalUnitsValue();
            case LayoutComponentStyleBase::justifyItemsValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->justifyItemsValue();
            case LayoutComponentStyleBase::layoutTypeValuePropertyKey:
                return object->as<LayoutComponentStyleBase>()
                    ->layoutTypeValue();
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
            case ListenerActionBase::flagsPropertyKey:
                return object->as<ListenerActionBase>()->flags();
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
            case KeyFrameUintBase::valuePropertyKey:
                return object->as<KeyFrameUintBase>()->value();
            case BlendAnimationDirectBase::blendSourcePropertyKey:
                return object->as<BlendAnimationDirectBase>()->blendSource();
            case StateMachineListenerSingleBase::listenerTypeValuePropertyKey:
                return object->as<StateMachineListenerSingleBase>()
                    ->listenerTypeValue();
            case KeyedPropertyBase::propertyKeyPropertyKey:
                return object->as<KeyedPropertyBase>()->propertyKey();
            case TransitionPropertyArtboardComparatorBase::
                propertyTypePropertyKey:
                return object->as<TransitionPropertyArtboardComparatorBase>()
                    ->propertyType();
            case ListenerBoolChangeBase::valuePropertyKey:
                return object->as<ListenerBoolChangeBase>()->value();
            case TransitionViewModelConditionBase::opValuePropertyKey:
                return object->as<TransitionViewModelConditionBase>()
                    ->opValue();
            case TransitionValueConditionBase::opValuePropertyKey:
                return object->as<TransitionValueConditionBase>()->opValue();
            case StateTransitionBase::flagsPropertyKey:
                return object->as<StateTransitionBase>()->flags();
            case StateTransitionBase::durationPropertyKey:
                return object->as<StateTransitionBase>()->duration();
            case StateTransitionBase::exitTimePropertyKey:
                return object->as<StateTransitionBase>()->exitTime();
            case StateTransitionBase::interpolationTypePropertyKey:
                return object->as<StateTransitionBase>()->interpolationType();
            case StateTransitionBase::randomWeightPropertyKey:
                return object->as<StateTransitionBase>()->randomWeight();
            case FocusActionTraversalBase::traversalKindPropertyKey:
                return object->as<FocusActionTraversalBase>()->traversalKind();
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
            case ListenerViewModelChangeBase::inputValuePropertyKey:
                return object->as<ListenerViewModelChangeBase>()->inputValue();
            case ListenerViewModelChangeBase::inputValueIndexPropertyKey:
                return object->as<ListenerViewModelChangeBase>()
                    ->inputValueIndex();
            case TransitionPropertyComponentComparatorBase::
                propertyKeyPropertyKey:
                return object->as<TransitionPropertyComponentComparatorBase>()
                    ->propertyKey();
            case ElasticInterpolatorBase::easingValuePropertyKey:
                return object->as<ElasticInterpolatorBase>()->easingValue();
            case ListenerInputTypeBase::listenerTypeValuePropertyKey:
                return object->as<ListenerInputTypeBase>()->listenerTypeValue();
            case ListenerInputTypePointerButtonBase::
                pointerButtonValuePropertyKey:
                return object->as<ListenerInputTypePointerButtonBase>()
                    ->pointerButtonValue();
            case ShapePaintBase::blendModeValuePropertyKey:
                return object->as<ShapePaintBase>()->blendModeValue();
            case ShapePaintBase::additiveAmountPropertyKey:
                return object->as<ShapePaintBase>()->additiveAmount();
            case ColorChannelsBase::colorRedPropertyKey:
                if (auto* _c = ColorChannelsBase::from(object))
                {
                    return _c->colorRed();
                }
                return 0u;
            case ColorChannelsBase::colorGreenPropertyKey:
                if (auto* _c = ColorChannelsBase::from(object))
                {
                    return _c->colorGreen();
                }
                return 0u;
            case ColorChannelsBase::colorBluePropertyKey:
                if (auto* _c = ColorChannelsBase::from(object))
                {
                    return _c->colorBlue();
                }
                return 0u;
            case ColorChannelsBase::colorAlphaPropertyKey:
                if (auto* _c = ColorChannelsBase::from(object))
                {
                    return _c->colorAlpha();
                }
                return 0u;
            case StrokeBase::capPropertyKey:
                return object->as<StrokeBase>()->cap();
            case StrokeBase::joinPropertyKey:
                return object->as<StrokeBase>()->join();
            case StrokeBase::positionPropertyKey:
                return object->as<StrokeBase>()->position();
            case PaintImageBase::imageSamplerFilterPropertyKey:
                return object->as<PaintImageBase>()->imageSamplerFilter();
            case PaintImageBase::imageSamplerWrapXPropertyKey:
                return object->as<PaintImageBase>()->imageSamplerWrapX();
            case PaintImageBase::imageSamplerWrapYPropertyKey:
                return object->as<PaintImageBase>()->imageSamplerWrapY();
            case PaintImageBase::imageSizeModePropertyKey:
                return object->as<PaintImageBase>()->imageSizeMode();
            case FeatherBase::spaceValuePropertyKey:
                return object->as<FeatherBase>()->spaceValue();
            case TrimPathBase::modeValuePropertyKey:
                return object->as<TrimPathBase>()->modeValue();
            case FillBase::fillRulePropertyKey:
                return object->as<FillBase>()->fillRule();
            case PathBase::pathFlagsPropertyKey:
                return object->as<PathBase>()->pathFlags();
            case ClippingShapeBase::fillRulePropertyKey:
                return object->as<ClippingShapeBase>()->fillRule();
            case PolygonBase::pointsPropertyKey:
                return object->as<PolygonBase>()->points();
            case ImageBase::fitPropertyKey:
                return object->as<ImageBase>()->fit();
            case ImageBase::samplerFilterPropertyKey:
                return object->as<ImageBase>()->samplerFilter();
            case ImageBase::samplerWrapXPropertyKey:
                return object->as<ImageBase>()->samplerWrapX();
            case ImageBase::samplerWrapYPropertyKey:
                return object->as<ImageBase>()->samplerWrapY();
            case FocusDataBase::focusFlagsPropertyKey:
                return object->as<FocusDataBase>()->focusFlags();
            case FocusDataBase::edgeBehaviorValuePropertyKey:
                return object->as<FocusDataBase>()->edgeBehaviorValue();
            case JoystickBase::joystickFlagsPropertyKey:
                return object->as<JoystickBase>()->joystickFlags();
            case OpenUrlEventBase::targetValuePropertyKey:
                return object->as<OpenUrlEventBase>()->targetValue();
            case SemanticDataBase::rolePropertyKey:
                return object->as<SemanticDataBase>()->role();
            case SemanticDataBase::headingLevelPropertyKey:
                return object->as<SemanticDataBase>()->headingLevel();
            case SemanticDataBase::traitFlagsPropertyKey:
                return object->as<SemanticDataBase>()->traitFlags();
            case SemanticDataBase::stateFlagsPropertyKey:
                return object->as<SemanticDataBase>()->stateFlags();
            case SemanticDataBase::isCheckedPropertyKey:
                return object->as<SemanticDataBase>()->isChecked();
            case BindablePropertyIntegerBase::propertyValuePropertyKey:
                return object->as<BindablePropertyIntegerBase>()
                    ->propertyValue();
            case DataBindBase::propertyKeyPropertyKey:
                return object->as<DataBindBase>()->propertyKey();
            case DataBindBase::flagsPropertyKey:
                return object->as<DataBindBase>()->flags();
            case DataConverterFormulaBase::randomModeValuePropertyKey:
                return object->as<DataConverterFormulaBase>()
                    ->randomModeValue();
            case DataConverterOperationBase::operationTypePropertyKey:
                return object->as<DataConverterOperationBase>()
                    ->operationType();
            case DataConverterRangeMapperBase::interpolationTypePropertyKey:
                return object->as<DataConverterRangeMapperBase>()
                    ->interpolationType();
            case DataConverterRangeMapperBase::flagsPropertyKey:
                return object->as<DataConverterRangeMapperBase>()->flags();
            case DataConverterInterpolatorBase::interpolationTypePropertyKey:
                return object->as<DataConverterInterpolatorBase>()
                    ->interpolationType();
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
            case NestedArtboardLeafBase::fitPropertyKey:
                return object->as<NestedArtboardLeafBase>()->fit();
            case WeightBase::valuesPropertyKey:
                return object->as<WeightBase>()->values();
            case WeightBase::indicesPropertyKey:
                return object->as<WeightBase>()->indices();
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
            case TextStyleFeatureBase::tagPropertyKey:
                return object->as<TextStyleFeatureBase>()->tag();
            case TextStyleFeatureBase::featureValuePropertyKey:
                return object->as<TextStyleFeatureBase>()->featureValue();
            case TextVariationModifierBase::axisTagPropertyKey:
                return object->as<TextVariationModifierBase>()->axisTag();
            case TextModifierGroupBase::modifierFlagsPropertyKey:
                return object->as<TextModifierGroupBase>()->modifierFlags();
            case TextInputBase::alignValuePropertyKey:
                return object->as<TextInputBase>()->alignValue();
            case TextInputBase::verticalAlignValuePropertyKey:
                return object->as<TextInputBase>()->verticalAlignValue();
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
            case TextBase::wordBreakValuePropertyKey:
                return object->as<TextBase>()->wordBreakValue();
            case TextBase::verticalAlignValuePropertyKey:
                return object->as<TextBase>()->verticalAlignValue();
            case TextBase::verticalTrimValuePropertyKey:
                return object->as<TextBase>()->verticalTrimValue();
            case TextBase::verticalTrimTopValuePropertyKey:
                return object->as<TextBase>()->verticalTrimTopValue();
            case TextBase::verticalTrimBottomValuePropertyKey:
                return object->as<TextBase>()->verticalTrimBottomValue();
            case FileAssetBase::assetIdPropertyKey:
                return object->as<FileAssetBase>()->assetId();
            case ScriptAssetBase::generatorFunctionRefPropertyKey:
                return object->as<ScriptAssetBase>()->generatorFunctionRef();
            case ScriptAssetBase::serializedImplementedMethodsPropertyKey:
                return object->as<ScriptAssetBase>()
                    ->serializedImplementedMethods();
            case ImageAssetBase::samplerFilterPropertyKey:
                return object->as<ImageAssetBase>()->samplerFilter();
            case ImageAssetBase::samplerWrapXPropertyKey:
                return object->as<ImageAssetBase>()->samplerWrapX();
            case ImageAssetBase::samplerWrapYPropertyKey:
                return object->as<ImageAssetBase>()->samplerWrapY();
            case ScriptModuleAssetBase::languagePropertyKey:
                return object->as<ScriptModuleAssetBase>()->language();
            case BitmapCacheBase::cacheFlagsPropertyKey:
                return object->as<BitmapCacheBase>()->cacheFlags();
            case GamepadInputBase::kindPropertyKey:
                return object->as<GamepadInputBase>()->kind();
            case GamepadInputBase::mappingPropertyKey:
                return object->as<GamepadInputBase>()->mapping();
            case GamepadInputBase::inputIndexPropertyKey:
                return object->as<GamepadInputBase>()->inputIndex();
            case GamepadInputBase::buttonPhasePropertyKey:
                return object->as<GamepadInputBase>()->buttonPhase();
            case KeyboardInputBase::keyTypePropertyKey:
                return object->as<KeyboardInputBase>()->keyType();
            case KeyboardInputBase::keyPhasePropertyKey:
                return object->as<KeyboardInputBase>()->keyPhase();
            case KeyboardInputBase::modifiersPropertyKey:
                return object->as<KeyboardInputBase>()->modifiers();
            case SemanticInputBase::actionTypePropertyKey:
                return object->as<SemanticInputBase>()->actionType();
            case ViewModelInstanceListItemBase::viewModelIdPropertyKey:
                return object->as<ViewModelInstanceListItemBase>()
                    ->viewModelId();
            case ViewModelInstanceListItemBase::viewModelInstanceIdPropertyKey:
                return object->as<ViewModelInstanceListItemBase>()
                    ->viewModelInstanceId();
            case ComponentBase::parentIdPropertyKey:
                return object->as<ComponentBase>()->parentId();
            case ViewModelInstanceValueBase::viewModelPropertyIdPropertyKey:
                return object->as<ViewModelInstanceValueBase>()
                    ->viewModelPropertyId();
            case ViewModelPropertyEnumCustomBase::enumIdPropertyKey:
                return object->as<ViewModelPropertyEnumCustomBase>()->enumId();
            case ViewModelInstanceEnumBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceEnumBase>()->propertyValue();
            case ViewModelInstanceAssetBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceAssetBase>()
                    ->propertyValue();
            case ViewModelInstanceArtboardBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceArtboardBase>()
                    ->propertyValue();
            case ViewModelPropertyViewModelBase::
                viewModelReferenceIdPropertyKey:
                return object->as<ViewModelPropertyViewModelBase>()
                    ->viewModelReferenceId();
            case ViewModelInstanceBase::viewModelIdPropertyKey:
                return object->as<ViewModelInstanceBase>()->viewModelId();
            case ViewModelInstanceListBase::listSourcePropertyKey:
                return object->as<ViewModelInstanceListBase>()->listSource();
            case ViewModelInstanceViewModelBase::propertyValuePropertyKey:
                return object->as<ViewModelInstanceViewModelBase>()
                    ->propertyValue();
            case DrawTargetBase::drawableIdPropertyKey:
                return object->as<DrawTargetBase>()->drawableId();
            case LayerMaskBase::sourceIdPropertyKey:
                return object->as<LayerMaskBase>()->sourceId();
            case TargetedConstraintBase::targetIdPropertyKey:
                return object->as<TargetedConstraintBase>()->targetId();
            case ScrollPhysicsBase::constraintIdPropertyKey:
                return object->as<ScrollPhysicsBase>()->constraintId();
            case ScrollConstraintBase::physicsIdPropertyKey:
                return object->as<ScrollConstraintBase>()->physicsId();
            case ScrollBarConstraintBase::scrollConstraintIdPropertyKey:
                return object->as<ScrollBarConstraintBase>()
                    ->scrollConstraintId();
            case NestedArtboardBase::artboardIdPropertyKey:
                return object->as<NestedArtboardBase>()->artboardId();
            case ArtboardComponentListBase::listSourcePropertyKey:
                return object->as<ArtboardComponentListBase>()->listSource();
            case NestedAnimationBase::animationIdPropertyKey:
                return object->as<NestedAnimationBase>()->animationId();
            case SoloBase::activeComponentIdPropertyKey:
                return object->as<SoloBase>()->activeComponentId();
            case ScriptedDrawableBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedDrawableBase>()->scriptAssetId();
            case ScriptedDataConverterBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedDataConverterBase>()->scriptAssetId();
            case ScriptedTransitionBase::activeComponentIdPropertyKey:
                return object->as<ScriptedTransitionBase>()
                    ->activeComponentId();
            case ScriptedTransitionBase::listSourcePropertyKey:
                return object->as<ScriptedTransitionBase>()->listSource();
            case ScriptedInterpolatorBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedInterpolatorBase>()->scriptAssetId();
            case ScriptedPathEffectBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedPathEffectBase>()->scriptAssetId();
            case LayoutComponentStyleBase::interpolatorIdPropertyKey:
                return object->as<LayoutComponentStyleBase>()->interpolatorId();
            case ArtboardComponentListOverrideBase::artboardIdPropertyKey:
                return object->as<ArtboardComponentListOverrideBase>()
                    ->artboardId();
            case ListenerFireEventBase::eventIdPropertyKey:
                return object->as<ListenerFireEventBase>()->eventId();
            case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
                return object->as<InterpolatingKeyFrameBase>()
                    ->interpolatorId();
            case ListenerInputChangeBase::inputIdPropertyKey:
                return object->as<ListenerInputChangeBase>()->inputId();
            case ListenerInputChangeBase::nestedInputIdPropertyKey:
                return object->as<ListenerInputChangeBase>()->nestedInputId();
            case AnimationStateBase::animationIdPropertyKey:
                return object->as<AnimationStateBase>()->animationId();
            case NestedInputBase::inputIdPropertyKey:
                return object->as<NestedInputBase>()->inputId();
            case ScriptedListenerActionBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedListenerActionBase>()
                    ->scriptAssetId();
            case KeyedObjectBase::objectIdPropertyKey:
                return object->as<KeyedObjectBase>()->objectId();
            case BlendAnimationBase::animationIdPropertyKey:
                return object->as<BlendAnimationBase>()->animationId();
            case BlendAnimationDirectBase::inputIdPropertyKey:
                return object->as<BlendAnimationDirectBase>()->inputId();
            case StateMachineListenerBase::targetIdPropertyKey:
                return object->as<StateMachineListenerBase>()->targetId();
            case StateMachineListenerSingleBase::eventIdPropertyKey:
                return object->as<StateMachineListenerSingleBase>()->eventId();
            case TransitionInputConditionBase::inputIdPropertyKey:
                return object->as<TransitionInputConditionBase>()->inputId();
            case KeyFrameIdBase::valuePropertyKey:
                return object->as<KeyFrameIdBase>()->value();
            case ListenerAlignTargetBase::targetIdPropertyKey:
                return object->as<ListenerAlignTargetBase>()->targetId();
            case ScriptedTransitionConditionBase::scriptAssetIdPropertyKey:
                return object->as<ScriptedTransitionConditionBase>()
                    ->scriptAssetId();
            case BlendState1DInputBase::inputIdPropertyKey:
                return object->as<BlendState1DInputBase>()->inputId();
            case FocusActionTargetBase::targetIdPropertyKey:
                return object->as<FocusActionTargetBase>()->targetId();
            case TransitionValueIdComparatorBase::valuePropertyKey:
                return object->as<TransitionValueIdComparatorBase>()->value();
            case StateTransitionBase::stateToIdPropertyKey:
                return object->as<StateTransitionBase>()->stateToId();
            case StateTransitionBase::interpolatorIdPropertyKey:
                return object->as<StateTransitionBase>()->interpolatorId();
            case StateMachineFireEventBase::eventIdPropertyKey:
                return object->as<StateMachineFireEventBase>()->eventId();
            case TransitionPropertyComponentComparatorBase::objectIdPropertyKey:
                return object->as<TransitionPropertyComponentComparatorBase>()
                    ->objectId();
            case ListenerInputTypeEventBase::eventIdPropertyKey:
                return object->as<ListenerInputTypeEventBase>()->eventId();
            case BlendStateTransitionBase::exitBlendAnimationIdPropertyKey:
                return object->as<BlendStateTransitionBase>()
                    ->exitBlendAnimationId();
            case TargetEffectBase::targetIdPropertyKey:
                return object->as<TargetEffectBase>()->targetId();
            case PaintImageBase::imageAssetIdPropertyKey:
                return object->as<PaintImageBase>()->imageAssetId();
            case ListPathBase::listSourcePropertyKey:
                return object->as<ListPathBase>()->listSource();
            case ClippingShapeBase::sourceIdPropertyKey:
                return object->as<ClippingShapeBase>()->sourceId();
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
            case JoystickBase::handleSourceIdPropertyKey:
                return object->as<JoystickBase>()->handleSourceId();
            case BindablePropertyIdBase::propertyValuePropertyKey:
                return object->as<BindablePropertyIdBase>()->propertyValue();
            case DataBindBase::converterIdPropertyKey:
                return object->as<DataBindBase>()->converterId();
            case DataConverterNumberToListBase::viewModelIdPropertyKey:
                return object->as<DataConverterNumberToListBase>()
                    ->viewModelId();
            case DataConverterRangeMapperBase::interpolatorIdPropertyKey:
                return object->as<DataConverterRangeMapperBase>()
                    ->interpolatorId();
            case DataConverterInterpolatorBase::interpolatorIdPropertyKey:
                return object->as<DataConverterInterpolatorBase>()
                    ->interpolatorId();
            case DataConverterGroupItemBase::converterIdPropertyKey:
                return object->as<DataConverterGroupItemBase>()->converterId();
            case BindablePropertyListBase::propertyValuePropertyKey:
                return object->as<BindablePropertyListBase>()->propertyValue();
            case BindablePropertyEnumBase::propertyValuePropertyKey:
                return object->as<BindablePropertyEnumBase>()->propertyValue();
            case TendonBase::boneIdPropertyKey:
                return object->as<TendonBase>()->boneId();
            case TextModifierRangeBase::runIdPropertyKey:
                return object->as<TextModifierRangeBase>()->runId();
            case TextTargetModifierBase::targetIdPropertyKey:
                return object->as<TextTargetModifierBase>()->targetId();
            case TextStyleBase::fontAssetIdPropertyKey:
                return object->as<TextStyleBase>()->fontAssetId();
            case TextBase::textRunListSourcePropertyKey:
                return object->as<TextBase>()->textRunListSource();
            case TextValueRunBase::styleIdPropertyKey:
                return object->as<TextValueRunBase>()->styleId();
            case ArtboardListMapRuleBase::artboardIdPropertyKey:
                return object->as<ArtboardListMapRuleBase>()->artboardId();
            case ArtboardListMapRuleBase::viewModelIdPropertyKey:
                return object->as<ArtboardListMapRuleBase>()->viewModelId();
            case CustomPropertyEnumBase::propertyValuePropertyKey:
                return object->as<CustomPropertyEnumBase>()->propertyValue();
            case CustomPropertyEnumBase::enumIdPropertyKey:
                return object->as<CustomPropertyEnumBase>()->enumId();
            case AudioEventBase::assetIdPropertyKey:
                return object->as<AudioEventBase>()->assetId();
            case ScriptInputArtboardBase::artboardIdPropertyKey:
                return object->as<ScriptInputArtboardBase>()->artboardId();
        }
        return 0;
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
            case SelectionStyleBase::highlightColorPropertyKey:
                return object->as<SelectionStyleBase>()->highlightColor();
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
            case LayerMaskBase::isVisiblePropertyKey:
                return object->as<LayerMaskBase>()->isVisible();
            case LayerMaskBase::sourceDrawsPropertyKey:
                return object->as<LayerMaskBase>()->sourceDraws();
            case LayerMaskBase::useCustomBoundsPropertyKey:
                return object->as<LayerMaskBase>()->useCustomBounds();
            case FollowPathConstraintBase::orientPropertyKey:
                return object->as<FollowPathConstraintBase>()->orient();
            case FollowPathConstraintBase::offsetPropertyKey:
                return object->as<FollowPathConstraintBase>()->offset();
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
            case ScrollConstraintBase::snapPropertyKey:
                return object->as<ScrollConstraintBase>()->snap();
            case ScrollConstraintBase::virtualizePropertyKey:
                return object->as<ScrollConstraintBase>()->virtualize();
            case ScrollConstraintBase::infinitePropertyKey:
                return object->as<ScrollConstraintBase>()->infinite();
            case ScrollConstraintBase::interactivePropertyKey:
                return object->as<ScrollConstraintBase>()->interactive();
            case ScrollConstraintBase::scrollActivePropertyKey:
                return object->as<ScrollConstraintBase>()->scrollActive();
            case ScrollConstraintBase::wheelInteractivePropertyKey:
                return object->as<ScrollConstraintBase>()->wheelInteractive();
            case ScrollBarConstraintBase::autoSizePropertyKey:
                return object->as<ScrollBarConstraintBase>()->autoSize();
            case NestedArtboardBase::isPausedPropertyKey:
                return object->as<NestedArtboardBase>()->isPaused();
            case NestedArtboardBase::isStatefulPropertyKey:
                return object->as<NestedArtboardBase>()->isStateful();
            case LayoutSizingStyleBase::hugUnboundedPropertyKey:
                return object->as<LayoutSizingStyleBase>()->hugUnbounded();
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
            case FocusDataBase::canFocusPropertyKey:
                return object->as<FocusDataBase>()->canFocus();
            case FocusDataBase::canTouchPropertyKey:
                return object->as<FocusDataBase>()->canTouch();
            case FocusDataBase::canTraversePropertyKey:
                return object->as<FocusDataBase>()->canTraverse();
            case CustomPropertyBooleanBase::propertyValuePropertyKey:
                return object->as<CustomPropertyBooleanBase>()->propertyValue();
            case LayoutComponentBase::clipPropertyKey:
                return object->as<LayoutComponentBase>()->clip();
            case SemanticDataBase::isExpandablePropertyKey:
                return object->as<SemanticDataBase>()->isExpandable();
            case SemanticDataBase::isSelectablePropertyKey:
                return object->as<SemanticDataBase>()->isSelectable();
            case SemanticDataBase::isCheckablePropertyKey:
                return object->as<SemanticDataBase>()->isCheckable();
            case SemanticDataBase::isToggleablePropertyKey:
                return object->as<SemanticDataBase>()->isToggleable();
            case SemanticDataBase::isRequirablePropertyKey:
                return object->as<SemanticDataBase>()->isRequirable();
            case SemanticDataBase::isEnablablePropertyKey:
                return object->as<SemanticDataBase>()->isEnablable();
            case SemanticDataBase::isFocusablePropertyKey:
                return object->as<SemanticDataBase>()->isFocusable();
            case SemanticDataBase::isExpandedPropertyKey:
                return object->as<SemanticDataBase>()->isExpanded();
            case SemanticDataBase::isSelectedPropertyKey:
                return object->as<SemanticDataBase>()->isSelected();
            case SemanticDataBase::isToggledPropertyKey:
                return object->as<SemanticDataBase>()->isToggled();
            case SemanticDataBase::isRequiredPropertyKey:
                return object->as<SemanticDataBase>()->isRequired();
            case SemanticDataBase::isDisabledPropertyKey:
                return object->as<SemanticDataBase>()->isDisabled();
            case SemanticDataBase::isFocusedPropertyKey:
                return object->as<SemanticDataBase>()->isFocused();
            case SemanticDataBase::isHiddenPropertyKey:
                return object->as<SemanticDataBase>()->isHidden();
            case SemanticDataBase::isLiveRegionPropertyKey:
                return object->as<SemanticDataBase>()->isLiveRegion();
            case SemanticDataBase::isReadOnlyPropertyKey:
                return object->as<SemanticDataBase>()->isReadOnly();
            case SemanticDataBase::isModalPropertyKey:
                return object->as<SemanticDataBase>()->isModal();
            case SemanticDataBase::isObscuredPropertyKey:
                return object->as<SemanticDataBase>()->isObscured();
            case SemanticDataBase::isMultilinePropertyKey:
                return object->as<SemanticDataBase>()->isMultiline();
            case DataBindPathBase::isRelativePropertyKey:
                return object->as<DataBindPathBase>()->isRelative();
            case BindablePropertyBooleanBase::propertyValuePropertyKey:
                return object->as<BindablePropertyBooleanBase>()
                    ->propertyValue();
            case NestedArtboardLeafBase::fitToLayoutParentPropertyKey:
                return object->as<NestedArtboardLeafBase>()
                    ->fitToLayoutParent();
            case TextModifierRangeBase::clampPropertyKey:
                return object->as<TextModifierRangeBase>()->clamp();
            case TextFollowPathModifierBase::radialPropertyKey:
                return object->as<TextFollowPathModifierBase>()->radial();
            case TextFollowPathModifierBase::orientPropertyKey:
                return object->as<TextFollowPathModifierBase>()->orient();
            case TextInputBase::multilinePropertyKey:
                return object->as<TextInputBase>()->multiline();
            case TextInputBase::obscuredPropertyKey:
                return object->as<TextInputBase>()->obscured();
            case TextInputBase::selectAllOnFocusPropertyKey:
                return object->as<TextInputBase>()->selectAllOnFocus();
            case TextBase::fitFromBaselinePropertyKey:
                return object->as<TextBase>()->fitFromBaseline();
            case TextBase::fitFontSizeResizesBoxPropertyKey:
                return object->as<TextBase>()->fitFontSizeResizesBox();
            case ScriptAssetBase::isModulePropertyKey:
                return object->as<ScriptAssetBase>()->isModule();
            case BitmapCacheBase::cacheEnabledPropertyKey:
                return object->as<BitmapCacheBase>()->cacheEnabled();
            case BitmapCacheBase::ditherPropertyKey:
                return object->as<BitmapCacheBase>()->dither();
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
            case LayerMaskBase::resolutionPropertyKey:
                return object->as<LayerMaskBase>()->resolution();
            case LayerMaskBase::boundsXPropertyKey:
                return object->as<LayerMaskBase>()->boundsX();
            case LayerMaskBase::boundsYPropertyKey:
                return object->as<LayerMaskBase>()->boundsY();
            case LayerMaskBase::boundsWidthPropertyKey:
                return object->as<LayerMaskBase>()->boundsWidth();
            case LayerMaskBase::boundsHeightPropertyKey:
                return object->as<LayerMaskBase>()->boundsHeight();
            case CustomPropertyNumberBase::propertyValuePropertyKey:
                return object->as<CustomPropertyNumberBase>()->propertyValue();
            case ConstraintBase::strengthPropertyKey:
                return object->as<ConstraintBase>()->strength();
            case DistanceConstraintBase::distancePropertyKey:
                return object->as<DistanceConstraintBase>()->distance();
            case FollowPathConstraintBase::distancePropertyKey:
                return object->as<FollowPathConstraintBase>()->distance();
            case ListFollowPathConstraintBase::distanceEndPropertyKey:
                return object->as<ListFollowPathConstraintBase>()
                    ->distanceEnd();
            case ListFollowPathConstraintBase::distanceOffsetPropertyKey:
                return object->as<ListFollowPathConstraintBase>()
                    ->distanceOffset();
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
            case ScrollConstraintBase::thresholdPropertyKey:
                return object->as<ScrollConstraintBase>()->threshold();
            case ScrollConstraintBase::velocityXPropertyKey:
                return object->as<ScrollConstraintBase>()->velocityX();
            case ScrollConstraintBase::velocityYPropertyKey:
                return object->as<ScrollConstraintBase>()->velocityY();
            case ScrollConstraintBase::dragMultiplierPropertyKey:
                return object->as<ScrollConstraintBase>()->dragMultiplier();
            case ScrollConstraintBase::computedContentWidthPropertyKey:
                return object->as<ScrollConstraintBase>()
                    ->computedContentWidth();
            case ScrollConstraintBase::computedContentHeightPropertyKey:
                return object->as<ScrollConstraintBase>()
                    ->computedContentHeight();
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
            case NestedArtboardBase::speedPropertyKey:
                return object->as<NestedArtboardBase>()->speed();
            case NestedArtboardBase::quantizePropertyKey:
                return object->as<NestedArtboardBase>()->quantize();
            case NestedArtboardLayoutBase::instanceWidthPropertyKey:
                return object->as<NestedArtboardLayoutBase>()->instanceWidth();
            case NestedArtboardLayoutBase::instanceHeightPropertyKey:
                return object->as<NestedArtboardLayoutBase>()->instanceHeight();
            case GridTrackBase::trackValuePropertyKey:
                return object->as<GridTrackBase>()->trackValue();
            case GridTrackBase::trackMaxValuePropertyKey:
                return object->as<GridTrackBase>()->trackMaxValue();
            case LayoutSizingStyleBase::minWidthPropertyKey:
                return object->as<LayoutSizingStyleBase>()->minWidth();
            case LayoutSizingStyleBase::maxWidthPropertyKey:
                return object->as<LayoutSizingStyleBase>()->maxWidth();
            case LayoutSizingStyleBase::minHeightPropertyKey:
                return object->as<LayoutSizingStyleBase>()->minHeight();
            case LayoutSizingStyleBase::maxHeightPropertyKey:
                return object->as<LayoutSizingStyleBase>()->maxHeight();
            case LayoutNodeStyleBase::widthPropertyKey:
                return object->as<LayoutNodeStyleBase>()->width();
            case LayoutNodeStyleBase::heightPropertyKey:
                return object->as<LayoutNodeStyleBase>()->height();
            case LayoutNodeStyleBase::fractionalWidthPropertyKey:
                return object->as<LayoutNodeStyleBase>()->fractionalWidth();
            case LayoutNodeStyleBase::fractionalHeightPropertyKey:
                return object->as<LayoutNodeStyleBase>()->fractionalHeight();
            case AxisBase::offsetPropertyKey:
                return object->as<AxisBase>()->offset();
            case LayoutComponentStyleBase::gapHorizontalPropertyKey:
                return object->as<LayoutComponentStyleBase>()->gapHorizontal();
            case LayoutComponentStyleBase::gapVerticalPropertyKey:
                return object->as<LayoutComponentStyleBase>()->gapVertical();
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
            case ComponentOriginBase::originXPropertyKey:
                return object->as<ComponentOriginBase>()->originX();
            case ComponentOriginBase::originYPropertyKey:
                return object->as<ComponentOriginBase>()->originY();
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
            case PaintImageBase::imageScaleXPropertyKey:
                return object->as<PaintImageBase>()->imageScaleX();
            case PaintImageBase::imageScaleYPropertyKey:
                return object->as<PaintImageBase>()->imageScaleY();
            case PaintImageBase::imageOffsetXPropertyKey:
                return object->as<PaintImageBase>()->imageOffsetX();
            case PaintImageBase::imageOffsetYPropertyKey:
                return object->as<PaintImageBase>()->imageOffsetY();
            case PaintImageBase::imageRotationPropertyKey:
                return object->as<PaintImageBase>()->imageRotation();
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
            case ImageBase::alignmentXPropertyKey:
                return object->as<ImageBase>()->alignmentX();
            case ImageBase::alignmentYPropertyKey:
                return object->as<ImageBase>()->alignmentY();
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
            case SelectionStyleBase::cornerRadiusPropertyKey:
                return object->as<SelectionStyleBase>()->cornerRadius();
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
            case TextStyleBackgroundBase::cornerRadiusPropertyKey:
                return object->as<TextStyleBackgroundBase>()->cornerRadius();
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
            case ExportAudioBase::volumePropertyKey:
                return object->as<ExportAudioBase>()->volume();
            case DrawableAssetBase::heightPropertyKey:
                return object->as<DrawableAssetBase>()->height();
            case DrawableAssetBase::widthPropertyKey:
                return object->as<DrawableAssetBase>()->width();
            case BitmapCacheBase::resolutionPropertyKey:
                return object->as<BitmapCacheBase>()->resolution();
        }
        return 0.0f;
    }
    static int32_t getInt(Core* object, int propertyKey)
    {
        switch (propertyKey)
        {
            case GridItemPlacementBase::gridColumnPropertyKey:
                return object->as<GridItemPlacementBase>()->gridColumn();
            case GridItemPlacementBase::gridRowPropertyKey:
                return object->as<GridItemPlacementBase>()->gridRow();
            case KeyFrameIntBase::valuePropertyKey:
                return object->as<KeyFrameIntBase>()->value();
        }
        return 0;
    }
    static int propertyFieldId(int propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceListItemBase::viewModelIdPropertyKey:
            case ViewModelInstanceListItemBase::viewModelInstanceIdPropertyKey:
            case ComponentBase::parentIdPropertyKey:
            case ViewModelInstanceValueBase::viewModelPropertyIdPropertyKey:
            case ViewModelPropertyEnumCustomBase::enumIdPropertyKey:
            case ViewModelInstanceEnumBase::propertyValuePropertyKey:
            case ViewModelInstanceAssetBase::propertyValuePropertyKey:
            case ViewModelInstanceArtboardBase::propertyValuePropertyKey:
            case ViewModelPropertyViewModelBase::
                viewModelReferenceIdPropertyKey:
            case ViewModelInstanceBase::viewModelIdPropertyKey:
            case ViewModelInstanceListBase::listSourcePropertyKey:
            case ViewModelInstanceViewModelBase::propertyValuePropertyKey:
            case DrawTargetBase::drawableIdPropertyKey:
            case LayerMaskBase::sourceIdPropertyKey:
            case TargetedConstraintBase::targetIdPropertyKey:
            case ScrollPhysicsBase::constraintIdPropertyKey:
            case ScrollConstraintBase::physicsIdPropertyKey:
            case ScrollBarConstraintBase::scrollConstraintIdPropertyKey:
            case NestedArtboardBase::artboardIdPropertyKey:
            case ArtboardComponentListBase::listSourcePropertyKey:
            case NestedAnimationBase::animationIdPropertyKey:
            case SoloBase::activeComponentIdPropertyKey:
            case ScriptedDrawableBase::scriptAssetIdPropertyKey:
            case ScriptedDataConverterBase::scriptAssetIdPropertyKey:
            case ScriptedTransitionBase::activeComponentIdPropertyKey:
            case ScriptedTransitionBase::listSourcePropertyKey:
            case ScriptedInterpolatorBase::scriptAssetIdPropertyKey:
            case ScriptedPathEffectBase::scriptAssetIdPropertyKey:
            case LayoutComponentStyleBase::interpolatorIdPropertyKey:
            case ArtboardComponentListOverrideBase::artboardIdPropertyKey:
            case ListenerFireEventBase::eventIdPropertyKey:
            case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
            case ListenerInputChangeBase::inputIdPropertyKey:
            case ListenerInputChangeBase::nestedInputIdPropertyKey:
            case AnimationStateBase::animationIdPropertyKey:
            case NestedInputBase::inputIdPropertyKey:
            case ScriptedListenerActionBase::scriptAssetIdPropertyKey:
            case KeyedObjectBase::objectIdPropertyKey:
            case BlendAnimationBase::animationIdPropertyKey:
            case BlendAnimationDirectBase::inputIdPropertyKey:
            case StateMachineListenerBase::targetIdPropertyKey:
            case StateMachineListenerSingleBase::eventIdPropertyKey:
            case TransitionInputConditionBase::inputIdPropertyKey:
            case KeyFrameIdBase::valuePropertyKey:
            case ListenerAlignTargetBase::targetIdPropertyKey:
            case ScriptedTransitionConditionBase::scriptAssetIdPropertyKey:
            case BlendState1DInputBase::inputIdPropertyKey:
            case FocusActionTargetBase::targetIdPropertyKey:
            case TransitionValueIdComparatorBase::valuePropertyKey:
            case StateTransitionBase::stateToIdPropertyKey:
            case StateTransitionBase::interpolatorIdPropertyKey:
            case StateMachineFireEventBase::eventIdPropertyKey:
            case TransitionPropertyComponentComparatorBase::objectIdPropertyKey:
            case ListenerInputTypeEventBase::eventIdPropertyKey:
            case BlendStateTransitionBase::exitBlendAnimationIdPropertyKey:
            case TargetEffectBase::targetIdPropertyKey:
            case PaintImageBase::imageAssetIdPropertyKey:
            case ListPathBase::listSourcePropertyKey:
            case ClippingShapeBase::sourceIdPropertyKey:
            case ImageBase::assetIdPropertyKey:
            case DrawRulesBase::drawTargetIdPropertyKey:
            case LayoutComponentBase::styleIdPropertyKey:
            case ArtboardBase::defaultStateMachineIdPropertyKey:
            case ArtboardBase::viewModelIdPropertyKey:
            case JoystickBase::xIdPropertyKey:
            case JoystickBase::yIdPropertyKey:
            case JoystickBase::handleSourceIdPropertyKey:
            case BindablePropertyIdBase::propertyValuePropertyKey:
            case DataBindBase::converterIdPropertyKey:
            case DataConverterNumberToListBase::viewModelIdPropertyKey:
            case DataConverterRangeMapperBase::interpolatorIdPropertyKey:
            case DataConverterInterpolatorBase::interpolatorIdPropertyKey:
            case DataConverterGroupItemBase::converterIdPropertyKey:
            case BindablePropertyListBase::propertyValuePropertyKey:
            case BindablePropertyEnumBase::propertyValuePropertyKey:
            case TendonBase::boneIdPropertyKey:
            case TextModifierRangeBase::runIdPropertyKey:
            case TextTargetModifierBase::targetIdPropertyKey:
            case TextStyleBase::fontAssetIdPropertyKey:
            case TextBase::textRunListSourcePropertyKey:
            case TextValueRunBase::styleIdPropertyKey:
            case ArtboardListMapRuleBase::artboardIdPropertyKey:
            case ArtboardListMapRuleBase::viewModelIdPropertyKey:
            case CustomPropertyEnumBase::propertyValuePropertyKey:
            case CustomPropertyEnumBase::enumIdPropertyKey:
            case AudioEventBase::assetIdPropertyKey:
            case ScriptInputArtboardBase::artboardIdPropertyKey:
                return CoreIdType::id;
            case ViewModelComponentBase::namePropertyKey:
            case ComponentBase::namePropertyKey:
            case DataEnumCustomBase::namePropertyKey:
            case ViewModelInstanceStringBase::propertyValuePropertyKey:
            case DataEnumValueBase::keyPropertyKey:
            case DataEnumValueBase::valuePropertyKey:
            case AssetBase::namePropertyKey:
            case DataConverterBase::namePropertyKey:
            case AnimationBase::namePropertyKey:
            case StateMachineComponentBase::namePropertyKey:
            case KeyFrameStringBase::valuePropertyKey:
            case TransitionValueStringComparatorBase::valuePropertyKey:
            case OpenUrlEventBase::urlPropertyKey:
            case SemanticDataBase::labelPropertyKey:
            case SemanticDataBase::valuePropertyKey:
            case SemanticDataBase::hintPropertyKey:
            case CustomPropertyStringBase::propertyValuePropertyKey:
            case DataConverterStringPadBase::textPropertyKey:
            case DataConverterToStringBase::colorFormatPropertyKey:
            case BindablePropertyStringBase::propertyValuePropertyKey:
            case TextInputBase::textPropertyKey:
            case TextValueRunBase::textPropertyKey:
            case FileAssetBase::cdnBaseUrlPropertyKey:
            case TextAssetBase::folderPathPropertyKey:
                return CoreStringType::id;
            case ViewModelPropertyBase::symbolTypeValuePropertyKey:
            case ViewModelPropertyBase::componentPropsPropertyKey:
            case ViewModelPropertyEnumSystemBase::enumTypePropertyKey:
            case ViewModelBase::viewModelTypePropertyKey:
            case DataEnumSystemBase::enumTypePropertyKey:
            case ViewModelInstanceTriggerBase::propertyValuePropertyKey:
            case ViewModelInstanceSymbolListIndexBase::propertyValuePropertyKey:
            case CustomPropertyBase::nameIdPropertyKey:
            case CustomPropertyTriggerBase::propertyValuePropertyKey:
            case DrawTargetBase::placementValuePropertyKey:
            case LayerMaskBase::maskFlagsPropertyKey:
            case LayerMaskBase::maskModeValuePropertyKey:
            case DistanceConstraintBase::modeValuePropertyKey:
            case TransformSpaceConstraintBase::sourceSpaceValuePropertyKey:
            case TransformSpaceConstraintBase::destSpaceValuePropertyKey:
            case TransformComponentConstraintBase::minMaxSpaceValuePropertyKey:
            case IKConstraintBase::parentBoneCountPropertyKey:
            case DraggableConstraintBase::directionValuePropertyKey:
            case ScrollConstraintBase::physicsTypeValuePropertyKey:
            case ScrollConstraintBase::virtualizeBufferPropertyKey:
            case ScrollConstraintBase::scrollFlagsPropertyKey:
            case DrawableBase::blendModeValuePropertyKey:
            case DrawableBase::additiveAmountPropertyKey:
            case DrawableBase::drawableFlagsPropertyKey:
            case NestedArtboardLayoutBase::instanceWidthUnitsValuePropertyKey:
            case NestedArtboardLayoutBase::instanceHeightUnitsValuePropertyKey:
            case NestedArtboardLayoutBase::instanceWidthScaleTypePropertyKey:
            case NestedArtboardLayoutBase::instanceHeightScaleTypePropertyKey:
            case NSlicerTileModeBase::patchIndexPropertyKey:
            case NSlicerTileModeBase::stylePropertyKey:
            case GridTrackBase::collectionPropertyKey:
            case GridTrackBase::trackTypePropertyKey:
            case GridTrackBase::trackMaxTypePropertyKey:
            case GridItemPlacementBase::gridColumnSpanPropertyKey:
            case GridItemPlacementBase::gridRowSpanPropertyKey:
            case LayoutSizingStyleBase::minWidthUnitsValuePropertyKey:
            case LayoutSizingStyleBase::maxWidthUnitsValuePropertyKey:
            case LayoutSizingStyleBase::minHeightUnitsValuePropertyKey:
            case LayoutSizingStyleBase::maxHeightUnitsValuePropertyKey:
            case LayoutSizingStyleBase::layoutWidthScaleTypePropertyKey:
            case LayoutSizingStyleBase::layoutHeightScaleTypePropertyKey:
            case LayoutSizingStyleBase::widthUnitsValuePropertyKey:
            case LayoutSizingStyleBase::heightUnitsValuePropertyKey:
            case LayoutSizingStyleBase::justifySelfValuePropertyKey:
            case LayoutSizingStyleBase::displayValuePropertyKey:
            case LayoutComponentStyleBase::positionLeftUnitsValuePropertyKey:
            case LayoutComponentStyleBase::positionRightUnitsValuePropertyKey:
            case LayoutComponentStyleBase::positionTopUnitsValuePropertyKey:
            case LayoutComponentStyleBase::positionBottomUnitsValuePropertyKey:
            case LayoutComponentStyleBase::flexBasisUnitsValuePropertyKey:
            case LayoutComponentStyleBase::layoutAlignmentTypePropertyKey:
            case LayoutComponentStyleBase::animationStyleTypePropertyKey:
            case LayoutComponentStyleBase::interpolationTypePropertyKey:
            case LayoutComponentStyleBase::positionTypeValuePropertyKey:
            case LayoutComponentStyleBase::flexDirectionValuePropertyKey:
            case LayoutComponentStyleBase::directionValuePropertyKey:
            case LayoutComponentStyleBase::flexWrapValuePropertyKey:
            case LayoutComponentStyleBase::overflowValuePropertyKey:
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
            case LayoutComponentStyleBase::gapHorizontalUnitsValuePropertyKey:
            case LayoutComponentStyleBase::gapVerticalUnitsValuePropertyKey:
            case LayoutComponentStyleBase::justifyItemsValuePropertyKey:
            case LayoutComponentStyleBase::layoutTypeValuePropertyKey:
            case ArtboardComponentListOverrideBase::
                instanceWidthUnitsValuePropertyKey:
            case ArtboardComponentListOverrideBase::
                instanceHeightUnitsValuePropertyKey:
            case ArtboardComponentListOverrideBase::
                instanceWidthScaleTypePropertyKey:
            case ArtboardComponentListOverrideBase::
                instanceHeightScaleTypePropertyKey:
            case ListenerActionBase::flagsPropertyKey:
            case LayerStateBase::flagsPropertyKey:
            case StateMachineFireActionBase::occursValuePropertyKey:
            case TransitionValueTriggerComparatorBase::valuePropertyKey:
            case KeyFrameBase::framePropertyKey:
            case InterpolatingKeyFrameBase::interpolationTypePropertyKey:
            case KeyFrameUintBase::valuePropertyKey:
            case BlendAnimationDirectBase::blendSourcePropertyKey:
            case StateMachineListenerSingleBase::listenerTypeValuePropertyKey:
            case KeyedPropertyBase::propertyKeyPropertyKey:
            case TransitionPropertyArtboardComparatorBase::
                propertyTypePropertyKey:
            case ListenerBoolChangeBase::valuePropertyKey:
            case TransitionViewModelConditionBase::opValuePropertyKey:
            case TransitionValueConditionBase::opValuePropertyKey:
            case StateTransitionBase::flagsPropertyKey:
            case StateTransitionBase::durationPropertyKey:
            case StateTransitionBase::exitTimePropertyKey:
            case StateTransitionBase::interpolationTypePropertyKey:
            case StateTransitionBase::randomWeightPropertyKey:
            case FocusActionTraversalBase::traversalKindPropertyKey:
            case LinearAnimationBase::fpsPropertyKey:
            case LinearAnimationBase::durationPropertyKey:
            case LinearAnimationBase::loopValuePropertyKey:
            case LinearAnimationBase::workStartPropertyKey:
            case LinearAnimationBase::workEndPropertyKey:
            case ListenerViewModelChangeBase::inputValuePropertyKey:
            case ListenerViewModelChangeBase::inputValueIndexPropertyKey:
            case TransitionPropertyComponentComparatorBase::
                propertyKeyPropertyKey:
            case ElasticInterpolatorBase::easingValuePropertyKey:
            case ListenerInputTypeBase::listenerTypeValuePropertyKey:
            case ListenerInputTypePointerButtonBase::
                pointerButtonValuePropertyKey:
            case ShapePaintBase::blendModeValuePropertyKey:
            case ShapePaintBase::additiveAmountPropertyKey:
            case ColorChannelsBase::colorRedPropertyKey:
            case ColorChannelsBase::colorGreenPropertyKey:
            case ColorChannelsBase::colorBluePropertyKey:
            case ColorChannelsBase::colorAlphaPropertyKey:
            case StrokeBase::capPropertyKey:
            case StrokeBase::joinPropertyKey:
            case StrokeBase::positionPropertyKey:
            case PaintImageBase::imageSamplerFilterPropertyKey:
            case PaintImageBase::imageSamplerWrapXPropertyKey:
            case PaintImageBase::imageSamplerWrapYPropertyKey:
            case PaintImageBase::imageSizeModePropertyKey:
            case FeatherBase::spaceValuePropertyKey:
            case TrimPathBase::modeValuePropertyKey:
            case FillBase::fillRulePropertyKey:
            case PathBase::pathFlagsPropertyKey:
            case ClippingShapeBase::fillRulePropertyKey:
            case PolygonBase::pointsPropertyKey:
            case ImageBase::fitPropertyKey:
            case ImageBase::samplerFilterPropertyKey:
            case ImageBase::samplerWrapXPropertyKey:
            case ImageBase::samplerWrapYPropertyKey:
            case FocusDataBase::focusFlagsPropertyKey:
            case FocusDataBase::edgeBehaviorValuePropertyKey:
            case JoystickBase::joystickFlagsPropertyKey:
            case OpenUrlEventBase::targetValuePropertyKey:
            case SemanticDataBase::rolePropertyKey:
            case SemanticDataBase::headingLevelPropertyKey:
            case SemanticDataBase::traitFlagsPropertyKey:
            case SemanticDataBase::stateFlagsPropertyKey:
            case SemanticDataBase::isCheckedPropertyKey:
            case BindablePropertyIntegerBase::propertyValuePropertyKey:
            case DataBindBase::propertyKeyPropertyKey:
            case DataBindBase::flagsPropertyKey:
            case DataConverterFormulaBase::randomModeValuePropertyKey:
            case DataConverterOperationBase::operationTypePropertyKey:
            case DataConverterRangeMapperBase::interpolationTypePropertyKey:
            case DataConverterRangeMapperBase::flagsPropertyKey:
            case DataConverterInterpolatorBase::interpolationTypePropertyKey:
            case DataConverterRounderBase::decimalsPropertyKey:
            case DataConverterStringPadBase::lengthPropertyKey:
            case DataConverterStringPadBase::padTypePropertyKey:
            case DataConverterStringTrimBase::trimTypePropertyKey:
            case FormulaTokenOperationBase::operationTypePropertyKey:
            case FormulaTokenFunctionBase::functionTypePropertyKey:
            case DataConverterToStringBase::flagsPropertyKey:
            case DataConverterToStringBase::decimalsPropertyKey:
            case NestedArtboardLeafBase::fitPropertyKey:
            case WeightBase::valuesPropertyKey:
            case WeightBase::indicesPropertyKey:
            case CubicWeightBase::inValuesPropertyKey:
            case CubicWeightBase::inIndicesPropertyKey:
            case CubicWeightBase::outValuesPropertyKey:
            case CubicWeightBase::outIndicesPropertyKey:
            case TextModifierRangeBase::unitsValuePropertyKey:
            case TextModifierRangeBase::typeValuePropertyKey:
            case TextModifierRangeBase::modeValuePropertyKey:
            case TextStyleFeatureBase::tagPropertyKey:
            case TextStyleFeatureBase::featureValuePropertyKey:
            case TextVariationModifierBase::axisTagPropertyKey:
            case TextModifierGroupBase::modifierFlagsPropertyKey:
            case TextInputBase::alignValuePropertyKey:
            case TextInputBase::verticalAlignValuePropertyKey:
            case TextStyleAxisBase::tagPropertyKey:
            case TextBase::alignValuePropertyKey:
            case TextBase::sizingValuePropertyKey:
            case TextBase::overflowValuePropertyKey:
            case TextBase::originValuePropertyKey:
            case TextBase::wrapValuePropertyKey:
            case TextBase::wordBreakValuePropertyKey:
            case TextBase::verticalAlignValuePropertyKey:
            case TextBase::verticalTrimValuePropertyKey:
            case TextBase::verticalTrimTopValuePropertyKey:
            case TextBase::verticalTrimBottomValuePropertyKey:
            case FileAssetBase::assetIdPropertyKey:
            case ScriptAssetBase::generatorFunctionRefPropertyKey:
            case ScriptAssetBase::serializedImplementedMethodsPropertyKey:
            case ImageAssetBase::samplerFilterPropertyKey:
            case ImageAssetBase::samplerWrapXPropertyKey:
            case ImageAssetBase::samplerWrapYPropertyKey:
            case ScriptModuleAssetBase::languagePropertyKey:
            case BitmapCacheBase::cacheFlagsPropertyKey:
            case GamepadInputBase::kindPropertyKey:
            case GamepadInputBase::mappingPropertyKey:
            case GamepadInputBase::inputIndexPropertyKey:
            case GamepadInputBase::buttonPhasePropertyKey:
            case KeyboardInputBase::keyTypePropertyKey:
            case KeyboardInputBase::keyPhasePropertyKey:
            case KeyboardInputBase::modifiersPropertyKey:
            case SemanticInputBase::actionTypePropertyKey:
                return CoreUintType::id;
            case ViewModelInstanceColorBase::propertyValuePropertyKey:
            case CustomPropertyColorBase::propertyValuePropertyKey:
            case KeyFrameColorBase::valuePropertyKey:
            case TransitionValueColorComparatorBase::valuePropertyKey:
            case SolidColorBase::colorValuePropertyKey:
            case GradientStopBase::colorValuePropertyKey:
            case SelectionStyleBase::highlightColorPropertyKey:
            case BindablePropertyColorBase::propertyValuePropertyKey:
                return CoreColorType::id;
            case ViewModelInstanceBooleanBase::propertyValuePropertyKey:
            case LayerMaskBase::isVisiblePropertyKey:
            case LayerMaskBase::sourceDrawsPropertyKey:
            case LayerMaskBase::useCustomBoundsPropertyKey:
            case FollowPathConstraintBase::orientPropertyKey:
            case FollowPathConstraintBase::offsetPropertyKey:
            case TransformComponentConstraintBase::offsetPropertyKey:
            case TransformComponentConstraintBase::doesCopyPropertyKey:
            case TransformComponentConstraintBase::minPropertyKey:
            case TransformComponentConstraintBase::maxPropertyKey:
            case TransformComponentConstraintYBase::doesCopyYPropertyKey:
            case TransformComponentConstraintYBase::minYPropertyKey:
            case TransformComponentConstraintYBase::maxYPropertyKey:
            case IKConstraintBase::invertDirectionPropertyKey:
            case ScrollConstraintBase::snapPropertyKey:
            case ScrollConstraintBase::virtualizePropertyKey:
            case ScrollConstraintBase::infinitePropertyKey:
            case ScrollConstraintBase::interactivePropertyKey:
            case ScrollConstraintBase::scrollActivePropertyKey:
            case ScrollConstraintBase::wheelInteractivePropertyKey:
            case ScrollBarConstraintBase::autoSizePropertyKey:
            case NestedArtboardBase::isPausedPropertyKey:
            case NestedArtboardBase::isStatefulPropertyKey:
            case LayoutSizingStyleBase::hugUnboundedPropertyKey:
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
            case FocusDataBase::canFocusPropertyKey:
            case FocusDataBase::canTouchPropertyKey:
            case FocusDataBase::canTraversePropertyKey:
            case CustomPropertyBooleanBase::propertyValuePropertyKey:
            case LayoutComponentBase::clipPropertyKey:
            case SemanticDataBase::isExpandablePropertyKey:
            case SemanticDataBase::isSelectablePropertyKey:
            case SemanticDataBase::isCheckablePropertyKey:
            case SemanticDataBase::isToggleablePropertyKey:
            case SemanticDataBase::isRequirablePropertyKey:
            case SemanticDataBase::isEnablablePropertyKey:
            case SemanticDataBase::isFocusablePropertyKey:
            case SemanticDataBase::isExpandedPropertyKey:
            case SemanticDataBase::isSelectedPropertyKey:
            case SemanticDataBase::isToggledPropertyKey:
            case SemanticDataBase::isRequiredPropertyKey:
            case SemanticDataBase::isDisabledPropertyKey:
            case SemanticDataBase::isFocusedPropertyKey:
            case SemanticDataBase::isHiddenPropertyKey:
            case SemanticDataBase::isLiveRegionPropertyKey:
            case SemanticDataBase::isReadOnlyPropertyKey:
            case SemanticDataBase::isModalPropertyKey:
            case SemanticDataBase::isObscuredPropertyKey:
            case SemanticDataBase::isMultilinePropertyKey:
            case DataBindPathBase::isRelativePropertyKey:
            case BindablePropertyBooleanBase::propertyValuePropertyKey:
            case NestedArtboardLeafBase::fitToLayoutParentPropertyKey:
            case TextModifierRangeBase::clampPropertyKey:
            case TextFollowPathModifierBase::radialPropertyKey:
            case TextFollowPathModifierBase::orientPropertyKey:
            case TextInputBase::multilinePropertyKey:
            case TextInputBase::obscuredPropertyKey:
            case TextInputBase::selectAllOnFocusPropertyKey:
            case TextBase::fitFromBaselinePropertyKey:
            case TextBase::fitFontSizeResizesBoxPropertyKey:
            case ScriptAssetBase::isModulePropertyKey:
            case BitmapCacheBase::cacheEnabledPropertyKey:
            case BitmapCacheBase::ditherPropertyKey:
                return CoreBoolType::id;
            case ViewModelInstanceNumberBase::propertyValuePropertyKey:
            case LayerMaskBase::resolutionPropertyKey:
            case LayerMaskBase::boundsXPropertyKey:
            case LayerMaskBase::boundsYPropertyKey:
            case LayerMaskBase::boundsWidthPropertyKey:
            case LayerMaskBase::boundsHeightPropertyKey:
            case CustomPropertyNumberBase::propertyValuePropertyKey:
            case ConstraintBase::strengthPropertyKey:
            case DistanceConstraintBase::distancePropertyKey:
            case FollowPathConstraintBase::distancePropertyKey:
            case ListFollowPathConstraintBase::distanceEndPropertyKey:
            case ListFollowPathConstraintBase::distanceOffsetPropertyKey:
            case TransformComponentConstraintBase::copyFactorPropertyKey:
            case TransformComponentConstraintBase::minValuePropertyKey:
            case TransformComponentConstraintBase::maxValuePropertyKey:
            case TransformComponentConstraintYBase::copyFactorYPropertyKey:
            case TransformComponentConstraintYBase::minValueYPropertyKey:
            case TransformComponentConstraintYBase::maxValueYPropertyKey:
            case ScrollConstraintBase::scrollOffsetXPropertyKey:
            case ScrollConstraintBase::scrollOffsetYPropertyKey:
            case ScrollConstraintBase::scrollPercentXPropertyKey:
            case ScrollConstraintBase::scrollPercentYPropertyKey:
            case ScrollConstraintBase::scrollIndexPropertyKey:
            case ScrollConstraintBase::thresholdPropertyKey:
            case ScrollConstraintBase::velocityXPropertyKey:
            case ScrollConstraintBase::velocityYPropertyKey:
            case ScrollConstraintBase::dragMultiplierPropertyKey:
            case ScrollConstraintBase::computedContentWidthPropertyKey:
            case ScrollConstraintBase::computedContentHeightPropertyKey:
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
            case NestedArtboardBase::speedPropertyKey:
            case NestedArtboardBase::quantizePropertyKey:
            case NestedArtboardLayoutBase::instanceWidthPropertyKey:
            case NestedArtboardLayoutBase::instanceHeightPropertyKey:
            case GridTrackBase::trackValuePropertyKey:
            case GridTrackBase::trackMaxValuePropertyKey:
            case LayoutSizingStyleBase::minWidthPropertyKey:
            case LayoutSizingStyleBase::maxWidthPropertyKey:
            case LayoutSizingStyleBase::minHeightPropertyKey:
            case LayoutSizingStyleBase::maxHeightPropertyKey:
            case LayoutNodeStyleBase::widthPropertyKey:
            case LayoutNodeStyleBase::heightPropertyKey:
            case LayoutNodeStyleBase::fractionalWidthPropertyKey:
            case LayoutNodeStyleBase::fractionalHeightPropertyKey:
            case AxisBase::offsetPropertyKey:
            case LayoutComponentStyleBase::gapHorizontalPropertyKey:
            case LayoutComponentStyleBase::gapVerticalPropertyKey:
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
            case ComponentOriginBase::originXPropertyKey:
            case ComponentOriginBase::originYPropertyKey:
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
            case PaintImageBase::imageScaleXPropertyKey:
            case PaintImageBase::imageScaleYPropertyKey:
            case PaintImageBase::imageOffsetXPropertyKey:
            case PaintImageBase::imageOffsetYPropertyKey:
            case PaintImageBase::imageRotationPropertyKey:
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
            case ImageBase::alignmentXPropertyKey:
            case ImageBase::alignmentYPropertyKey:
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
            case SelectionStyleBase::cornerRadiusPropertyKey:
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
            case TextStyleBackgroundBase::cornerRadiusPropertyKey:
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
            case ExportAudioBase::volumePropertyKey:
            case DrawableAssetBase::heightPropertyKey:
            case DrawableAssetBase::widthPropertyKey:
            case BitmapCacheBase::resolutionPropertyKey:
                return CoreDoubleType::id;
            case ScriptInputViewModelPropertyBase::dataBindPathIdsPropertyKey:
            case NestedArtboardBase::dataBindPathIdsPropertyKey:
            case StateMachineFireTriggerBase::viewModelPathIdsPropertyKey:
            case StateMachineListenerSingleBase::viewModelPathIdsPropertyKey:
            case ListenerInputTypeViewModelBase::viewModelPathIdsPropertyKey:
            case MeshBase::triangleIndexBytesPropertyKey:
            case DataBindPathBase::pathPropertyKey:
            case DataConverterOperationViewModelBase::sourcePathIdsPropertyKey:
            case DataBindContextBase::sourcePathIdsPropertyKey:
            case FileAssetBase::cdnUuidPropertyKey:
            case FileAssetContentsBase::bytesPropertyKey:
            case FileAssetContentsBase::signaturePropertyKey:
                return CoreBytesType::id;
            case GridItemPlacementBase::gridColumnPropertyKey:
            case GridItemPlacementBase::gridRowPropertyKey:
            case KeyFrameIntBase::valuePropertyKey:
                return CoreIntType::id;
            default:
                return -1;
        }
    }
    static bool isInterpolatableUint(uint32_t propertyKey)
    {
        switch (propertyKey)
        {
            case DrawableBase::additiveAmountPropertyKey:
            case ShapePaintBase::additiveAmountPropertyKey:
            case ColorChannelsBase::colorRedPropertyKey:
            case ColorChannelsBase::colorGreenPropertyKey:
            case ColorChannelsBase::colorBluePropertyKey:
            case ColorChannelsBase::colorAlphaPropertyKey:
                return true;
            default:
                return false;
        }
    }
    static bool isSignedInt(uint32_t propertyKey)
    {
        switch (propertyKey)
        {
            case GridItemPlacementBase::gridColumnPropertyKey:
            case GridItemPlacementBase::gridRowPropertyKey:
            case KeyFrameIntBase::valuePropertyKey:
                return true;
            default:
                return false;
        }
    }
    static bool isCallback(uint32_t propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceTriggerBase::firePropertyKey:
            case CustomPropertyTriggerBase::firePropertyKey:
            case NestedTriggerBase::firePropertyKey:
            case EventBase::triggerPropertyKey:
                return true;
            default:
                return false;
        }
    }
    static uint16_t propertyOwnerTypeKey(uint32_t propertyKey)
    {
        switch (propertyKey)
        {
            case ViewModelInstanceListItemBase::viewModelIdPropertyKey:
                return ViewModelInstanceListItemBase::typeKey;
            case ViewModelInstanceListItemBase::viewModelInstanceIdPropertyKey:
                return ViewModelInstanceListItemBase::typeKey;
            case ComponentBase::parentIdPropertyKey:
                return ComponentBase::typeKey;
            case ViewModelInstanceValueBase::viewModelPropertyIdPropertyKey:
                return ViewModelInstanceValueBase::typeKey;
            case ViewModelPropertyEnumCustomBase::enumIdPropertyKey:
                return ViewModelPropertyEnumCustomBase::typeKey;
            case ViewModelInstanceEnumBase::propertyValuePropertyKey:
                return ViewModelInstanceEnumBase::typeKey;
            case ViewModelInstanceAssetBase::propertyValuePropertyKey:
                return ViewModelInstanceAssetBase::typeKey;
            case ViewModelInstanceArtboardBase::propertyValuePropertyKey:
                return ViewModelInstanceArtboardBase::typeKey;
            case ViewModelPropertyViewModelBase::
                viewModelReferenceIdPropertyKey:
                return ViewModelPropertyViewModelBase::typeKey;
            case ViewModelInstanceBase::viewModelIdPropertyKey:
                return ViewModelInstanceBase::typeKey;
            case ViewModelInstanceListBase::listSourcePropertyKey:
                return ViewModelInstanceListBase::typeKey;
            case ViewModelInstanceViewModelBase::propertyValuePropertyKey:
                return ViewModelInstanceViewModelBase::typeKey;
            case DrawTargetBase::drawableIdPropertyKey:
                return DrawTargetBase::typeKey;
            case LayerMaskBase::sourceIdPropertyKey:
                return LayerMaskBase::typeKey;
            case TargetedConstraintBase::targetIdPropertyKey:
                return TargetedConstraintBase::typeKey;
            case ScrollPhysicsBase::constraintIdPropertyKey:
                return ScrollPhysicsBase::typeKey;
            case ScrollConstraintBase::physicsIdPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollBarConstraintBase::scrollConstraintIdPropertyKey:
                return ScrollBarConstraintBase::typeKey;
            case NestedArtboardBase::artboardIdPropertyKey:
                return NestedArtboardBase::typeKey;
            case ArtboardComponentListBase::listSourcePropertyKey:
                return ArtboardComponentListBase::typeKey;
            case NestedAnimationBase::animationIdPropertyKey:
                return NestedAnimationBase::typeKey;
            case SoloBase::activeComponentIdPropertyKey:
                return SoloBase::typeKey;
            case ScriptedDrawableBase::scriptAssetIdPropertyKey:
                return ScriptedDrawableBase::typeKey;
            case ScriptedDataConverterBase::scriptAssetIdPropertyKey:
                return ScriptedDataConverterBase::typeKey;
            case ScriptedTransitionBase::activeComponentIdPropertyKey:
                return ScriptedTransitionBase::typeKey;
            case ScriptedTransitionBase::listSourcePropertyKey:
                return ScriptedTransitionBase::typeKey;
            case ScriptedInterpolatorBase::scriptAssetIdPropertyKey:
                return ScriptedInterpolatorBase::typeKey;
            case ScriptedPathEffectBase::scriptAssetIdPropertyKey:
                return ScriptedPathEffectBase::typeKey;
            case LayoutComponentStyleBase::interpolatorIdPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case ArtboardComponentListOverrideBase::artboardIdPropertyKey:
                return ArtboardComponentListOverrideBase::typeKey;
            case ListenerFireEventBase::eventIdPropertyKey:
                return ListenerFireEventBase::typeKey;
            case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
                return InterpolatingKeyFrameBase::typeKey;
            case ListenerInputChangeBase::inputIdPropertyKey:
                return ListenerInputChangeBase::typeKey;
            case ListenerInputChangeBase::nestedInputIdPropertyKey:
                return ListenerInputChangeBase::typeKey;
            case AnimationStateBase::animationIdPropertyKey:
                return AnimationStateBase::typeKey;
            case NestedInputBase::inputIdPropertyKey:
                return NestedInputBase::typeKey;
            case ScriptedListenerActionBase::scriptAssetIdPropertyKey:
                return ScriptedListenerActionBase::typeKey;
            case KeyedObjectBase::objectIdPropertyKey:
                return KeyedObjectBase::typeKey;
            case BlendAnimationBase::animationIdPropertyKey:
                return BlendAnimationBase::typeKey;
            case BlendAnimationDirectBase::inputIdPropertyKey:
                return BlendAnimationDirectBase::typeKey;
            case StateMachineListenerBase::targetIdPropertyKey:
                return StateMachineListenerBase::typeKey;
            case StateMachineListenerSingleBase::eventIdPropertyKey:
                return StateMachineListenerSingleBase::typeKey;
            case TransitionInputConditionBase::inputIdPropertyKey:
                return TransitionInputConditionBase::typeKey;
            case KeyFrameIdBase::valuePropertyKey:
                return KeyFrameIdBase::typeKey;
            case ListenerAlignTargetBase::targetIdPropertyKey:
                return ListenerAlignTargetBase::typeKey;
            case ScriptedTransitionConditionBase::scriptAssetIdPropertyKey:
                return ScriptedTransitionConditionBase::typeKey;
            case BlendState1DInputBase::inputIdPropertyKey:
                return BlendState1DInputBase::typeKey;
            case FocusActionTargetBase::targetIdPropertyKey:
                return FocusActionTargetBase::typeKey;
            case TransitionValueIdComparatorBase::valuePropertyKey:
                return TransitionValueIdComparatorBase::typeKey;
            case StateTransitionBase::stateToIdPropertyKey:
                return StateTransitionBase::typeKey;
            case StateTransitionBase::interpolatorIdPropertyKey:
                return StateTransitionBase::typeKey;
            case StateMachineFireEventBase::eventIdPropertyKey:
                return StateMachineFireEventBase::typeKey;
            case TransitionPropertyComponentComparatorBase::objectIdPropertyKey:
                return TransitionPropertyComponentComparatorBase::typeKey;
            case ListenerInputTypeEventBase::eventIdPropertyKey:
                return ListenerInputTypeEventBase::typeKey;
            case BlendStateTransitionBase::exitBlendAnimationIdPropertyKey:
                return BlendStateTransitionBase::typeKey;
            case TargetEffectBase::targetIdPropertyKey:
                return TargetEffectBase::typeKey;
            case PaintImageBase::imageAssetIdPropertyKey:
                return PaintImageBase::typeKey;
            case ListPathBase::listSourcePropertyKey:
                return ListPathBase::typeKey;
            case ClippingShapeBase::sourceIdPropertyKey:
                return ClippingShapeBase::typeKey;
            case ImageBase::assetIdPropertyKey:
                return ImageBase::typeKey;
            case DrawRulesBase::drawTargetIdPropertyKey:
                return DrawRulesBase::typeKey;
            case LayoutComponentBase::styleIdPropertyKey:
                return LayoutComponentBase::typeKey;
            case ArtboardBase::defaultStateMachineIdPropertyKey:
                return ArtboardBase::typeKey;
            case ArtboardBase::viewModelIdPropertyKey:
                return ArtboardBase::typeKey;
            case JoystickBase::xIdPropertyKey:
                return JoystickBase::typeKey;
            case JoystickBase::yIdPropertyKey:
                return JoystickBase::typeKey;
            case JoystickBase::handleSourceIdPropertyKey:
                return JoystickBase::typeKey;
            case BindablePropertyIdBase::propertyValuePropertyKey:
                return BindablePropertyIdBase::typeKey;
            case DataBindBase::converterIdPropertyKey:
                return DataBindBase::typeKey;
            case DataConverterNumberToListBase::viewModelIdPropertyKey:
                return DataConverterNumberToListBase::typeKey;
            case DataConverterRangeMapperBase::interpolatorIdPropertyKey:
                return DataConverterRangeMapperBase::typeKey;
            case DataConverterInterpolatorBase::interpolatorIdPropertyKey:
                return DataConverterInterpolatorBase::typeKey;
            case DataConverterGroupItemBase::converterIdPropertyKey:
                return DataConverterGroupItemBase::typeKey;
            case BindablePropertyListBase::propertyValuePropertyKey:
                return BindablePropertyListBase::typeKey;
            case BindablePropertyEnumBase::propertyValuePropertyKey:
                return BindablePropertyEnumBase::typeKey;
            case TendonBase::boneIdPropertyKey:
                return TendonBase::typeKey;
            case TextModifierRangeBase::runIdPropertyKey:
                return TextModifierRangeBase::typeKey;
            case TextTargetModifierBase::targetIdPropertyKey:
                return TextTargetModifierBase::typeKey;
            case TextStyleBase::fontAssetIdPropertyKey:
                return TextStyleBase::typeKey;
            case TextBase::textRunListSourcePropertyKey:
                return TextBase::typeKey;
            case TextValueRunBase::styleIdPropertyKey:
                return TextValueRunBase::typeKey;
            case ArtboardListMapRuleBase::artboardIdPropertyKey:
                return ArtboardListMapRuleBase::typeKey;
            case ArtboardListMapRuleBase::viewModelIdPropertyKey:
                return ArtboardListMapRuleBase::typeKey;
            case CustomPropertyEnumBase::propertyValuePropertyKey:
                return CustomPropertyEnumBase::typeKey;
            case CustomPropertyEnumBase::enumIdPropertyKey:
                return CustomPropertyEnumBase::typeKey;
            case AudioEventBase::assetIdPropertyKey:
                return AudioEventBase::typeKey;
            case ScriptInputArtboardBase::artboardIdPropertyKey:
                return ScriptInputArtboardBase::typeKey;
            case ViewModelComponentBase::namePropertyKey:
                return ViewModelComponentBase::typeKey;
            case ComponentBase::namePropertyKey:
                return ComponentBase::typeKey;
            case DataEnumCustomBase::namePropertyKey:
                return DataEnumCustomBase::typeKey;
            case ViewModelInstanceStringBase::propertyValuePropertyKey:
                return ViewModelInstanceStringBase::typeKey;
            case DataEnumValueBase::keyPropertyKey:
                return DataEnumValueBase::typeKey;
            case DataEnumValueBase::valuePropertyKey:
                return DataEnumValueBase::typeKey;
            case AssetBase::namePropertyKey:
                return AssetBase::typeKey;
            case DataConverterBase::namePropertyKey:
                return DataConverterBase::typeKey;
            case AnimationBase::namePropertyKey:
                return AnimationBase::typeKey;
            case StateMachineComponentBase::namePropertyKey:
                return StateMachineComponentBase::typeKey;
            case KeyFrameStringBase::valuePropertyKey:
                return KeyFrameStringBase::typeKey;
            case TransitionValueStringComparatorBase::valuePropertyKey:
                return TransitionValueStringComparatorBase::typeKey;
            case OpenUrlEventBase::urlPropertyKey:
                return OpenUrlEventBase::typeKey;
            case SemanticDataBase::labelPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::valuePropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::hintPropertyKey:
                return SemanticDataBase::typeKey;
            case CustomPropertyStringBase::propertyValuePropertyKey:
                return CustomPropertyStringBase::typeKey;
            case DataConverterStringPadBase::textPropertyKey:
                return DataConverterStringPadBase::typeKey;
            case DataConverterToStringBase::colorFormatPropertyKey:
                return DataConverterToStringBase::typeKey;
            case BindablePropertyStringBase::propertyValuePropertyKey:
                return BindablePropertyStringBase::typeKey;
            case TextInputBase::textPropertyKey:
                return TextInputBase::typeKey;
            case TextValueRunBase::textPropertyKey:
                return TextValueRunBase::typeKey;
            case FileAssetBase::cdnBaseUrlPropertyKey:
                return FileAssetBase::typeKey;
            case TextAssetBase::folderPathPropertyKey:
                return TextAssetBase::typeKey;
            case ViewModelPropertyBase::symbolTypeValuePropertyKey:
                return ViewModelPropertyBase::typeKey;
            case ViewModelPropertyBase::componentPropsPropertyKey:
                return ViewModelPropertyBase::typeKey;
            case ViewModelPropertyEnumSystemBase::enumTypePropertyKey:
                return ViewModelPropertyEnumSystemBase::typeKey;
            case ViewModelBase::viewModelTypePropertyKey:
                return ViewModelBase::typeKey;
            case DataEnumSystemBase::enumTypePropertyKey:
                return DataEnumSystemBase::typeKey;
            case ViewModelInstanceTriggerBase::propertyValuePropertyKey:
                return ViewModelInstanceTriggerBase::typeKey;
            case ViewModelInstanceSymbolListIndexBase::propertyValuePropertyKey:
                return ViewModelInstanceSymbolListIndexBase::typeKey;
            case CustomPropertyBase::nameIdPropertyKey:
                return CustomPropertyBase::typeKey;
            case CustomPropertyTriggerBase::propertyValuePropertyKey:
                return CustomPropertyTriggerBase::typeKey;
            case DrawTargetBase::placementValuePropertyKey:
                return DrawTargetBase::typeKey;
            case LayerMaskBase::maskFlagsPropertyKey:
                return LayerMaskBase::typeKey;
            case LayerMaskBase::maskModeValuePropertyKey:
                return LayerMaskBase::typeKey;
            case DistanceConstraintBase::modeValuePropertyKey:
                return DistanceConstraintBase::typeKey;
            case TransformSpaceConstraintBase::sourceSpaceValuePropertyKey:
                return TransformSpaceConstraintBase::typeKey;
            case TransformSpaceConstraintBase::destSpaceValuePropertyKey:
                return TransformSpaceConstraintBase::typeKey;
            case TransformComponentConstraintBase::minMaxSpaceValuePropertyKey:
                return TransformComponentConstraintBase::typeKey;
            case IKConstraintBase::parentBoneCountPropertyKey:
                return IKConstraintBase::typeKey;
            case DraggableConstraintBase::directionValuePropertyKey:
                return DraggableConstraintBase::typeKey;
            case ScrollConstraintBase::physicsTypeValuePropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::virtualizeBufferPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::scrollFlagsPropertyKey:
                return ScrollConstraintBase::typeKey;
            case DrawableBase::blendModeValuePropertyKey:
                return DrawableBase::typeKey;
            case DrawableBase::additiveAmountPropertyKey:
                return DrawableBase::typeKey;
            case DrawableBase::drawableFlagsPropertyKey:
                return DrawableBase::typeKey;
            case NestedArtboardLayoutBase::instanceWidthUnitsValuePropertyKey:
                return NestedArtboardLayoutBase::typeKey;
            case NestedArtboardLayoutBase::instanceHeightUnitsValuePropertyKey:
                return NestedArtboardLayoutBase::typeKey;
            case NestedArtboardLayoutBase::instanceWidthScaleTypePropertyKey:
                return NestedArtboardLayoutBase::typeKey;
            case NestedArtboardLayoutBase::instanceHeightScaleTypePropertyKey:
                return NestedArtboardLayoutBase::typeKey;
            case NSlicerTileModeBase::patchIndexPropertyKey:
                return NSlicerTileModeBase::typeKey;
            case NSlicerTileModeBase::stylePropertyKey:
                return NSlicerTileModeBase::typeKey;
            case GridTrackBase::collectionPropertyKey:
                return GridTrackBase::typeKey;
            case GridTrackBase::trackTypePropertyKey:
                return GridTrackBase::typeKey;
            case GridTrackBase::trackMaxTypePropertyKey:
                return GridTrackBase::typeKey;
            case GridItemPlacementBase::gridColumnSpanPropertyKey:
                return GridItemPlacementBase::typeKey;
            case GridItemPlacementBase::gridRowSpanPropertyKey:
                return GridItemPlacementBase::typeKey;
            case LayoutSizingStyleBase::minWidthUnitsValuePropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::maxWidthUnitsValuePropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::minHeightUnitsValuePropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::maxHeightUnitsValuePropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::layoutWidthScaleTypePropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::layoutHeightScaleTypePropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::widthUnitsValuePropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::heightUnitsValuePropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::justifySelfValuePropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::displayValuePropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutComponentStyleBase::positionLeftUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::positionRightUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::positionTopUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::positionBottomUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::flexBasisUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::layoutAlignmentTypePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::animationStyleTypePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::interpolationTypePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::positionTypeValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::flexDirectionValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::directionValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::flexWrapValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::overflowValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::borderLeftUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::borderRightUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::borderTopUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::borderBottomUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::marginLeftUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::marginRightUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::marginTopUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::marginBottomUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::paddingLeftUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::paddingRightUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::paddingTopUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::paddingBottomUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::gapHorizontalUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::gapVerticalUnitsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::justifyItemsValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::layoutTypeValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case ArtboardComponentListOverrideBase::
                instanceWidthUnitsValuePropertyKey:
                return ArtboardComponentListOverrideBase::typeKey;
            case ArtboardComponentListOverrideBase::
                instanceHeightUnitsValuePropertyKey:
                return ArtboardComponentListOverrideBase::typeKey;
            case ArtboardComponentListOverrideBase::
                instanceWidthScaleTypePropertyKey:
                return ArtboardComponentListOverrideBase::typeKey;
            case ArtboardComponentListOverrideBase::
                instanceHeightScaleTypePropertyKey:
                return ArtboardComponentListOverrideBase::typeKey;
            case ListenerActionBase::flagsPropertyKey:
                return ListenerActionBase::typeKey;
            case LayerStateBase::flagsPropertyKey:
                return LayerStateBase::typeKey;
            case StateMachineFireActionBase::occursValuePropertyKey:
                return StateMachineFireActionBase::typeKey;
            case TransitionValueTriggerComparatorBase::valuePropertyKey:
                return TransitionValueTriggerComparatorBase::typeKey;
            case KeyFrameBase::framePropertyKey:
                return KeyFrameBase::typeKey;
            case InterpolatingKeyFrameBase::interpolationTypePropertyKey:
                return InterpolatingKeyFrameBase::typeKey;
            case KeyFrameUintBase::valuePropertyKey:
                return KeyFrameUintBase::typeKey;
            case BlendAnimationDirectBase::blendSourcePropertyKey:
                return BlendAnimationDirectBase::typeKey;
            case StateMachineListenerSingleBase::listenerTypeValuePropertyKey:
                return StateMachineListenerSingleBase::typeKey;
            case KeyedPropertyBase::propertyKeyPropertyKey:
                return KeyedPropertyBase::typeKey;
            case TransitionPropertyArtboardComparatorBase::
                propertyTypePropertyKey:
                return TransitionPropertyArtboardComparatorBase::typeKey;
            case ListenerBoolChangeBase::valuePropertyKey:
                return ListenerBoolChangeBase::typeKey;
            case TransitionViewModelConditionBase::opValuePropertyKey:
                return TransitionViewModelConditionBase::typeKey;
            case TransitionValueConditionBase::opValuePropertyKey:
                return TransitionValueConditionBase::typeKey;
            case StateTransitionBase::flagsPropertyKey:
                return StateTransitionBase::typeKey;
            case StateTransitionBase::durationPropertyKey:
                return StateTransitionBase::typeKey;
            case StateTransitionBase::exitTimePropertyKey:
                return StateTransitionBase::typeKey;
            case StateTransitionBase::interpolationTypePropertyKey:
                return StateTransitionBase::typeKey;
            case StateTransitionBase::randomWeightPropertyKey:
                return StateTransitionBase::typeKey;
            case FocusActionTraversalBase::traversalKindPropertyKey:
                return FocusActionTraversalBase::typeKey;
            case LinearAnimationBase::fpsPropertyKey:
                return LinearAnimationBase::typeKey;
            case LinearAnimationBase::durationPropertyKey:
                return LinearAnimationBase::typeKey;
            case LinearAnimationBase::loopValuePropertyKey:
                return LinearAnimationBase::typeKey;
            case LinearAnimationBase::workStartPropertyKey:
                return LinearAnimationBase::typeKey;
            case LinearAnimationBase::workEndPropertyKey:
                return LinearAnimationBase::typeKey;
            case ListenerViewModelChangeBase::inputValuePropertyKey:
                return ListenerViewModelChangeBase::typeKey;
            case ListenerViewModelChangeBase::inputValueIndexPropertyKey:
                return ListenerViewModelChangeBase::typeKey;
            case TransitionPropertyComponentComparatorBase::
                propertyKeyPropertyKey:
                return TransitionPropertyComponentComparatorBase::typeKey;
            case ElasticInterpolatorBase::easingValuePropertyKey:
                return ElasticInterpolatorBase::typeKey;
            case ListenerInputTypeBase::listenerTypeValuePropertyKey:
                return ListenerInputTypeBase::typeKey;
            case ListenerInputTypePointerButtonBase::
                pointerButtonValuePropertyKey:
                return ListenerInputTypePointerButtonBase::typeKey;
            case ShapePaintBase::blendModeValuePropertyKey:
                return ShapePaintBase::typeKey;
            case ShapePaintBase::additiveAmountPropertyKey:
                return ShapePaintBase::typeKey;
            case StrokeBase::capPropertyKey:
                return StrokeBase::typeKey;
            case StrokeBase::joinPropertyKey:
                return StrokeBase::typeKey;
            case StrokeBase::positionPropertyKey:
                return StrokeBase::typeKey;
            case PaintImageBase::imageSamplerFilterPropertyKey:
                return PaintImageBase::typeKey;
            case PaintImageBase::imageSamplerWrapXPropertyKey:
                return PaintImageBase::typeKey;
            case PaintImageBase::imageSamplerWrapYPropertyKey:
                return PaintImageBase::typeKey;
            case PaintImageBase::imageSizeModePropertyKey:
                return PaintImageBase::typeKey;
            case FeatherBase::spaceValuePropertyKey:
                return FeatherBase::typeKey;
            case TrimPathBase::modeValuePropertyKey:
                return TrimPathBase::typeKey;
            case FillBase::fillRulePropertyKey:
                return FillBase::typeKey;
            case PathBase::pathFlagsPropertyKey:
                return PathBase::typeKey;
            case ClippingShapeBase::fillRulePropertyKey:
                return ClippingShapeBase::typeKey;
            case PolygonBase::pointsPropertyKey:
                return PolygonBase::typeKey;
            case ImageBase::fitPropertyKey:
                return ImageBase::typeKey;
            case ImageBase::samplerFilterPropertyKey:
                return ImageBase::typeKey;
            case ImageBase::samplerWrapXPropertyKey:
                return ImageBase::typeKey;
            case ImageBase::samplerWrapYPropertyKey:
                return ImageBase::typeKey;
            case FocusDataBase::focusFlagsPropertyKey:
                return FocusDataBase::typeKey;
            case FocusDataBase::edgeBehaviorValuePropertyKey:
                return FocusDataBase::typeKey;
            case JoystickBase::joystickFlagsPropertyKey:
                return JoystickBase::typeKey;
            case OpenUrlEventBase::targetValuePropertyKey:
                return OpenUrlEventBase::typeKey;
            case SemanticDataBase::rolePropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::headingLevelPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::traitFlagsPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::stateFlagsPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isCheckedPropertyKey:
                return SemanticDataBase::typeKey;
            case BindablePropertyIntegerBase::propertyValuePropertyKey:
                return BindablePropertyIntegerBase::typeKey;
            case DataBindBase::propertyKeyPropertyKey:
                return DataBindBase::typeKey;
            case DataBindBase::flagsPropertyKey:
                return DataBindBase::typeKey;
            case DataConverterFormulaBase::randomModeValuePropertyKey:
                return DataConverterFormulaBase::typeKey;
            case DataConverterOperationBase::operationTypePropertyKey:
                return DataConverterOperationBase::typeKey;
            case DataConverterRangeMapperBase::interpolationTypePropertyKey:
                return DataConverterRangeMapperBase::typeKey;
            case DataConverterRangeMapperBase::flagsPropertyKey:
                return DataConverterRangeMapperBase::typeKey;
            case DataConverterInterpolatorBase::interpolationTypePropertyKey:
                return DataConverterInterpolatorBase::typeKey;
            case DataConverterRounderBase::decimalsPropertyKey:
                return DataConverterRounderBase::typeKey;
            case DataConverterStringPadBase::lengthPropertyKey:
                return DataConverterStringPadBase::typeKey;
            case DataConverterStringPadBase::padTypePropertyKey:
                return DataConverterStringPadBase::typeKey;
            case DataConverterStringTrimBase::trimTypePropertyKey:
                return DataConverterStringTrimBase::typeKey;
            case FormulaTokenOperationBase::operationTypePropertyKey:
                return FormulaTokenOperationBase::typeKey;
            case FormulaTokenFunctionBase::functionTypePropertyKey:
                return FormulaTokenFunctionBase::typeKey;
            case DataConverterToStringBase::flagsPropertyKey:
                return DataConverterToStringBase::typeKey;
            case DataConverterToStringBase::decimalsPropertyKey:
                return DataConverterToStringBase::typeKey;
            case NestedArtboardLeafBase::fitPropertyKey:
                return NestedArtboardLeafBase::typeKey;
            case WeightBase::valuesPropertyKey:
                return WeightBase::typeKey;
            case WeightBase::indicesPropertyKey:
                return WeightBase::typeKey;
            case CubicWeightBase::inValuesPropertyKey:
                return CubicWeightBase::typeKey;
            case CubicWeightBase::inIndicesPropertyKey:
                return CubicWeightBase::typeKey;
            case CubicWeightBase::outValuesPropertyKey:
                return CubicWeightBase::typeKey;
            case CubicWeightBase::outIndicesPropertyKey:
                return CubicWeightBase::typeKey;
            case TextModifierRangeBase::unitsValuePropertyKey:
                return TextModifierRangeBase::typeKey;
            case TextModifierRangeBase::typeValuePropertyKey:
                return TextModifierRangeBase::typeKey;
            case TextModifierRangeBase::modeValuePropertyKey:
                return TextModifierRangeBase::typeKey;
            case TextStyleFeatureBase::tagPropertyKey:
                return TextStyleFeatureBase::typeKey;
            case TextStyleFeatureBase::featureValuePropertyKey:
                return TextStyleFeatureBase::typeKey;
            case TextVariationModifierBase::axisTagPropertyKey:
                return TextVariationModifierBase::typeKey;
            case TextModifierGroupBase::modifierFlagsPropertyKey:
                return TextModifierGroupBase::typeKey;
            case TextInputBase::alignValuePropertyKey:
                return TextInputBase::typeKey;
            case TextInputBase::verticalAlignValuePropertyKey:
                return TextInputBase::typeKey;
            case TextStyleAxisBase::tagPropertyKey:
                return TextStyleAxisBase::typeKey;
            case TextBase::alignValuePropertyKey:
                return TextBase::typeKey;
            case TextBase::sizingValuePropertyKey:
                return TextBase::typeKey;
            case TextBase::overflowValuePropertyKey:
                return TextBase::typeKey;
            case TextBase::originValuePropertyKey:
                return TextBase::typeKey;
            case TextBase::wrapValuePropertyKey:
                return TextBase::typeKey;
            case TextBase::wordBreakValuePropertyKey:
                return TextBase::typeKey;
            case TextBase::verticalAlignValuePropertyKey:
                return TextBase::typeKey;
            case TextBase::verticalTrimValuePropertyKey:
                return TextBase::typeKey;
            case TextBase::verticalTrimTopValuePropertyKey:
                return TextBase::typeKey;
            case TextBase::verticalTrimBottomValuePropertyKey:
                return TextBase::typeKey;
            case FileAssetBase::assetIdPropertyKey:
                return FileAssetBase::typeKey;
            case ScriptAssetBase::generatorFunctionRefPropertyKey:
                return ScriptAssetBase::typeKey;
            case ScriptAssetBase::serializedImplementedMethodsPropertyKey:
                return ScriptAssetBase::typeKey;
            case ImageAssetBase::samplerFilterPropertyKey:
                return ImageAssetBase::typeKey;
            case ImageAssetBase::samplerWrapXPropertyKey:
                return ImageAssetBase::typeKey;
            case ImageAssetBase::samplerWrapYPropertyKey:
                return ImageAssetBase::typeKey;
            case ScriptModuleAssetBase::languagePropertyKey:
                return ScriptModuleAssetBase::typeKey;
            case BitmapCacheBase::cacheFlagsPropertyKey:
                return BitmapCacheBase::typeKey;
            case GamepadInputBase::kindPropertyKey:
                return GamepadInputBase::typeKey;
            case GamepadInputBase::mappingPropertyKey:
                return GamepadInputBase::typeKey;
            case GamepadInputBase::inputIndexPropertyKey:
                return GamepadInputBase::typeKey;
            case GamepadInputBase::buttonPhasePropertyKey:
                return GamepadInputBase::typeKey;
            case KeyboardInputBase::keyTypePropertyKey:
                return KeyboardInputBase::typeKey;
            case KeyboardInputBase::keyPhasePropertyKey:
                return KeyboardInputBase::typeKey;
            case KeyboardInputBase::modifiersPropertyKey:
                return KeyboardInputBase::typeKey;
            case SemanticInputBase::actionTypePropertyKey:
                return SemanticInputBase::typeKey;
            case ViewModelInstanceColorBase::propertyValuePropertyKey:
                return ViewModelInstanceColorBase::typeKey;
            case CustomPropertyColorBase::propertyValuePropertyKey:
                return CustomPropertyColorBase::typeKey;
            case KeyFrameColorBase::valuePropertyKey:
                return KeyFrameColorBase::typeKey;
            case TransitionValueColorComparatorBase::valuePropertyKey:
                return TransitionValueColorComparatorBase::typeKey;
            case SolidColorBase::colorValuePropertyKey:
                return SolidColorBase::typeKey;
            case GradientStopBase::colorValuePropertyKey:
                return GradientStopBase::typeKey;
            case SelectionStyleBase::highlightColorPropertyKey:
                return SelectionStyleBase::typeKey;
            case BindablePropertyColorBase::propertyValuePropertyKey:
                return BindablePropertyColorBase::typeKey;
            case ViewModelInstanceBooleanBase::propertyValuePropertyKey:
                return ViewModelInstanceBooleanBase::typeKey;
            case LayerMaskBase::isVisiblePropertyKey:
                return LayerMaskBase::typeKey;
            case LayerMaskBase::sourceDrawsPropertyKey:
                return LayerMaskBase::typeKey;
            case LayerMaskBase::useCustomBoundsPropertyKey:
                return LayerMaskBase::typeKey;
            case FollowPathConstraintBase::orientPropertyKey:
                return FollowPathConstraintBase::typeKey;
            case FollowPathConstraintBase::offsetPropertyKey:
                return FollowPathConstraintBase::typeKey;
            case TransformComponentConstraintBase::offsetPropertyKey:
                return TransformComponentConstraintBase::typeKey;
            case TransformComponentConstraintBase::doesCopyPropertyKey:
                return TransformComponentConstraintBase::typeKey;
            case TransformComponentConstraintBase::minPropertyKey:
                return TransformComponentConstraintBase::typeKey;
            case TransformComponentConstraintBase::maxPropertyKey:
                return TransformComponentConstraintBase::typeKey;
            case TransformComponentConstraintYBase::doesCopyYPropertyKey:
                return TransformComponentConstraintYBase::typeKey;
            case TransformComponentConstraintYBase::minYPropertyKey:
                return TransformComponentConstraintYBase::typeKey;
            case TransformComponentConstraintYBase::maxYPropertyKey:
                return TransformComponentConstraintYBase::typeKey;
            case IKConstraintBase::invertDirectionPropertyKey:
                return IKConstraintBase::typeKey;
            case ScrollConstraintBase::snapPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::virtualizePropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::infinitePropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::interactivePropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::scrollActivePropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::wheelInteractivePropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollBarConstraintBase::autoSizePropertyKey:
                return ScrollBarConstraintBase::typeKey;
            case NestedArtboardBase::isPausedPropertyKey:
                return NestedArtboardBase::typeKey;
            case NestedArtboardBase::isStatefulPropertyKey:
                return NestedArtboardBase::typeKey;
            case LayoutSizingStyleBase::hugUnboundedPropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case AxisBase::normalizedPropertyKey:
                return AxisBase::typeKey;
            case LayoutComponentStyleBase::intrinsicallySizedValuePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::linkCornerRadiusPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case NestedSimpleAnimationBase::isPlayingPropertyKey:
                return NestedSimpleAnimationBase::typeKey;
            case KeyFrameBoolBase::valuePropertyKey:
                return KeyFrameBoolBase::typeKey;
            case ListenerAlignTargetBase::preserveOffsetPropertyKey:
                return ListenerAlignTargetBase::typeKey;
            case TransitionValueBooleanComparatorBase::valuePropertyKey:
                return TransitionValueBooleanComparatorBase::typeKey;
            case NestedBoolBase::nestedValuePropertyKey:
                return NestedBoolBase::typeKey;
            case LinearAnimationBase::enableWorkAreaPropertyKey:
                return LinearAnimationBase::typeKey;
            case LinearAnimationBase::quantizePropertyKey:
                return LinearAnimationBase::typeKey;
            case StateMachineBoolBase::valuePropertyKey:
                return StateMachineBoolBase::typeKey;
            case ShapePaintBase::isVisiblePropertyKey:
                return ShapePaintBase::typeKey;
            case DashPathBase::offsetIsPercentagePropertyKey:
                return DashPathBase::typeKey;
            case DashBase::lengthIsPercentagePropertyKey:
                return DashBase::typeKey;
            case StrokeBase::transformAffectsStrokePropertyKey:
                return StrokeBase::typeKey;
            case FeatherBase::innerPropertyKey:
                return FeatherBase::typeKey;
            case PathBase::isHolePropertyKey:
                return PathBase::typeKey;
            case PointsCommonPathBase::isClosedPropertyKey:
                return PointsCommonPathBase::typeKey;
            case RectangleBase::linkCornerRadiusPropertyKey:
                return RectangleBase::typeKey;
            case ClippingShapeBase::isVisiblePropertyKey:
                return ClippingShapeBase::typeKey;
            case FocusDataBase::canFocusPropertyKey:
                return FocusDataBase::typeKey;
            case FocusDataBase::canTouchPropertyKey:
                return FocusDataBase::typeKey;
            case FocusDataBase::canTraversePropertyKey:
                return FocusDataBase::typeKey;
            case CustomPropertyBooleanBase::propertyValuePropertyKey:
                return CustomPropertyBooleanBase::typeKey;
            case LayoutComponentBase::clipPropertyKey:
                return LayoutComponentBase::typeKey;
            case SemanticDataBase::isExpandablePropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isSelectablePropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isCheckablePropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isToggleablePropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isRequirablePropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isEnablablePropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isFocusablePropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isExpandedPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isSelectedPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isToggledPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isRequiredPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isDisabledPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isFocusedPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isHiddenPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isLiveRegionPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isReadOnlyPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isModalPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isObscuredPropertyKey:
                return SemanticDataBase::typeKey;
            case SemanticDataBase::isMultilinePropertyKey:
                return SemanticDataBase::typeKey;
            case DataBindPathBase::isRelativePropertyKey:
                return DataBindPathBase::typeKey;
            case BindablePropertyBooleanBase::propertyValuePropertyKey:
                return BindablePropertyBooleanBase::typeKey;
            case NestedArtboardLeafBase::fitToLayoutParentPropertyKey:
                return NestedArtboardLeafBase::typeKey;
            case TextModifierRangeBase::clampPropertyKey:
                return TextModifierRangeBase::typeKey;
            case TextFollowPathModifierBase::radialPropertyKey:
                return TextFollowPathModifierBase::typeKey;
            case TextFollowPathModifierBase::orientPropertyKey:
                return TextFollowPathModifierBase::typeKey;
            case TextInputBase::multilinePropertyKey:
                return TextInputBase::typeKey;
            case TextInputBase::obscuredPropertyKey:
                return TextInputBase::typeKey;
            case TextInputBase::selectAllOnFocusPropertyKey:
                return TextInputBase::typeKey;
            case TextBase::fitFromBaselinePropertyKey:
                return TextBase::typeKey;
            case TextBase::fitFontSizeResizesBoxPropertyKey:
                return TextBase::typeKey;
            case ScriptAssetBase::isModulePropertyKey:
                return ScriptAssetBase::typeKey;
            case BitmapCacheBase::cacheEnabledPropertyKey:
                return BitmapCacheBase::typeKey;
            case BitmapCacheBase::ditherPropertyKey:
                return BitmapCacheBase::typeKey;
            case ViewModelInstanceNumberBase::propertyValuePropertyKey:
                return ViewModelInstanceNumberBase::typeKey;
            case LayerMaskBase::resolutionPropertyKey:
                return LayerMaskBase::typeKey;
            case LayerMaskBase::boundsXPropertyKey:
                return LayerMaskBase::typeKey;
            case LayerMaskBase::boundsYPropertyKey:
                return LayerMaskBase::typeKey;
            case LayerMaskBase::boundsWidthPropertyKey:
                return LayerMaskBase::typeKey;
            case LayerMaskBase::boundsHeightPropertyKey:
                return LayerMaskBase::typeKey;
            case CustomPropertyNumberBase::propertyValuePropertyKey:
                return CustomPropertyNumberBase::typeKey;
            case ConstraintBase::strengthPropertyKey:
                return ConstraintBase::typeKey;
            case DistanceConstraintBase::distancePropertyKey:
                return DistanceConstraintBase::typeKey;
            case FollowPathConstraintBase::distancePropertyKey:
                return FollowPathConstraintBase::typeKey;
            case ListFollowPathConstraintBase::distanceEndPropertyKey:
                return ListFollowPathConstraintBase::typeKey;
            case ListFollowPathConstraintBase::distanceOffsetPropertyKey:
                return ListFollowPathConstraintBase::typeKey;
            case TransformComponentConstraintBase::copyFactorPropertyKey:
                return TransformComponentConstraintBase::typeKey;
            case TransformComponentConstraintBase::minValuePropertyKey:
                return TransformComponentConstraintBase::typeKey;
            case TransformComponentConstraintBase::maxValuePropertyKey:
                return TransformComponentConstraintBase::typeKey;
            case TransformComponentConstraintYBase::copyFactorYPropertyKey:
                return TransformComponentConstraintYBase::typeKey;
            case TransformComponentConstraintYBase::minValueYPropertyKey:
                return TransformComponentConstraintYBase::typeKey;
            case TransformComponentConstraintYBase::maxValueYPropertyKey:
                return TransformComponentConstraintYBase::typeKey;
            case ScrollConstraintBase::scrollOffsetXPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::scrollOffsetYPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::scrollPercentXPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::scrollPercentYPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::scrollIndexPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::thresholdPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::velocityXPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::velocityYPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::dragMultiplierPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::computedContentWidthPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ScrollConstraintBase::computedContentHeightPropertyKey:
                return ScrollConstraintBase::typeKey;
            case ElasticScrollPhysicsBase::frictionPropertyKey:
                return ElasticScrollPhysicsBase::typeKey;
            case ElasticScrollPhysicsBase::speedMultiplierPropertyKey:
                return ElasticScrollPhysicsBase::typeKey;
            case ElasticScrollPhysicsBase::elasticFactorPropertyKey:
                return ElasticScrollPhysicsBase::typeKey;
            case TransformConstraintBase::originXPropertyKey:
                return TransformConstraintBase::typeKey;
            case TransformConstraintBase::originYPropertyKey:
                return TransformConstraintBase::typeKey;
            case WorldTransformComponentBase::opacityPropertyKey:
                return WorldTransformComponentBase::typeKey;
            case TransformComponentBase::rotationPropertyKey:
                return TransformComponentBase::typeKey;
            case TransformComponentBase::scaleXPropertyKey:
                return TransformComponentBase::typeKey;
            case TransformComponentBase::scaleYPropertyKey:
                return TransformComponentBase::typeKey;
            case NodeBase::xPropertyKey:
            case NodeBase::xArtboardPropertyKey:
                return NodeBase::typeKey;
            case NodeBase::yPropertyKey:
            case NodeBase::yArtboardPropertyKey:
                return NodeBase::typeKey;
            case NodeBase::computedLocalXPropertyKey:
                return NodeBase::typeKey;
            case NodeBase::computedLocalYPropertyKey:
                return NodeBase::typeKey;
            case NodeBase::computedWorldXPropertyKey:
                return NodeBase::typeKey;
            case NodeBase::computedWorldYPropertyKey:
                return NodeBase::typeKey;
            case NodeBase::computedRootXPropertyKey:
                return NodeBase::typeKey;
            case NodeBase::computedRootYPropertyKey:
                return NodeBase::typeKey;
            case NodeBase::computedWidthPropertyKey:
                return NodeBase::typeKey;
            case NodeBase::computedHeightPropertyKey:
                return NodeBase::typeKey;
            case NestedArtboardBase::speedPropertyKey:
                return NestedArtboardBase::typeKey;
            case NestedArtboardBase::quantizePropertyKey:
                return NestedArtboardBase::typeKey;
            case NestedArtboardLayoutBase::instanceWidthPropertyKey:
                return NestedArtboardLayoutBase::typeKey;
            case NestedArtboardLayoutBase::instanceHeightPropertyKey:
                return NestedArtboardLayoutBase::typeKey;
            case GridTrackBase::trackValuePropertyKey:
                return GridTrackBase::typeKey;
            case GridTrackBase::trackMaxValuePropertyKey:
                return GridTrackBase::typeKey;
            case LayoutSizingStyleBase::minWidthPropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::maxWidthPropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::minHeightPropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutSizingStyleBase::maxHeightPropertyKey:
                return LayoutSizingStyleBase::typeKey;
            case LayoutNodeStyleBase::widthPropertyKey:
                return LayoutNodeStyleBase::typeKey;
            case LayoutNodeStyleBase::heightPropertyKey:
                return LayoutNodeStyleBase::typeKey;
            case LayoutNodeStyleBase::fractionalWidthPropertyKey:
                return LayoutNodeStyleBase::typeKey;
            case LayoutNodeStyleBase::fractionalHeightPropertyKey:
                return LayoutNodeStyleBase::typeKey;
            case AxisBase::offsetPropertyKey:
                return AxisBase::typeKey;
            case LayoutComponentStyleBase::gapHorizontalPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::gapVerticalPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::borderLeftPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::borderRightPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::borderTopPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::borderBottomPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::marginLeftPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::marginRightPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::marginTopPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::marginBottomPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::paddingLeftPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::paddingRightPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::paddingTopPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::paddingBottomPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::positionLeftPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::positionRightPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::positionTopPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::positionBottomPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::flexBasisPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::aspectRatioPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::interpolationTimePropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::cornerRadiusTLPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::cornerRadiusTRPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::cornerRadiusBLPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case LayoutComponentStyleBase::cornerRadiusBRPropertyKey:
                return LayoutComponentStyleBase::typeKey;
            case NSlicedNodeBase::initialWidthPropertyKey:
                return NSlicedNodeBase::typeKey;
            case NSlicedNodeBase::initialHeightPropertyKey:
                return NSlicedNodeBase::typeKey;
            case NSlicedNodeBase::widthPropertyKey:
                return NSlicedNodeBase::typeKey;
            case NSlicedNodeBase::heightPropertyKey:
                return NSlicedNodeBase::typeKey;
            case ArtboardComponentListOverrideBase::instanceWidthPropertyKey:
                return ArtboardComponentListOverrideBase::typeKey;
            case ArtboardComponentListOverrideBase::instanceHeightPropertyKey:
                return ArtboardComponentListOverrideBase::typeKey;
            case ComponentOriginBase::originXPropertyKey:
                return ComponentOriginBase::typeKey;
            case ComponentOriginBase::originYPropertyKey:
                return ComponentOriginBase::typeKey;
            case NestedLinearAnimationBase::mixPropertyKey:
                return NestedLinearAnimationBase::typeKey;
            case NestedSimpleAnimationBase::speedPropertyKey:
                return NestedSimpleAnimationBase::typeKey;
            case AdvanceableStateBase::speedPropertyKey:
                return AdvanceableStateBase::typeKey;
            case BlendAnimationDirectBase::mixValuePropertyKey:
                return BlendAnimationDirectBase::typeKey;
            case StateMachineNumberBase::valuePropertyKey:
                return StateMachineNumberBase::typeKey;
            case CubicInterpolatorBase::x1PropertyKey:
                return CubicInterpolatorBase::typeKey;
            case CubicInterpolatorBase::y1PropertyKey:
                return CubicInterpolatorBase::typeKey;
            case CubicInterpolatorBase::x2PropertyKey:
                return CubicInterpolatorBase::typeKey;
            case CubicInterpolatorBase::y2PropertyKey:
                return CubicInterpolatorBase::typeKey;
            case TransitionNumberConditionBase::valuePropertyKey:
                return TransitionNumberConditionBase::typeKey;
            case CubicInterpolatorComponentBase::x1PropertyKey:
                return CubicInterpolatorComponentBase::typeKey;
            case CubicInterpolatorComponentBase::y1PropertyKey:
                return CubicInterpolatorComponentBase::typeKey;
            case CubicInterpolatorComponentBase::x2PropertyKey:
                return CubicInterpolatorComponentBase::typeKey;
            case CubicInterpolatorComponentBase::y2PropertyKey:
                return CubicInterpolatorComponentBase::typeKey;
            case ListenerNumberChangeBase::valuePropertyKey:
                return ListenerNumberChangeBase::typeKey;
            case KeyFrameDoubleBase::valuePropertyKey:
                return KeyFrameDoubleBase::typeKey;
            case LinearAnimationBase::speedPropertyKey:
                return LinearAnimationBase::typeKey;
            case TransitionValueNumberComparatorBase::valuePropertyKey:
                return TransitionValueNumberComparatorBase::typeKey;
            case ElasticInterpolatorBase::amplitudePropertyKey:
                return ElasticInterpolatorBase::typeKey;
            case ElasticInterpolatorBase::periodPropertyKey:
                return ElasticInterpolatorBase::typeKey;
            case NestedNumberBase::nestedValuePropertyKey:
                return NestedNumberBase::typeKey;
            case NestedRemapAnimationBase::timePropertyKey:
                return NestedRemapAnimationBase::typeKey;
            case BlendAnimation1DBase::valuePropertyKey:
                return BlendAnimation1DBase::typeKey;
            case DashPathBase::offsetPropertyKey:
                return DashPathBase::typeKey;
            case LinearGradientBase::startXPropertyKey:
                return LinearGradientBase::typeKey;
            case LinearGradientBase::startYPropertyKey:
                return LinearGradientBase::typeKey;
            case LinearGradientBase::endXPropertyKey:
                return LinearGradientBase::typeKey;
            case LinearGradientBase::endYPropertyKey:
                return LinearGradientBase::typeKey;
            case LinearGradientBase::opacityPropertyKey:
                return LinearGradientBase::typeKey;
            case DashBase::lengthPropertyKey:
                return DashBase::typeKey;
            case StrokeBase::thicknessPropertyKey:
                return StrokeBase::typeKey;
            case PaintImageBase::imageScaleXPropertyKey:
                return PaintImageBase::typeKey;
            case PaintImageBase::imageScaleYPropertyKey:
                return PaintImageBase::typeKey;
            case PaintImageBase::imageOffsetXPropertyKey:
                return PaintImageBase::typeKey;
            case PaintImageBase::imageOffsetYPropertyKey:
                return PaintImageBase::typeKey;
            case PaintImageBase::imageRotationPropertyKey:
                return PaintImageBase::typeKey;
            case GradientStopBase::positionPropertyKey:
                return GradientStopBase::typeKey;
            case FeatherBase::strengthPropertyKey:
                return FeatherBase::typeKey;
            case FeatherBase::offsetXPropertyKey:
                return FeatherBase::typeKey;
            case FeatherBase::offsetYPropertyKey:
                return FeatherBase::typeKey;
            case TrimPathBase::startPropertyKey:
                return TrimPathBase::typeKey;
            case TrimPathBase::endPropertyKey:
                return TrimPathBase::typeKey;
            case TrimPathBase::offsetPropertyKey:
                return TrimPathBase::typeKey;
            case VertexBase::xPropertyKey:
                return VertexBase::typeKey;
            case VertexBase::yPropertyKey:
                return VertexBase::typeKey;
            case MeshVertexBase::uPropertyKey:
                return MeshVertexBase::typeKey;
            case MeshVertexBase::vPropertyKey:
                return MeshVertexBase::typeKey;
            case ShapeBase::lengthPropertyKey:
                return ShapeBase::typeKey;
            case StraightVertexBase::radiusPropertyKey:
                return StraightVertexBase::typeKey;
            case CubicAsymmetricVertexBase::rotationPropertyKey:
                return CubicAsymmetricVertexBase::typeKey;
            case CubicAsymmetricVertexBase::inDistancePropertyKey:
                return CubicAsymmetricVertexBase::typeKey;
            case CubicAsymmetricVertexBase::outDistancePropertyKey:
                return CubicAsymmetricVertexBase::typeKey;
            case ParametricPathBase::widthPropertyKey:
                return ParametricPathBase::typeKey;
            case ParametricPathBase::heightPropertyKey:
                return ParametricPathBase::typeKey;
            case ParametricPathBase::originXPropertyKey:
                return ParametricPathBase::typeKey;
            case ParametricPathBase::originYPropertyKey:
                return ParametricPathBase::typeKey;
            case RectangleBase::cornerRadiusTLPropertyKey:
                return RectangleBase::typeKey;
            case RectangleBase::cornerRadiusTRPropertyKey:
                return RectangleBase::typeKey;
            case RectangleBase::cornerRadiusBLPropertyKey:
                return RectangleBase::typeKey;
            case RectangleBase::cornerRadiusBRPropertyKey:
                return RectangleBase::typeKey;
            case CubicMirroredVertexBase::rotationPropertyKey:
                return CubicMirroredVertexBase::typeKey;
            case CubicMirroredVertexBase::distancePropertyKey:
                return CubicMirroredVertexBase::typeKey;
            case PolygonBase::cornerRadiusPropertyKey:
                return PolygonBase::typeKey;
            case StarBase::innerRadiusPropertyKey:
                return StarBase::typeKey;
            case ImageBase::originXPropertyKey:
                return ImageBase::typeKey;
            case ImageBase::originYPropertyKey:
                return ImageBase::typeKey;
            case ImageBase::alignmentXPropertyKey:
                return ImageBase::typeKey;
            case ImageBase::alignmentYPropertyKey:
                return ImageBase::typeKey;
            case CubicDetachedVertexBase::inRotationPropertyKey:
                return CubicDetachedVertexBase::typeKey;
            case CubicDetachedVertexBase::inDistancePropertyKey:
                return CubicDetachedVertexBase::typeKey;
            case CubicDetachedVertexBase::outRotationPropertyKey:
                return CubicDetachedVertexBase::typeKey;
            case CubicDetachedVertexBase::outDistancePropertyKey:
                return CubicDetachedVertexBase::typeKey;
            case LayoutComponentBase::widthPropertyKey:
                return LayoutComponentBase::typeKey;
            case LayoutComponentBase::heightPropertyKey:
                return LayoutComponentBase::typeKey;
            case LayoutComponentBase::fractionalWidthPropertyKey:
                return LayoutComponentBase::typeKey;
            case LayoutComponentBase::fractionalHeightPropertyKey:
                return LayoutComponentBase::typeKey;
            case ArtboardBase::originXPropertyKey:
                return ArtboardBase::typeKey;
            case ArtboardBase::originYPropertyKey:
                return ArtboardBase::typeKey;
            case JoystickBase::xPropertyKey:
                return JoystickBase::typeKey;
            case JoystickBase::yPropertyKey:
                return JoystickBase::typeKey;
            case JoystickBase::posXPropertyKey:
                return JoystickBase::typeKey;
            case JoystickBase::posYPropertyKey:
                return JoystickBase::typeKey;
            case JoystickBase::originXPropertyKey:
                return JoystickBase::typeKey;
            case JoystickBase::originYPropertyKey:
                return JoystickBase::typeKey;
            case JoystickBase::widthPropertyKey:
                return JoystickBase::typeKey;
            case JoystickBase::heightPropertyKey:
                return JoystickBase::typeKey;
            case SelectionStyleBase::cornerRadiusPropertyKey:
                return SelectionStyleBase::typeKey;
            case DataConverterOperationValueBase::operationValuePropertyKey:
                return DataConverterOperationValueBase::typeKey;
            case DataConverterRangeMapperBase::minInputPropertyKey:
                return DataConverterRangeMapperBase::typeKey;
            case DataConverterRangeMapperBase::maxInputPropertyKey:
                return DataConverterRangeMapperBase::typeKey;
            case DataConverterRangeMapperBase::minOutputPropertyKey:
                return DataConverterRangeMapperBase::typeKey;
            case DataConverterRangeMapperBase::maxOutputPropertyKey:
                return DataConverterRangeMapperBase::typeKey;
            case DataConverterInterpolatorBase::durationPropertyKey:
                return DataConverterInterpolatorBase::typeKey;
            case FormulaTokenValueBase::operationValuePropertyKey:
                return FormulaTokenValueBase::typeKey;
            case BindablePropertyNumberBase::propertyValuePropertyKey:
                return BindablePropertyNumberBase::typeKey;
            case NestedArtboardLeafBase::alignmentXPropertyKey:
                return NestedArtboardLeafBase::typeKey;
            case NestedArtboardLeafBase::alignmentYPropertyKey:
                return NestedArtboardLeafBase::typeKey;
            case BoneBase::lengthPropertyKey:
                return BoneBase::typeKey;
            case RootBoneBase::xPropertyKey:
                return RootBoneBase::typeKey;
            case RootBoneBase::yPropertyKey:
                return RootBoneBase::typeKey;
            case SkinBase::xxPropertyKey:
                return SkinBase::typeKey;
            case SkinBase::yxPropertyKey:
                return SkinBase::typeKey;
            case SkinBase::xyPropertyKey:
                return SkinBase::typeKey;
            case SkinBase::yyPropertyKey:
                return SkinBase::typeKey;
            case SkinBase::txPropertyKey:
                return SkinBase::typeKey;
            case SkinBase::tyPropertyKey:
                return SkinBase::typeKey;
            case TendonBase::xxPropertyKey:
                return TendonBase::typeKey;
            case TendonBase::yxPropertyKey:
                return TendonBase::typeKey;
            case TendonBase::xyPropertyKey:
                return TendonBase::typeKey;
            case TendonBase::yyPropertyKey:
                return TendonBase::typeKey;
            case TendonBase::txPropertyKey:
                return TendonBase::typeKey;
            case TendonBase::tyPropertyKey:
                return TendonBase::typeKey;
            case TextModifierRangeBase::modifyFromPropertyKey:
                return TextModifierRangeBase::typeKey;
            case TextModifierRangeBase::modifyToPropertyKey:
                return TextModifierRangeBase::typeKey;
            case TextModifierRangeBase::strengthPropertyKey:
                return TextModifierRangeBase::typeKey;
            case TextModifierRangeBase::falloffFromPropertyKey:
                return TextModifierRangeBase::typeKey;
            case TextModifierRangeBase::falloffToPropertyKey:
                return TextModifierRangeBase::typeKey;
            case TextModifierRangeBase::offsetPropertyKey:
                return TextModifierRangeBase::typeKey;
            case TextFollowPathModifierBase::startPropertyKey:
                return TextFollowPathModifierBase::typeKey;
            case TextFollowPathModifierBase::endPropertyKey:
                return TextFollowPathModifierBase::typeKey;
            case TextFollowPathModifierBase::strengthPropertyKey:
                return TextFollowPathModifierBase::typeKey;
            case TextFollowPathModifierBase::offsetPropertyKey:
                return TextFollowPathModifierBase::typeKey;
            case TextStyleBackgroundBase::cornerRadiusPropertyKey:
                return TextStyleBackgroundBase::typeKey;
            case TextVariationModifierBase::axisValuePropertyKey:
                return TextVariationModifierBase::typeKey;
            case TextModifierGroupBase::originXPropertyKey:
                return TextModifierGroupBase::typeKey;
            case TextModifierGroupBase::originYPropertyKey:
                return TextModifierGroupBase::typeKey;
            case TextModifierGroupBase::opacityPropertyKey:
                return TextModifierGroupBase::typeKey;
            case TextModifierGroupBase::xPropertyKey:
                return TextModifierGroupBase::typeKey;
            case TextModifierGroupBase::yPropertyKey:
                return TextModifierGroupBase::typeKey;
            case TextModifierGroupBase::rotationPropertyKey:
                return TextModifierGroupBase::typeKey;
            case TextModifierGroupBase::scaleXPropertyKey:
                return TextModifierGroupBase::typeKey;
            case TextModifierGroupBase::scaleYPropertyKey:
                return TextModifierGroupBase::typeKey;
            case TextStyleBase::fontSizePropertyKey:
                return TextStyleBase::typeKey;
            case TextStyleBase::lineHeightPropertyKey:
                return TextStyleBase::typeKey;
            case TextStyleBase::letterSpacingPropertyKey:
                return TextStyleBase::typeKey;
            case TextInputBase::selectionRadiusPropertyKey:
                return TextInputBase::typeKey;
            case TextStyleAxisBase::axisValuePropertyKey:
                return TextStyleAxisBase::typeKey;
            case TextBase::widthPropertyKey:
                return TextBase::typeKey;
            case TextBase::heightPropertyKey:
                return TextBase::typeKey;
            case TextBase::originXPropertyKey:
                return TextBase::typeKey;
            case TextBase::originYPropertyKey:
                return TextBase::typeKey;
            case TextBase::paragraphSpacingPropertyKey:
                return TextBase::typeKey;
            case ExportAudioBase::volumePropertyKey:
                return ExportAudioBase::typeKey;
            case DrawableAssetBase::heightPropertyKey:
                return DrawableAssetBase::typeKey;
            case DrawableAssetBase::widthPropertyKey:
                return DrawableAssetBase::typeKey;
            case BitmapCacheBase::resolutionPropertyKey:
                return BitmapCacheBase::typeKey;
            case ViewModelInstanceTriggerBase::firePropertyKey:
                return ViewModelInstanceTriggerBase::typeKey;
            case CustomPropertyTriggerBase::firePropertyKey:
                return CustomPropertyTriggerBase::typeKey;
            case NestedTriggerBase::firePropertyKey:
                return NestedTriggerBase::typeKey;
            case EventBase::triggerPropertyKey:
                return EventBase::typeKey;
            case GridItemPlacementBase::gridColumnPropertyKey:
                return GridItemPlacementBase::typeKey;
            case GridItemPlacementBase::gridRowPropertyKey:
                return GridItemPlacementBase::typeKey;
            case KeyFrameIntBase::valuePropertyKey:
                return KeyFrameIntBase::typeKey;
        }
        return 0;
    }
    static bool objectSupportsProperty(Core* object, uint32_t propertyKey)
    {
        switch (propertyKey)
        {
            case ColorChannelsBase::colorRedPropertyKey:
                return ColorChannelsBase::from(object) != nullptr;
            case ColorChannelsBase::colorGreenPropertyKey:
                return ColorChannelsBase::from(object) != nullptr;
            case ColorChannelsBase::colorBluePropertyKey:
                return ColorChannelsBase::from(object) != nullptr;
            case ColorChannelsBase::colorAlphaPropertyKey:
                return ColorChannelsBase::from(object) != nullptr;
        }
        uint16_t owner = propertyOwnerTypeKey(propertyKey);
        return owner != 0 && object->isTypeOf(owner);
    }
};
} // namespace rive

#endif