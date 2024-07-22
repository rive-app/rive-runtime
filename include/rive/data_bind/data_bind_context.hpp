#ifndef _RIVE_DATA_BIND_CONTEXT_HPP_
#define _RIVE_DATA_BIND_CONTEXT_HPP_
#include "rive/generated/data_bind/data_bind_context_base.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/data_bind/context/context_value.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/refcnt.hpp"
#include <stdio.h>
namespace rive
{
class DataBindContext : public DataBindContextBase
{
protected:
    std::vector<uint32_t> m_SourcePathIdsBuffer;

public:
    void decodeSourcePathIds(Span<const uint8_t> value) override;
    void copySourcePathIds(const DataBindContextBase& object) override;
    void bindFromContext(DataContext* dataContext);
    ViewModelInstanceValue* source() { return m_Source; };
};
} // namespace rive

#endif