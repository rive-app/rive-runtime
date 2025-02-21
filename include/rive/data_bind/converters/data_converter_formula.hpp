#ifndef _RIVE_DATA_CONVERTER_FORMULA_HPP_
#define _RIVE_DATA_CONVERTER_FORMULA_HPP_
#include "rive/generated/data_bind/converters/data_converter_formula_base.hpp"
#include "rive/data_bind/converters/formula/formula_token.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include <stdio.h>
#include <unordered_map>
namespace rive
{

class DataConverterFormula : public DataConverterFormulaBase
{
public:
    ~DataConverterFormula();
    DataType outputType() override { return DataType::number; };
    void addToken(FormulaToken*);
    void addOutputToken(FormulaToken*, int);
    void initialize();
    void isInstance(bool value) { m_isInstance = value; }

protected:
    DataValue* convert(DataValue* value, DataBind* dataBind) override;
    DataValue* reverseConvert(DataValue* value, DataBind* dataBind) override;
    DataValueNumber m_output;
    Core* clone() const override;
    void bindFromContext(DataContext* dataContext, DataBind* dataBind) override;
    void update() override;

private:
    int getPrecedence(FormulaToken*);
    float getRandom(int);
    float applyOperation(float left, float right, int operationType);
    float applyFunction(std::vector<float>& stack,
                        int functionTypeIndex,
                        int totalArguments);
    std::vector<FormulaToken*> m_tokens;
    std::vector<FormulaToken*> m_outputQueue;
    std::vector<float> m_randoms;
    std::unordered_map<FormulaToken*, int> m_argumentsCount;
    bool m_isInstance = false;
};
} // namespace rive

#endif