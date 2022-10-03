#ifndef _RIVE_LINEAR_ANIMATION_IMPORTER_HPP_
#define _RIVE_LINEAR_ANIMATION_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class Core;
class LinearAnimation;
class KeyedObject;
class LinearAnimationImporter : public ImportStackObject
{
private:
    LinearAnimation* m_Animation;

public:
    LinearAnimation* animation() const { return m_Animation; }
    LinearAnimationImporter(LinearAnimation* animation);
    void addKeyedObject(std::unique_ptr<KeyedObject>);
};
} // namespace rive
#endif
