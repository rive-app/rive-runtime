#ifndef _RIVE_IMPORT_STACK_HPP_
#define _RIVE_IMPORT_STACK_HPP_
#include "rive/status_code.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <algorithm>

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
	private:
		std::unordered_map<uint16_t, ImportStackObject*> m_Latests;
		std::vector<ImportStackObject*> m_LastAdded;

	public:
		template <typename T = ImportStackObject> T* latest(uint16_t coreType)
		{
			auto itr = m_Latests.find(coreType);
			if (itr == m_Latests.end())
			{
				return nullptr;
			}
			return reinterpret_cast<T*>(itr->second);
		}

		StatusCode makeLatest(uint16_t coreType, ImportStackObject* object)
		{
			// Clean up the old object in the stack.
			auto itr = m_Latests.find(coreType);
			if (itr != m_Latests.end())
			{
				auto stackObject = itr->second;

				// Remove it from latests.
				auto itr = std::find(
				    m_LastAdded.begin(), m_LastAdded.end(), stackObject);
				if (itr != m_LastAdded.end())
				{
					m_LastAdded.erase(itr);
				}

				StatusCode code = stackObject->resolve();
				delete stackObject;
				if (code != StatusCode::Ok)
				{
					m_Latests.erase(coreType);
					return code;
				}
			}

			// Set the new one.
			if (object == nullptr)
			{
				m_Latests.erase(coreType);
			}
			else
			{
				m_Latests[coreType] = object;
				m_LastAdded.push_back(object);
			}
			return StatusCode::Ok;
		}

		StatusCode resolve()
		{
			for (auto& pair : m_Latests)
			{
				StatusCode code = pair.second->resolve();
				if (code != StatusCode::Ok)
				{
					return code;
				}
			}
			return StatusCode::Ok;
		}

		~ImportStack()
		{
			for (auto& pair : m_Latests)
			{
				delete pair.second;
			}
		}

		bool readNullObject()
		{
			for (auto itr = m_LastAdded.rbegin(); itr != m_LastAdded.rend();
			     itr++)
			{
				if ((*itr)->readNullObject())
				{
					return true;
				}
			}
			return false;
		}
	};
} // namespace rive
#endif