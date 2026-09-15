#include "rive/assets/script_module_asset.hpp"
#include "rive/signed_content_header.hpp"

using namespace rive;

bool ScriptModuleAsset::decode(SimpleArray<uint8_t>& data, Factory* factory)
{
#ifdef WITH_RIVE_SCRIPTING
    m_verified = false;

    SignedContentHeader header(Span<const uint8_t>(data.data(), data.size()));
    if (!header.isValid())
    {
        return false;
    }

    auto module = header.content();
    m_module = SimpleArray<uint8_t>(module.data(), module.size());
#endif
    return true;
}
