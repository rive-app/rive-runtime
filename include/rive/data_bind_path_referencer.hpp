#ifndef _RIVE_DATA_BIND_PATH_REFERENCER_HPP_
#define _RIVE_DATA_BIND_PATH_REFERENCER_HPP_
#include <stdio.h>
#include "rive/data_bind/data_bind_path.hpp"
namespace rive
{
class File;
class DataBindPathReferencer
{
public:
    ~DataBindPathReferencer();
    DataBindPath* dataBindPath() const { return m_dataBindPath; }

    void copyDataBindPath(DataBindPath* dataBindPath);
    void importDataBindPath(ImportStack& importStack);
    void decodeDataBindPath(Span<const uint8_t>& value);

protected:
    DataBindPath* m_dataBindPath = nullptr;
};
} // namespace rive

#endif