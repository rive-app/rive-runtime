#ifndef _RIVE_ENUM_IMPORTER_HPP_
#define _RIVE_ENUM_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class Core;
class DataEnum;
class DataEnumValue;
class EnumImporter : public ImportStackObject
{
private:
    DataEnum* m_DataEnum;

public:
    EnumImporter(DataEnum* dataEnum);
    void addValue(DataEnumValue* object);
    StatusCode resolve() override;
    const DataEnum* dataEnum() const { return m_DataEnum; }
};
} // namespace rive
#endif
