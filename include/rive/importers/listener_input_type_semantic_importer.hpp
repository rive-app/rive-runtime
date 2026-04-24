#ifndef _RIVE_LISTENER_INPUT_TYPE_SEMANTIC_IMPORTER_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_SEMANTIC_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class ListenerInputTypeSemantic;

class ListenerInputTypeSemanticImporter : public ImportStackObject
{
private:
    ListenerInputTypeSemantic* m_listenerInputTypeSemantic;

public:
    explicit ListenerInputTypeSemanticImporter(
        ListenerInputTypeSemantic* listenerInputTypeSemantic);
    ListenerInputTypeSemantic* listenerInputTypeSemantic() const
    {
        return m_listenerInputTypeSemantic;
    }
    StatusCode resolve() override;
};
} // namespace rive

#endif
