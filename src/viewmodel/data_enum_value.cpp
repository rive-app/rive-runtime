#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/data_enum.hpp"
#include "rive/viewmodel/data_enum_value.hpp"
#include "rive/importers/enum_importer.hpp"

using namespace rive;

StatusCode DataEnumValue::import(ImportStack& importStack)
{
    auto enumImporter = importStack.latest<EnumImporter>(DataEnum::typeKey);
    if (enumImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }

    enumImporter->addValue(this);
    return Super::import(importStack);
}