#ifndef _RIVE_CORE_CONTEXT_HPP_
#define _RIVE_CORE_CONTEXT_HPP_

namespace rive
{
	class Core;
	class CoreContext
	{
	public:
		virtual Core* resolve(int id) const = 0;
	};
} // namespace rive
#endif