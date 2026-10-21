#include "rive/data_bind/host_data_context_consumer.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style_global.hpp"

using namespace rive;

HostDataContextConsumer* HostDataContextConsumer::from(Core* component)
{
  switch (component->coreType())
  {
    case TextStyleGlobalBase::typeKey:
      return component->as<TextStyleGlobal>();
    case TextBase::typeKey:
      // Its global text styles instanced on demand for list runs.
      return component->as<Text>();
  }
  return nullptr;
}
