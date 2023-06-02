#include "rive/animation/animation.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/any_state.hpp"
#include "rive/animation/entry_state.hpp"
#include "rive/animation/exit_state.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/artboard.hpp"
#include "rive/bones/bone.hpp"
#include "rive/bones/root_bone.hpp"
#include "rive/component.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/core.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/file.hpp"
#include "rive/layout.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/node.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/cubic_vertex.hpp"
#include "rive/shapes/path.hpp"
#include "rive/transform_component.hpp"

#include "js_alignment.hpp"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
// #include <sanitizer/lsan_interface.h>

using namespace emscripten;

// We had to do this because binding the core class const defined types directly
// caused wasm-ld linker issues. See
// https://www.mail-archive.com/emscripten-discuss@googlegroups.com/msg09132.html
// for other people suffering our same pains.
const uint16_t stateMachineBoolTypeKey = rive::StateMachineBoolBase::typeKey;

const uint16_t stateMachineNumberTypeKey = rive::StateMachineNumberBase::typeKey;

const uint16_t stateMachineTriggerTypeKey = rive::StateMachineTriggerBase::typeKey;

rive::Alignment convertAlignment(JsAlignment alignment)
{
    switch (alignment)
    {
        case JsAlignment::topLeft:
            return rive::Alignment::topLeft;
        case JsAlignment::topCenter:
            return rive::Alignment::topCenter;
        case JsAlignment::topRight:
            return rive::Alignment::topRight;
        case JsAlignment::centerLeft:
            return rive::Alignment::centerLeft;
        case JsAlignment::center:
            return rive::Alignment::center;
        case JsAlignment::centerRight:
            return rive::Alignment::centerRight;
        case JsAlignment::bottomLeft:
            return rive::Alignment::bottomLeft;
        case JsAlignment::bottomCenter:
            return rive::Alignment::bottomCenter;
        case JsAlignment::bottomRight:
            return rive::Alignment::bottomRight;
    }
    return rive::Alignment::center;
}

rive::Mat2D computeAlignment(rive::Fit fit, JsAlignment alignment, rive::AABB orig, rive::AABB dest)
{
    return rive::computeAlignment(fit, convertAlignment(alignment), orig, dest);
}

rive::Vec2D mapXY(rive::Mat2D invertedMatrix, rive::Vec2D canvasVector)
{
    return invertedMatrix * canvasVector;
}

EMSCRIPTEN_KEEPALIVE
EMSCRIPTEN_BINDINGS(RiveWASM)
{
    function("computeAlignment", &computeAlignment);
    function("mapXY", &mapXY);

#ifdef ENABLE_QUERY_FLAT_VERTICES
    class_<rive::FlattenedPath>("FlattenedPath")
        .function("length", optional_override([](rive::FlattenedPath& self) -> size_t {
                      return self.vertices().size();
                  }))
        .function("isCubic", optional_override([](rive::FlattenedPath& self, size_t index) -> bool {
                      if (index >= self.vertices().size())
                      {
                          return false;
                      }
                      return self.vertices()[index]->is<rive::CubicVertex>();
                  }))
        .function("x", optional_override([](rive::FlattenedPath& self, size_t index) -> float {
                      return self.vertices()[index]->x();
                  }))
        .function("y", optional_override([](rive::FlattenedPath& self, size_t index) -> float {
                      return self.vertices()[index]->y();
                  }))
        .function("inX", optional_override([](rive::FlattenedPath& self, size_t index) -> float {
                      return self.vertices()[index]->as<rive::CubicVertex>()->renderIn()[0];
                  }))
        .function("inY", optional_override([](rive::FlattenedPath& self, size_t index) -> float {
                      return self.vertices()[index]->as<rive::CubicVertex>()->renderIn()[1];
                  }))
        .function("outX", optional_override([](rive::FlattenedPath& self, size_t index) -> float {
                      return self.vertices()[index]->as<rive::CubicVertex>()->renderOut()[0];
                  }))
        .function("outY", optional_override([](rive::FlattenedPath& self, size_t index) -> float {
                      return self.vertices()[index]->as<rive::CubicVertex>()->renderOut()[1];
                  }));
#endif

    class_<rive::Vec2D>("Vec2D")
        .constructor<float, float>()
        // TODO: For next major verison, make these properties instead of methods to match
        // patterns on other math-based Rive classes, such as Mat2D
        .function("x", optional_override([](rive::Vec2D self) -> float { return self.x; }))
        .function("y", optional_override([](rive::Vec2D self) -> float { return self.y; }));

    class_<rive::Mat2D>("Mat2D")
        .constructor<>()
        .property("xx",
                  select_overload<float() const>(&rive::Mat2D::xx),
                  select_overload<void(float)>(&rive::Mat2D::xx))
        .property("xy",
                  select_overload<float() const>(&rive::Mat2D::xy),
                  select_overload<void(float)>(&rive::Mat2D::xy))
        .property("yx",
                  select_overload<float() const>(&rive::Mat2D::yx),
                  select_overload<void(float)>(&rive::Mat2D::yx))
        .property("yy",
                  select_overload<float() const>(&rive::Mat2D::yy),
                  select_overload<void(float)>(&rive::Mat2D::yy))
        .property("tx",
                  select_overload<float() const>(&rive::Mat2D::tx),
                  select_overload<void(float)>(&rive::Mat2D::tx))
        .property("ty",
                  select_overload<float() const>(&rive::Mat2D::ty),
                  select_overload<void(float)>(&rive::Mat2D::ty))
        .function("invert", optional_override([](rive::Mat2D& self, rive::Mat2D& result) -> bool {
                      return self.invert(&result);
                  }))
        .function("multiply",
                  optional_override([](rive::Mat2D& self, rive::Mat2D& result, rive::Mat2D& other)
                                        -> void { result = rive::Mat2D::multiply(self, other); }));

    class_<rive::File>("File")
        .function("defaultArtboard",
                  optional_override([](rive::File& self) -> rive::ArtboardInstance* {
                      return self.artboardAt(0).release();
                  }),
                  allow_raw_pointers())
        .function("artboardByName",
                  optional_override([](const rive::File& self,
                                       const std::string& name) -> rive::ArtboardInstance* {
                      return self.artboardNamed(name).release();
                  }),
                  allow_raw_pointers())
        .function(
            "artboardByIndex",
            optional_override([](const rive::File& self, size_t index) -> rive::ArtboardInstance* {
                return self.artboardAt(index).release();
            }),
            allow_raw_pointers())
        .function("artboardCount", &rive::File::artboardCount);

    class_<rive::Artboard>("ArtboardBase");
    class_<rive::ArtboardInstance, base<rive::Artboard>>("Artboard")
#ifdef ENABLE_QUERY_FLAT_VERTICES
        .function("flattenPath",
                  optional_override([](rive::Artboard& self,
                                       size_t index,
                                       bool transformToParent) -> rive::FlattenedPath* {
                      auto artboardObjects = self.objects();
                      if (index >= artboardObjects.size())
                      {
                          return nullptr;
                      }
                      auto object = artboardObjects[index];
                      if (!object->is<rive::Path>())
                      {
                          return nullptr;
                      }
                      auto path = object->as<rive::Path>();
                      return path->makeFlat(transformToParent);
                  }),
                  allow_raw_pointers())
#endif
        .property("name", select_overload<const std::string&() const>(&rive::Artboard::name))
        .function("advance",
                  optional_override([](rive::ArtboardInstance& self, double seconds) -> bool {
                      return self.advance(seconds);
                  }),
                  allow_raw_pointers())
        .function("draw",
                  optional_override([](rive::ArtboardInstance& self, rive::Renderer* renderer) {
                      return self.draw(renderer, rive::Artboard::DrawOption::kNormal);
                  }),
                  allow_raw_pointers())
        .function("transformComponent",
                  optional_override([](rive::ArtboardInstance& self, const std::string& name) {
                      return self.find<rive::TransformComponent>(name);
                  }),
                  allow_raw_pointers())
        .function("node",
                  optional_override([](rive::ArtboardInstance& self, const std::string& name) {
                      return self.find<rive::Node>(name);
                  }),
                  allow_raw_pointers())
        .function("bone",
                  optional_override([](rive::ArtboardInstance& self, const std::string& name) {
                      return self.find<rive::Bone>(name);
                  }),
                  allow_raw_pointers())
        .function("rootBone",
                  optional_override([](rive::ArtboardInstance& self, const std::string& name) {
                      return self.find<rive::RootBone>(name);
                  }),
                  allow_raw_pointers())
        // Animations
        .function("animationByIndex",
                  optional_override(
                      [](rive::ArtboardInstance& self, size_t index) -> rive::LinearAnimation* {
                          return self.animation(index);
                      }),
                  allow_raw_pointers())
        .function("animationByName",
                  optional_override([](rive::ArtboardInstance& self, const std::string& name)
                                        -> rive::LinearAnimation* { return self.animation(name); }),
                  allow_raw_pointers())
        .function("animationCount", optional_override([](rive::ArtboardInstance& self) -> size_t {
                      return self.animationCount();
                  }))
        // State machines
        .function("stateMachineByIndex",
                  optional_override(
                      [](rive::ArtboardInstance& self, size_t index) -> rive::StateMachine* {
                          return self.stateMachine(index);
                      }),
                  allow_raw_pointers())
        .function("stateMachineByName",
                  optional_override([](rive::ArtboardInstance& self, const std::string& name)
                                        -> rive::StateMachine* { return self.stateMachine(name); }),
                  allow_raw_pointers())
        .function("stateMachineCount",
                  optional_override([](rive::ArtboardInstance& self) -> size_t {
                      return self.stateMachineCount();
                  }))
        .property("bounds", optional_override([](const rive::ArtboardInstance& self) -> rive::AABB {
                      return self.bounds();
                  }))
        .property("frameOrigin",
                  select_overload<bool() const>(&rive::Artboard::frameOrigin),
                  select_overload<void(bool)>(&rive::Artboard::frameOrigin));

    class_<rive::TransformComponent>("TransformComponent")
        .property("scaleX",
                  select_overload<float() const>(&rive::TransformComponent::scaleX),
                  select_overload<void(float)>(&rive::TransformComponent::scaleX))
        .property("scaleY",
                  select_overload<float() const>(&rive::TransformComponent::scaleY),
                  select_overload<void(float)>(&rive::TransformComponent::scaleY))
        .property("rotation",
                  select_overload<float() const>(&rive::TransformComponent::rotation),
                  select_overload<void(float)>(&rive::TransformComponent::rotation))
        .function("worldTransform",
                  optional_override([](rive::TransformComponent& self) -> rive::Mat2D& {
                      return self.mutableWorldTransform();
                  }),
                  allow_raw_pointers())
        .function(
            "parentWorldTransform",
            optional_override([](rive::TransformComponent& self, rive::Mat2D& result) -> void {
                result = rive::Mat2D(getParentWorld(self));
            }),
            allow_raw_pointers());

    class_<rive::Node, base<rive::TransformComponent>>("Node")
        .property("x",
                  select_overload<float() const>(&rive::TransformComponent::x),
                  select_overload<void(float)>(&rive::Node::x))
        .property("y",
                  select_overload<float() const>(&rive::TransformComponent::y),
                  select_overload<void(float)>(&rive::Node::y));

    class_<rive::Bone, base<rive::TransformComponent>>("Bone").property(
        "length",
        select_overload<float() const>(&rive::Bone::length),
        select_overload<void(float)>(&rive::Bone::length));

    class_<rive::RootBone, base<rive::Bone>>("RootBone")
        .property("x",
                  select_overload<float() const>(&rive::TransformComponent::x),
                  select_overload<void(float)>(&rive::RootBone::x))
        .property("y",
                  select_overload<float() const>(&rive::TransformComponent::y),
                  select_overload<void(float)>(&rive::RootBone::y));

    class_<rive::Animation>("Animation")
        .property("name", select_overload<const std::string&() const>(&rive::AnimationBase::name));

    class_<rive::LinearAnimation, base<rive::Animation>>("LinearAnimation")
        .property("name", select_overload<const std::string&() const>(&rive::AnimationBase::name))
        .property("duration",
                  select_overload<uint32_t() const>(&rive::LinearAnimationBase::duration))
        .property("fps", select_overload<uint32_t() const>(&rive::LinearAnimationBase::fps))
        .property("workStart",
                  select_overload<uint32_t() const>(&rive::LinearAnimationBase::workStart))
        .property("workEnd", select_overload<uint32_t() const>(&rive::LinearAnimationBase::workEnd))
        .property("enableWorkArea",
                  select_overload<bool() const>(&rive::LinearAnimationBase::enableWorkArea))
        .property("loopValue",
                  select_overload<uint32_t() const>(&rive::LinearAnimationBase::loopValue))
        .property("speed", select_overload<float() const>(&rive::LinearAnimationBase::speed))
        .function("apply", &rive::LinearAnimation::apply, allow_raw_pointers());

    class_<rive::LinearAnimationInstance>("LinearAnimationInstance")
        .constructor<rive::LinearAnimation*, rive::ArtboardInstance*>()
        .property("time",
                  select_overload<float() const>(&rive::LinearAnimationInstance::time),
                  select_overload<void(float)>(&rive::LinearAnimationInstance::time))
        .property("didLoop", &rive::LinearAnimationInstance::didLoop)
        .function("advance", &rive::LinearAnimationInstance::advance)
        .function("apply", &rive::LinearAnimationInstance::apply, allow_raw_pointers());

    class_<rive::StateMachine, base<rive::Animation>>("StateMachine");

    class_<rive::StateMachineInstance>("StateMachineInstance")
        .constructor<rive::StateMachine*, rive::ArtboardInstance*>()
        .function("advance", &rive::StateMachineInstance::advance, allow_raw_pointers())
        .function("inputCount", &rive::StateMachineInstance::inputCount)
        .function("input", &rive::StateMachineInstance::input, allow_raw_pointers())
        .function("pointerDown",
                  optional_override([](rive::StateMachineInstance& self, double x, double y) {
                      self.pointerDown(rive::Vec2D((float)x, (float)y));
                  }))
        .function("pointerMove",
                  optional_override([](rive::StateMachineInstance& self, double x, double y) {
                      self.pointerMove(rive::Vec2D((float)x, (float)y));
                  }))
        .function("pointerUp",
                  optional_override([](rive::StateMachineInstance& self, double x, double y) {
                      self.pointerUp(rive::Vec2D((float)x, (float)y));
                  }))
        .function("stateChangedCount", &rive::StateMachineInstance::stateChangedCount)
        .function(
            "stateChangedNameByIndex",
            optional_override([](rive::StateMachineInstance& self, size_t index) -> std::string {
                const rive::LayerState* state = self.stateChangedByIndex(index);
                if (state != nullptr)
                    switch (state->coreType())
                    {
                        case rive::AnimationState::typeKey:
                            return state->as<rive::AnimationState>()->animation()->name();
                        case rive::EntryState::typeKey:
                            return "entry";
                        case rive::ExitState::typeKey:
                            return "exit";
                        case rive::AnyState::typeKey:
                            return "any";
                    }
                return "unknown";
            }),
            allow_raw_pointers());

    class_<rive::SMIInput>("SMIInput")
        .property("type", &rive::SMIInput::inputCoreType)
        .property("name", &rive::SMIInput::name)
        .class_property("bool", &stateMachineBoolTypeKey)
        .class_property("number", &stateMachineNumberTypeKey)
        .class_property("trigger", &stateMachineTriggerTypeKey)
        .function("asBool",
                  optional_override([](rive::SMIInput& self) -> rive::SMIBool* {
                      if (self.inputCoreType() != stateMachineBoolTypeKey)
                      {
                          return nullptr;
                      }
                      return static_cast<rive::SMIBool*>(&self);
                  }),
                  allow_raw_pointers())
        .function("asNumber",
                  optional_override([](rive::SMIInput& self) -> rive::SMINumber* {
                      if (self.inputCoreType() != stateMachineNumberTypeKey)
                      {
                          return nullptr;
                      }
                      return static_cast<rive::SMINumber*>(&self);
                  }),
                  allow_raw_pointers())
        .function("asTrigger",
                  optional_override([](rive::SMIInput& self) -> rive::SMITrigger* {
                      if (self.inputCoreType() != stateMachineTriggerTypeKey)
                      {
                          return nullptr;
                      }
                      return static_cast<rive::SMITrigger*>(&self);
                  }),
                  allow_raw_pointers());

    class_<rive::SMIBool, base<rive::SMIInput>>("SMIBool").property(
        "value",
        select_overload<bool() const>(&rive::SMIBool::value),
        select_overload<void(bool)>(&rive::SMIBool::value));
    class_<rive::SMINumber, base<rive::SMIInput>>("SMINumber")
        .property("value",
                  select_overload<float() const>(&rive::SMINumber::value),
                  select_overload<void(float)>(&rive::SMINumber::value));
    class_<rive::SMITrigger, base<rive::SMIInput>>("SMITrigger")
        .function("fire", &rive::SMITrigger::fire);

    enum_<rive::Fit>("Fit")
        .value("fill", rive::Fit::fill)
        .value("contain", rive::Fit::contain)
        .value("cover", rive::Fit::cover)
        .value("fitWidth", rive::Fit::fitWidth)
        .value("fitHeight", rive::Fit::fitHeight)
        .value("none", rive::Fit::none)
        .value("scaleDown", rive::Fit::scaleDown);

    enum_<JsAlignment>("Alignment")
        .value("topLeft", JsAlignment::topLeft)
        .value("topCenter", JsAlignment::topCenter)
        .value("topRight", JsAlignment::topRight)
        .value("centerLeft", JsAlignment::centerLeft)
        .value("center", JsAlignment::center)
        .value("centerRight", JsAlignment::centerRight)
        .value("bottomLeft", JsAlignment::bottomLeft)
        .value("bottomCenter", JsAlignment::bottomCenter)
        .value("bottomRight", JsAlignment::bottomRight);

    value_object<rive::AABB>("AABB")
        .field("minX", &rive::AABB::minX)
        .field("minY", &rive::AABB::minY)
        .field("maxX", &rive::AABB::maxX)
        .field("maxY", &rive::AABB::maxY);

    // #ifdef DEBUG
    //     function("doLeakCheck", &__lsan_do_recoverable_leak_check);
    // #endif
}
