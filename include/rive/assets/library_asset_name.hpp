#ifndef _RIVE_LIBRARY_ASSET_NAME_HPP_
#define _RIVE_LIBRARY_ASSET_NAME_HPP_

#include <cstddef>
#include <string_view>

namespace rive
{
// Whether registered is path in the library scripts call label. A library's
// assets register as <label>[#<digits>]@<digits>/<path>, so a name says which
// library holds it.
inline bool isLibraryAssetName(
    std::string_view registered,
    std::string_view label,
    std::string_view path)
{
  if (registered.size() <= label.size() ||
      registered.substr(0, label.size()) != label)
  {
    return false;
  }
  size_t i = label.size();
  if (registered[i] == '#')
  {
    size_t start = ++i;
    while (i < registered.size() && registered[i] >= '0' &&
           registered[i] <= '9')
    {
      i++;
    }
    if (i == start)
    {
      return false;
    }
  }
  if (i >= registered.size() || registered[i] != '@')
  {
    return false;
  }
  size_t start = ++i;
  while (i < registered.size() && registered[i] >= '0' && registered[i] <= '9')
  {
    i++;
  }
  if (i == start || i >= registered.size() || registered[i] != '/')
  {
    return false;
  }
  return registered.substr(i + 1) == path;
}

inline bool isLibraryNumber(std::string_view text)
{
  for (char c : text)
  {
    if (c < '0' || c > '9')
    {
      return false;
    }
  }
  return !text.empty();
}

// The <label>[#<digits>]@<digits> a library's asset name starts with, none
// for the project's.
inline std::string_view libraryAssetPrefix(std::string_view name)
{
  size_t slash = name.find('/');
  std::string_view folder = name.substr(0, slash);
  size_t label = folder.find_first_of("#@");
  size_t version = folder.rfind('@');
  if (slash == std::string_view::npos || label == 0 ||
      version == std::string_view::npos ||
      !isLibraryNumber(folder.substr(version + 1)))
  {
    return {};
  }
  std::string_view id = folder.substr(label, version - label);
  return id.empty() || (id[0] == '#' && isLibraryNumber(id.substr(1)))
             ? folder
             : std::string_view();
}
} // namespace rive

#endif
