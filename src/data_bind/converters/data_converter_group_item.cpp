#include "rive/backboard.hpp"
#include "rive/math/math_types.hpp"
#include "rive/data_bind/converters/data_converter_group_item.hpp"
#include "rive/data_bind/converters/data_converter_group.hpp"
#include "rive/importers/data_converter_group_importer.hpp"
#include "rive/importers/backboard_importer.hpp"

using namespace rive;

StatusCode DataConverterGroupItem::import(ImportStack& importStack)
{
    auto backboardImporter = importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    backboardImporter->addDataConverterGroupItemReferencer(this);
    auto dataConveterGroupImporter =
        importStack.latest<DataConverterGroupImporter>(DataConverterGroupBase::typeKey);
    if (dataConveterGroupImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    dataConveterGroupImporter->group()->addItem(this);
    return Super::import(importStack);
}