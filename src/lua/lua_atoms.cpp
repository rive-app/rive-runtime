#ifdef WITH_RIVE_SCRIPTING
// The LuaAtoms name table and lookup, shared by the native runtime and the
// wasm script module build; both install it as the state's useratom callback
// so namecall dispatch and direct field getters agree everywhere.
#include "rive/lua/rive_lua_libs.hpp"

#include <array>
#include <string_view>

using namespace rive;

namespace
{
struct LuaAtomName
{
    std::string_view name;
    int16_t atom;
};

constexpr LuaAtomName atoms[] = {
    {"length", (int16_t)LuaAtoms::length},
    {"lengthSquared", (int16_t)LuaAtoms::lengthSquared},
    {"normalized", (int16_t)LuaAtoms::normalized},
    {"distance", (int16_t)LuaAtoms::distance},
    {"distanceSquared", (int16_t)LuaAtoms::distanceSquared},
    {"dot", (int16_t)LuaAtoms::dot},
    {"lerp", (int16_t)LuaAtoms::lerp},
    {"moveTo", (int16_t)LuaAtoms::moveTo},
    {"lineTo", (int16_t)LuaAtoms::lineTo},
    {"quadTo", (int16_t)LuaAtoms::quadTo},
    {"cubicTo", (int16_t)LuaAtoms::cubicTo},
    {"close", (int16_t)LuaAtoms::close},
    {"type", (int16_t)LuaAtoms::type},
    {"reset", (int16_t)LuaAtoms::reset},
    {"add", (int16_t)LuaAtoms::add},
    {"contours", (int16_t)LuaAtoms::contours},
    {"measure", (int16_t)LuaAtoms::measure},
    {"invert", (int16_t)LuaAtoms::invert},
    {"isIdentity", (int16_t)LuaAtoms::isIdentity},
    {"width", (int16_t)LuaAtoms::width},
    {"height", (int16_t)LuaAtoms::height},
    {"clamp", (int16_t)LuaAtoms::clamp},
    {"repeat", (int16_t)LuaAtoms::repeat},
    {"mirror", (int16_t)LuaAtoms::mirror},
    {"bilinear", (int16_t)LuaAtoms::bilinear},
    {"nearest", (int16_t)LuaAtoms::nearest},
    {"style", (int16_t)LuaAtoms::style},
    {"join", (int16_t)LuaAtoms::join},
    {"cap", (int16_t)LuaAtoms::cap},
    {"thickness", (int16_t)LuaAtoms::thickness},
    {"blendMode", (int16_t)LuaAtoms::blendMode},
    {"feather", (int16_t)LuaAtoms::feather},
    {"gradient", (int16_t)LuaAtoms::gradient},
    {"color", (int16_t)LuaAtoms::color},
    {"stroke", (int16_t)LuaAtoms::stroke},
    {"fill", (int16_t)LuaAtoms::fill},
    {"miter", (int16_t)LuaAtoms::miter},
    {"round", (int16_t)LuaAtoms::round},
    {"bevel", (int16_t)LuaAtoms::bevel},
    {"butt", (int16_t)LuaAtoms::butt},
    {"square", (int16_t)LuaAtoms::square},
    {"srcOver", (int16_t)LuaAtoms::srcOver},
    {"screen", (int16_t)LuaAtoms::screen},
    {"overlay", (int16_t)LuaAtoms::overlay},
    {"darken", (int16_t)LuaAtoms::darken},
    {"lighten", (int16_t)LuaAtoms::lighten},
    {"colorDodge", (int16_t)LuaAtoms::colorDodge},
    {"colorBurn", (int16_t)LuaAtoms::colorBurn},
    {"hardLight", (int16_t)LuaAtoms::hardLight},
    {"softLight", (int16_t)LuaAtoms::softLight},
    {"difference", (int16_t)LuaAtoms::difference},
    {"exclusion", (int16_t)LuaAtoms::exclusion},
    {"multiply", (int16_t)LuaAtoms::multiply},
    {"hue", (int16_t)LuaAtoms::hue},
    {"saturation", (int16_t)LuaAtoms::saturation},
    {"luminosity", (int16_t)LuaAtoms::luminosity},
    {"copy", (int16_t)LuaAtoms::copy},
    {"drawPath", (int16_t)LuaAtoms::drawPath},
    {"drawImage", (int16_t)LuaAtoms::drawImage},
    {"drawImageMesh", (int16_t)LuaAtoms::drawImageMesh},
    {"clipPath", (int16_t)LuaAtoms::clipPath},
    {"save", (int16_t)LuaAtoms::save},
    {"restore", (int16_t)LuaAtoms::restore},
    {"transform", (int16_t)LuaAtoms::transform},
    {"value", (int16_t)LuaAtoms::value},
    {"red", (int16_t)LuaAtoms::red},
    {"green", (int16_t)LuaAtoms::green},
    {"blue", (int16_t)LuaAtoms::blue},
    {"alpha", (int16_t)LuaAtoms::alpha},
    {"getNumber", (int16_t)LuaAtoms::getNumber},
    {"getTrigger", (int16_t)LuaAtoms::getTrigger},
    {"getString", (int16_t)LuaAtoms::getString},
    {"getBoolean", (int16_t)LuaAtoms::getBoolean},
    {"getColor", (int16_t)LuaAtoms::getColor},
    {"getList", (int16_t)LuaAtoms::getList},
    {"getViewModel", (int16_t)LuaAtoms::getViewModel},
    {"getEnum", (int16_t)LuaAtoms::getEnum},
    {"getIndex", (int16_t)LuaAtoms::getIndex},
    {"getImage", (int16_t)LuaAtoms::getImage},
    {"getFont", (int16_t)LuaAtoms::getFont},
    {"getBlob", (int16_t)LuaAtoms::getBlob},
    {"values", (int16_t)LuaAtoms::values},
    {"addListener", (int16_t)LuaAtoms::addListener},
    {"removeListener", (int16_t)LuaAtoms::removeListener},
    {"fire", (int16_t)LuaAtoms::fire},
    {"push", (int16_t)LuaAtoms::push},
    {"insert", (int16_t)LuaAtoms::insert},
    {"pop", (int16_t)LuaAtoms::pop},
    {"swap", (int16_t)LuaAtoms::swap},
    {"shift", (int16_t)LuaAtoms::shift},
    {"clear", (int16_t)LuaAtoms::clear},
    {"draw", (int16_t)LuaAtoms::draw},
    {"advance", (int16_t)LuaAtoms::advance},
    {"frameOrigin", (int16_t)LuaAtoms::frameOrigin},
    {"data", (int16_t)LuaAtoms::data},
    {"instance", (int16_t)LuaAtoms::instance},
    {"animation", (int16_t)LuaAtoms::animation},
    {"new", (int16_t)LuaAtoms::newAtom},
    {"bounds", (int16_t)LuaAtoms::bounds},
    {"pointerDown", (int16_t)LuaAtoms::pointerDown},
    {"pointerUp", (int16_t)LuaAtoms::pointerUp},
    {"pointerMove", (int16_t)LuaAtoms::pointerMove},
    {"pointerExit", (int16_t)LuaAtoms::pointerExit},
    {"isNumber", (int16_t)LuaAtoms::isNumber},
    {"isString", (int16_t)LuaAtoms::isString},
    {"isBoolean", (int16_t)LuaAtoms::isBoolean},
    {"isColor", (int16_t)LuaAtoms::isColor},
    {"hit", (int16_t)LuaAtoms::hit},
    {"id", (int16_t)LuaAtoms::id},
    {"position", (int16_t)LuaAtoms::position},
    {"rotation", (int16_t)LuaAtoms::rotation},
    {"scale", (int16_t)LuaAtoms::scale},
    {"worldTransform", (int16_t)LuaAtoms::worldTransform},
    {"scaleX", (int16_t)LuaAtoms::scaleX},
    {"scaleY", (int16_t)LuaAtoms::scaleY},
    {"decompose", (int16_t)LuaAtoms::decompose},
    {"children", (int16_t)LuaAtoms::children},
    {"parent", (int16_t)LuaAtoms::parent},
    {"node", (int16_t)LuaAtoms::node},
    {"paint", (int16_t)LuaAtoms::paint},
    {"asPath", (int16_t)LuaAtoms::asPath},
    {"asPaint", (int16_t)LuaAtoms::asPaint},
    {"addToPath", (int16_t)LuaAtoms::addToPath},
    {"positionAndTangent", (int16_t)LuaAtoms::positionAndTangent},
    {"warp", (int16_t)LuaAtoms::warp},
    {"extract", (int16_t)LuaAtoms::extract},
    {"next", (int16_t)LuaAtoms::next},
    {"isClosed", (int16_t)LuaAtoms::isClosed},
    {"markNeedsUpdate", (int16_t)LuaAtoms::markNeedsUpdate},
    {"viewModel", (int16_t)LuaAtoms::viewModel},
    {"rootViewModel", (int16_t)LuaAtoms::rootViewModel},
    {"globalViewModel", (int16_t)LuaAtoms::globalViewModel},
    {"globalViewModelNames", (int16_t)LuaAtoms::globalViewModelNames},
    {"dataContext", (int16_t)LuaAtoms::dataContext},
    {"image", (int16_t)LuaAtoms::image},
    {"blob", (int16_t)LuaAtoms::blob},
    {"size", (int16_t)LuaAtoms::size},
    {"name", (int16_t)LuaAtoms::name},
    {"duration", (int16_t)LuaAtoms::duration},
    {"setTime", (int16_t)LuaAtoms::setTime},
    {"setTimeFrames", (int16_t)LuaAtoms::setTimeFrames},
    {"setTimePercentage", (int16_t)LuaAtoms::setTimePercentage},
    {"isPointerEvent", (int16_t)LuaAtoms::isPointerEvent},
    {"isKeyboardEvent", (int16_t)LuaAtoms::isKeyboardEvent},
    {"isTextInput", (int16_t)LuaAtoms::isTextInput},
    {"previousPosition", (int16_t)LuaAtoms::previousPosition},
    {"timeStamp", (int16_t)LuaAtoms::timeStamp},
    {"isFocus", (int16_t)LuaAtoms::isFocus},
    {"isReportedEvent", (int16_t)LuaAtoms::isReportedEvent},
    {"isViewModelChange", (int16_t)LuaAtoms::isViewModelChange},
    {"isNone", (int16_t)LuaAtoms::isNone},
    {"isGamepadConnected", (int16_t)LuaAtoms::isGamepadConnected},
    {"isGamepadEvent", (int16_t)LuaAtoms::isGamepadEvent},
    {"isGamepadDisconnected", (int16_t)LuaAtoms::isGamepadDisconnected},
    {"asPointerEvent", (int16_t)LuaAtoms::asPointerEvent},
    {"asKeyboardEvent", (int16_t)LuaAtoms::asKeyboardEvent},
    {"asTextInput", (int16_t)LuaAtoms::asTextInput},
    {"asFocus", (int16_t)LuaAtoms::asFocus},
    {"asReportedEvent", (int16_t)LuaAtoms::asReportedEvent},
    {"asViewModelChange", (int16_t)LuaAtoms::asViewModelChange},
    {"asGamepadConnected", (int16_t)LuaAtoms::asGamepadConnected},
    {"asGamepadEvent", (int16_t)LuaAtoms::asGamepadEvent},
    {"asGamepadDisconnected", (int16_t)LuaAtoms::asGamepadDisconnected},
    {"gamepadEvent", (int16_t)LuaAtoms::gamepadEvent},
    {"gamepadConnected", (int16_t)LuaAtoms::gamepadConnected},
    {"gamepadDisconnected", (int16_t)LuaAtoms::gamepadDisconnected},
    {"asNone", (int16_t)LuaAtoms::asNone},
    {"key", (int16_t)LuaAtoms::key},
    {"shift", (int16_t)LuaAtoms::shift},
    {"alt", (int16_t)LuaAtoms::alt},
    {"control", (int16_t)LuaAtoms::control},
    {"meta", (int16_t)LuaAtoms::meta},
    {"text", (int16_t)LuaAtoms::text},
    {"phase", (int16_t)LuaAtoms::phase},
    {"delaySeconds", (int16_t)LuaAtoms::delaySeconds},
    {"deviceId", (int16_t)LuaAtoms::deviceId},
    {"buttonMask", (int16_t)LuaAtoms::buttonMask},
    {"remove", (int16_t)LuaAtoms::remove},
    {"removeAt", (int16_t)LuaAtoms::removeAt},
    {"removeAllOf", (int16_t)LuaAtoms::removeAllOf},
    {"axes", (int16_t)LuaAtoms::axes},
    {"gamepadMapping", (int16_t)LuaAtoms::gamepadMapping},
    {"mapping", (int16_t)LuaAtoms::mapping},
    {"isStandardMapping", (int16_t)LuaAtoms::isStandardMapping},
    {"buttons", (int16_t)LuaAtoms::buttons},
    {"buttonPressed", (int16_t)LuaAtoms::buttonPressed},
    {"buttonValue", (int16_t)LuaAtoms::buttonValue},
    {"axis", (int16_t)LuaAtoms::axis},
    {"west", (int16_t)LuaAtoms::west},
    {"south", (int16_t)LuaAtoms::south},
    {"north", (int16_t)LuaAtoms::north},
    {"east", (int16_t)LuaAtoms::east},
    {"leftShoulder", (int16_t)LuaAtoms::leftShoulder},
    {"rightShoulder", (int16_t)LuaAtoms::rightShoulder},
    {"back", (int16_t)LuaAtoms::gamepadBack},
    {"forward", (int16_t)LuaAtoms::gamepadForward},
    {"leftStickButton", (int16_t)LuaAtoms::leftStickButton},
    {"rightStickButton", (int16_t)LuaAtoms::rightStickButton},
    {"dpadUp", (int16_t)LuaAtoms::dpadUp},
    {"dpadDown", (int16_t)LuaAtoms::dpadDown},
    {"dpadLeft", (int16_t)LuaAtoms::dpadLeft},
    {"dpadRight", (int16_t)LuaAtoms::dpadRight},
    {"start", (int16_t)LuaAtoms::start},
    {"leftStick", (int16_t)LuaAtoms::leftStick},
    {"rightStick", (int16_t)LuaAtoms::rightStick},
    {"leftTrigger", (int16_t)LuaAtoms::leftTrigger},
    {"rightTrigger", (int16_t)LuaAtoms::rightTrigger},
    {"leftTriggerPressed", (int16_t)LuaAtoms::leftTriggerPressed},
    {"rightTriggerPressed", (int16_t)LuaAtoms::rightTriggerPressed},
    {"changeKind", (int16_t)LuaAtoms::changeKind},
    {"changeIndex", (int16_t)LuaAtoms::changeIndex},
    {"changeValue", (int16_t)LuaAtoms::changeValue},
    {"hasStandardButtonIntent", (int16_t)LuaAtoms::hasStandardButtonIntent},
    {"hasStandardAxisIntent", (int16_t)LuaAtoms::hasStandardAxisIntent},
    {"intentButton", (int16_t)LuaAtoms::intentButton},
    {"intentAxis", (int16_t)LuaAtoms::intentAxis},
    {"audio", (int16_t)LuaAtoms::audio},
    {"play", (int16_t)LuaAtoms::play},
    {"playAtTime", (int16_t)LuaAtoms::playAtTime},
    {"playInTime", (int16_t)LuaAtoms::playInTime},
    {"playAtFrame", (int16_t)LuaAtoms::playAtFrame},
    {"playInFrame", (int16_t)LuaAtoms::playInFrame},
    {"stop", (int16_t)LuaAtoms::stop},
    {"pause", (int16_t)LuaAtoms::pause},
    {"resume", (int16_t)LuaAtoms::resume},
    {"seek", (int16_t)LuaAtoms::seek},
    {"seekFrame", (int16_t)LuaAtoms::seekFrame},
    {"volume", (int16_t)LuaAtoms::volume},
    {"completed", (int16_t)LuaAtoms::completed},
    {"time", (int16_t)LuaAtoms::time},
    {"timeFrame", (int16_t)LuaAtoms::timeFrame},
    {"sampleRate", (int16_t)LuaAtoms::sampleRate},
    // GPU
    {"write", (int16_t)LuaAtoms::write},
    {"upload", (int16_t)LuaAtoms::upload},
    {"view", (int16_t)LuaAtoms::view},
    {"setPipeline", (int16_t)LuaAtoms::setPipeline},
    {"setVertexBuffer", (int16_t)LuaAtoms::setVertexBuffer},
    {"setIndexBuffer", (int16_t)LuaAtoms::setIndexBuffer},
    {"setBindGroup", (int16_t)LuaAtoms::setBindGroup},
    {"setViewport", (int16_t)LuaAtoms::setViewport},
    {"setScissorRect", (int16_t)LuaAtoms::setScissorRect},
    {"setStencilReference", (int16_t)LuaAtoms::setStencilReference},
    {"drawIndexed", (int16_t)LuaAtoms::drawIndexed},
    {"finish", (int16_t)LuaAtoms::finish},
    {"beginRenderPass", (int16_t)LuaAtoms::beginRenderPass},
    {"beginFrame", (int16_t)LuaAtoms::beginFrame},
    {"endFrame", (int16_t)LuaAtoms::endFrame},
    {"colorView", (int16_t)LuaAtoms::colorView},
    {"depthView", (int16_t)LuaAtoms::depthView},
    {"setBlendColor", (int16_t)LuaAtoms::setBlendColor},
    {"resize", (int16_t)LuaAtoms::resize},
    {"canvas", (int16_t)LuaAtoms::canvas},
    {"gpuCanvas", (int16_t)LuaAtoms::gpuCanvas},
    {"features", (int16_t)LuaAtoms::features},
    {"shader", (int16_t)LuaAtoms::shader},
    {"format", (int16_t)LuaAtoms::format},
    {"andThen", (int16_t)LuaAtoms::andThen},
    {"catch", (int16_t)LuaAtoms::catch_},
    {"finally", (int16_t)LuaAtoms::finally_},
    {"cancel", (int16_t)LuaAtoms::cancel},
    {"onCancel", (int16_t)LuaAtoms::onCancel},
    {"getStatus", (int16_t)LuaAtoms::getStatus},
    {"decodeImage", (int16_t)LuaAtoms::decodeImage},
    // Mat4
    {"transpose", (int16_t)LuaAtoms::transpose},
    {"transformPoint", (int16_t)LuaAtoms::transformPoint},
    {"transformVec4", (int16_t)LuaAtoms::transformVec4},
    {"writeToBuffer", (int16_t)LuaAtoms::writeToBuffer},
    {"invertAffine", (int16_t)LuaAtoms::invertAffine},
    // Vector
    {"writeVec4", (int16_t)LuaAtoms::writeVec4},
};

constexpr size_t atomCount = std::size(atoms);
// Power of two so the modulo is a mask, and roomy enough to keep probes short.
constexpr size_t atomSlotCount = 1024;
static_assert(atomCount < atomSlotCount,
              "atomSlotCount must exceed the number of atoms");

constexpr uint32_t hashAtomName(std::string_view name)
{
    uint32_t hash = 2166136261u;
    for (char c : name)
    {
        hash = (hash ^ (uint8_t)c) * 16777619u;
    }
    return hash;
}

constexpr size_t longestAtomName()
{
    size_t longest = 0;
    for (const LuaAtomName& entry : atoms)
    {
        if (entry.name.size() > longest)
        {
            longest = entry.name.size();
        }
    }
    return longest;
}
constexpr size_t maxAtomNameLength = longestAtomName();

// Slots hold an index into atoms biased by one so zero reads as empty.
constexpr std::array<uint16_t, atomSlotCount> buildAtomSlots()
{
    std::array<uint16_t, atomSlotCount> slots{};
    for (size_t i = 0; i < atomCount; i++)
    {
        size_t slot = hashAtomName(atoms[i].name) & (atomSlotCount - 1);
        while (slots[slot] != 0 && atoms[slots[slot] - 1].name != atoms[i].name)
        {
            slot = (slot + 1) & (atomSlotCount - 1);
        }
        if (slots[slot] == 0)
        {
            slots[slot] = (uint16_t)(i + 1);
        }
    }
    return slots;
}
constexpr std::array<uint16_t, atomSlotCount> atomSlots = buildAtomSlots();

int16_t findAtom(const char* chars, size_t length)
{
    if (length > maxAtomNameLength)
    {
        return -1;
    }
    std::string_view name(chars, length);
    size_t slot = hashAtomName(name) & (atomSlotCount - 1);
    for (uint16_t index = atomSlots[slot]; index != 0;
         index = atomSlots[slot = (slot + 1) & (atomSlotCount - 1)])
    {
        if (atoms[index - 1].name == name)
        {
            return atoms[index - 1].atom;
        }
    }
    return -1;
}
} // namespace

namespace rive
{
int16_t rive_lua_findAtom(const char* chars, size_t length)
{
    return findAtom(chars, length);
}
} // namespace rive
#endif
