#ifndef _RIVE_FORMULA_TOKEN_HPP_
#define _RIVE_FORMULA_TOKEN_HPP_
#include "rive/generated/data_bind/converters/formula/formula_token_base.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/data_bind/data_bind.hpp"
#include <stdio.h>
namespace rive
{
class FormulaToken : public FormulaTokenBase
{
public:
    StatusCode import(ImportStack& importStack) override;

    virtual void bindFromContext(DataContext* dataContext, DataBind* dataBind);
    virtual void update();
    void markDirty();
    void addDataBind(DataBind* dataBind);
    void copy(const FormulaTokenBase& object);
    std::vector<DataBind*> dataBinds() const { return m_dataBinds; }

private:
    std::vector<DataBind*> m_dataBinds;
    DataBind* m_parentDataBind;
};
} // namespace rive

#endif