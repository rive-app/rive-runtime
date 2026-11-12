#ifndef _RIVE_KEYED_PROPERTY_IMPORTER_HPP_
#define _RIVE_KEYED_PROPERTY_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class Core;
class KeyFrame;
class KeyedProperty;
class LinearAnimation;
class KeyedPropertyImporter : public ImportStackObject
{
private:
  LinearAnimation* m_Animation;
  KeyedProperty* m_KeyedProperty;

public:
  /// A null [keyedProperty] stands in for the last one of a body the import
  /// deferred (see File::readObjects): it swallows unknown objects as the
  /// real one would, but takes no keyframes.
  KeyedPropertyImporter(
      LinearAnimation* animation,
      KeyedProperty* keyedProperty);
  bool hasKeyedProperty() const { return m_KeyedProperty != nullptr; }
  void addKeyFrame(std::unique_ptr<KeyFrame>);
  bool readNullObject() override;
};
} // namespace rive
#endif
