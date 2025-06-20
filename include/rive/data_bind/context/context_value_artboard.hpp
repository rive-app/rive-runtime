#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_ARTBOARD_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_ARTBOARD_HPP_
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_values/data_value_artboard.hpp"
namespace rive
{
class Artboard;
class DataBindContextValueArtboard : public DataBindContextValue
{

public:
    DataBindContextValueArtboard(DataBind* m_dataBind);
    void apply(Core* component,
               uint32_t propertyKey,
               bool isMainDirection) override;
    bool syncTargetValue(Core* target, uint32_t propertyKey) override;

protected:
    DataValue* targetValue() override { return &m_targetDataValue; }

private:
    uint32_t m_previousValue = -1;
    DataValueArtboard m_targetDataValue;
};
} // namespace rive

#endif