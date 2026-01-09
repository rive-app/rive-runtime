#ifndef _RIVE_DATA_BIND_PATH_HPP_
#define _RIVE_DATA_BIND_PATH_HPP_
#include "rive/generated/data_bind/data_bind_path_base.hpp"
#include <stdio.h>
namespace rive
{
class File;
class DataBindPath : public DataBindPathBase
{
public:
    void decodePath(Span<const uint8_t> value) override;
    void copyPath(const DataBindPathBase& object) override;
    StatusCode import(ImportStack& importStack) override;
    std::vector<uint32_t>& path() { return m_pathBuffer; }
    const std::vector<uint32_t>& resolvedPath();
    void file(File* value);
    File* file() { return m_file; };
    void resolved(bool value) { m_resolved = value; }

protected:
    std::vector<uint32_t> m_pathBuffer;

private:
    File* m_file = nullptr;
    bool m_resolved = false;
};
} // namespace rive

#endif