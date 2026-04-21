#ifndef _RIVE_SEMANTIC_INFERENCE_REGISTRY_HPP_
#define _RIVE_SEMANTIC_INFERENCE_REGISTRY_HPP_

#include "rive/semantic/semantic_provider.hpp"

namespace rive
{
class Component;

bool supportsInferredSemantics(Component* component);
bool resolveInferredSemantics(Component* component, ResolvedSemanticData& out);
} // namespace rive

#endif
