#include "rive/viewmodel/write_attribution.hpp"

#ifdef WITH_RIVE_TOOLS
namespace rive
{
bool WriteAttribution::s_enabled = false;
thread_local std::vector<WriteSource> WriteAttribution::s_sources;
} // namespace rive
#endif
