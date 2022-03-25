#ifndef _RIVE_KEYED_PROPERTY_IMPORTER_HPP_
#define _RIVE_KEYED_PROPERTY_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive {
    class Core;
    class KeyFrame;
    class KeyedProperty;
    class LinearAnimation;
    class KeyedPropertyImporter : public ImportStackObject {
    private:
        LinearAnimation* m_Animation;
        KeyedProperty* m_KeyedProperty;

    public:
        KeyedPropertyImporter(LinearAnimation* animation, KeyedProperty* keyedProperty);
        void addKeyFrame(KeyFrame* keyFrame);
        bool readNullObject() override;
    };
} // namespace rive
#endif
