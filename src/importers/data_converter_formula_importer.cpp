#include "rive/artboard.hpp"
#include "rive/importers/data_converter_formula_importer.hpp"
#include "rive/data_bind/converters/data_converter_formula.hpp"

using namespace rive;

DataConverterFormulaImporter::DataConverterFormulaImporter(
    DataConverterFormula* formula) :
    m_dataConverterFormula(formula)
{}

StatusCode DataConverterFormulaImporter::resolve()
{
    m_dataConverterFormula->initialize();
    return StatusCode::Ok;
}