#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_TRIGGER_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_TRIGGER_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_trigger.hpp"
namespace rive
{
class DataBindContextValueTrigger : public DataBindContextValue
{

public:
    DataBindContextValueTrigger(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection,
               DataBind* dataBind) override;
    void applyToSource(Core* component,
                       uint32_t propertyKey,
                       bool isMainDirection,
                       DataBind* dataBind) override;

private:
    // A trigger's count only says how often it has fired and is never reset,
    // so a fire crosses this bind as one more fire on the other side rather
    // than as a copy of the count. A copy would fire the other side whenever a
    // bind first meets a count that isn't 0, and miss a fire whenever the two
    // counts happen to match.
    uint32_t m_sourceCount = 0;
    bool m_sourceCountKnown = false;
    bool m_targetCountKnown = false;
};
} // namespace rive

#endif
