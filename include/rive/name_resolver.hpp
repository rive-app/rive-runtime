#ifndef _RIVE_NAME_RESOLVER_HPP_
#define _RIVE_NAME_RESOLVER_HPP_
#include <stdio.h>
#include <string>
namespace rive
{
class NameResolver
{
public:
    virtual const std::string& resolveName(int id) = 0;
};
} // namespace rive

#endif