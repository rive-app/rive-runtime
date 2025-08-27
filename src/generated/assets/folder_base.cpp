#include "rive/generated/assets/folder_base.hpp"
#include "rive/assets/folder.hpp"

using namespace rive;

Core* FolderBase::clone() const
{
    auto cloned = new Folder();
    cloned->copy(*this);
    return cloned;
}
