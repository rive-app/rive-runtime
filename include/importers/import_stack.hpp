#ifndef _RIVE_IMPORT_STACK_HPP_
#define _RIVE_IMPORT_STACK_HPP_
#include "status_code.hpp"
#include <unordered_map>

namespace rive
{
	class ImportStackObject
	{
	public:
		virtual ~ImportStackObject() {}
		virtual StatusCode resolve() { return StatusCode::Ok; }
	};

	class ImportStack
	{
	private:
		std::unordered_map<uint16_t, ImportStackObject*> m_Latests;

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
				StatusCode code = stackObject->resolve();
				delete stackObject;
				if (code != StatusCode::Ok)
				{
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
	};
} // namespace rive
#endif