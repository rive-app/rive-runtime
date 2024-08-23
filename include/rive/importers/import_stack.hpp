#ifndef _RIVE_IMPORT_STACK_HPP_
#define _RIVE_IMPORT_STACK_HPP_
#include "rive/status_code.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <memory>

namespace rive
{
class ImportStackObject
{
public:
    virtual ~ImportStackObject() {}
    virtual StatusCode resolve() { return StatusCode::Ok; }
    virtual bool readNullObject() { return false; }
};

class ImportStack
{
public:
    template <typename T = ImportStackObject> T* latest(uint16_t coreType)
    {
        auto itr = m_latests.find(coreType);
        if (itr == m_latests.end())
        {
            return nullptr;
        }
        return static_cast<T*>(itr->second.get());
    }

    StatusCode makeLatest(uint16_t coreType, std::unique_ptr<ImportStackObject> object)
    {
        // Clean up the old object in the stack.
        auto itr = m_latests.find(coreType);
        if (itr != m_latests.end())
        {
            auto stackObject = itr->second.get();

            // Remove it from latests.
            auto lastAddedItr = std::find(m_lastAdded.begin(), m_lastAdded.end(), stackObject);
            if (lastAddedItr != m_lastAdded.end())
            {
                m_lastAdded.erase(lastAddedItr);
            }

            StatusCode code = stackObject->resolve();
            if (code != StatusCode::Ok)
            {
                m_latests.erase(coreType);
                return code;
            }
        }

        // Set the new one.
        if (object == nullptr)
        {
            m_latests.erase(coreType);
        }
        else
        {
            m_lastAdded.push_back(object.get());
            m_latests[coreType] = std::move(object);
        }
        return StatusCode::Ok;
    }

    StatusCode resolve()
    {
        StatusCode returnCode = StatusCode::Ok;

        // Reverse iterate the last added import stack objects and resolve them.
        // If any don't resolve, capture the return code and stop resolving.
        for (auto itr = m_lastAdded.rbegin(); itr != m_lastAdded.rend(); itr++)
        {
            StatusCode code = (*itr)->resolve();
            if (code != StatusCode::Ok)
            {
                returnCode = code;
                break;
            }
        }

        m_latests.clear();
        m_lastAdded.clear();

        return returnCode;
    }

    bool readNullObject()
    {
        for (auto itr = m_lastAdded.rbegin(); itr != m_lastAdded.rend(); itr++)
        {
            if ((*itr)->readNullObject())
            {
                return true;
            }
        }
        return false;
    }

private:
    std::unordered_map<uint16_t, std::unique_ptr<ImportStackObject>> m_latests;
    std::vector<ImportStackObject*> m_lastAdded;
};
} // namespace rive
#endif
