#ifndef _RIVE_DATA_RESOLVER_HPP_
#define _RIVE_DATA_RESOLVER_HPP_
#include <stdio.h>
#include <string>
namespace rive
{
class DataResolver
{
public:
    virtual const std::string& resolveName(int id) = 0;
    virtual const std::vector<uint32_t>& resolvePath(int id) = 0;
};
} // namespace rive

#endif