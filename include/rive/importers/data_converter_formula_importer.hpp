#ifndef _RIVE_DATA_CONVERTER_FORMULA_IMPORTER_HPP_
#define _RIVE_DATA_CONVERTER_FORMULA_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class Core;
class DataConverterFormula;
class DataConverterFormulaImporter : public ImportStackObject
{
private:
    DataConverterFormula* m_dataConverterFormula;

public:
    DataConverterFormulaImporter(DataConverterFormula* formula);
    DataConverterFormula* formula() { return m_dataConverterFormula; }
    StatusCode resolve() override;
};
} // namespace rive
#endif