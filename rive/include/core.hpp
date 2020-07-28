#ifndef _RIVE_CORE_HPP_
#define _RIVE_CORE_HPP_

#include "core/binary_reader.hpp"
#include <cassert>

namespace rive
{
	class CoreContext;
	class Core
	{
	public:
		virtual ~Core() {}
		virtual int coreType() const = 0;
		virtual bool isTypeOf(int typeKey) const = 0;
		virtual bool deserialize(int propertyKey, BinaryReader& reader) = 0;

		template <typename T> bool is() const { return isTypeOf(T::typeKey); }
		template <typename T> T* as()
		{
			assert(is<T>());
			return reinterpret_cast<T*>(this);
		}

		template <typename T> const T* as() const
		{
			assert(is<T>());
			return reinterpret_cast<const T*>(this);
		}

		/// Called when the object is first added to the context, other objects
		/// may not have resolved their dependencies yet. This is an opportunity
		/// to look up objects referenced by id, but not assume that they in
		/// turn have resolved their references yet. Called during
		/// load/instance.
		virtual void onAddedDirty(CoreContext* context) = 0;

		/// Called when all the objects in the context have had onAddedDirty
		/// called. This is an opportunity to reference things referenced by
		/// dependencies. (A path should be able to find a Shape somewhere in
		/// its hierarchy, which may be multiple levels up).
		virtual void onAddedClean(CoreContext* context) = 0;
	};
} // namespace rive
#endif