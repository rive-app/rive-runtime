#include "semantic_inference_registry.hpp"

#include "rive/component.hpp"
#include "rive/generated/text/text_base.hpp"
#include "rive/semantic/semantic_role.hpp"
#include "rive/text/text.hpp"

namespace rive
{
#ifdef WITH_RIVE_TEXT
namespace
{

using InferSemanticFn = bool (*)(Component*, ResolvedSemanticData&);

struct InferenceRule
{
    uint16_t typeKey;
    InferSemanticFn infer;
};

static std::string inferredTextLabel(Text* text)
{
    std::string label;
    for (auto run : text->runs())
    {
        if (run != nullptr)
        {
            label += run->text();
        }
    }
    return label;
}

static bool inferTextSemantics(Component* component, ResolvedSemanticData& out)
{
    const auto label = inferredTextLabel(component->as<Text>());
    if (label.empty())
    {
        return false;
    }

    out.hasSemantics = true;
    out.role = static_cast<uint32_t>(SemanticRole::text);
    out.label = label;
    return true;
}

static constexpr InferenceRule kInferenceRules[] = {
    {TextBase::typeKey, inferTextSemantics},
};

} // namespace

bool supportsInferredSemantics(Component* component)
{
    if (component == nullptr)
    {
        return false;
    }
    for (const auto& rule : kInferenceRules)
    {
        if (component->isTypeOf(rule.typeKey))
        {
            return true;
        }
    }
    return false;
}

bool resolveInferredSemantics(Component* component, ResolvedSemanticData& out)
{
    if (component == nullptr)
    {
        return false;
    }
    for (const auto& rule : kInferenceRules)
    {
        if (component->isTypeOf(rule.typeKey) && rule.infer(component, out))
        {
            return true;
        }
    }
    return false;
}
#else
// No inference rules are active when the text engine is compiled out —
// Text is the only current rule and it needs Text::runs().
bool supportsInferredSemantics(Component*) { return false; }

bool resolveInferredSemantics(Component*, ResolvedSemanticData&)
{
    return false;
}
#endif

} // namespace rive
