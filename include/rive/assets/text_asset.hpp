#ifndef _RIVE_TEXT_ASSET_HPP_
#define _RIVE_TEXT_ASSET_HPP_
#include "rive/generated/assets/text_asset_base.hpp"
#include <stdio.h>

namespace rive
{
#ifdef WITH_RIVE_SCRIPTING
class TextAssetImporter;
#endif

/// Abstract base for assets that carry editor-authored source text compiled
/// to a signed in-band payload. Concrete subclasses: ScriptAsset (Luau
/// bytecode), ShaderAsset (RSTB blob). The aggregate-signature machinery in
/// TextAssetImporter is shared across all TextAsset subclasses so a single
/// signature can cover mixed Luau + RSTB content in export order.
class TextAsset : public TextAssetBase
{
#ifdef WITH_RIVE_SCRIPTING
    friend class TextAssetImporter;
#endif

public:
#ifdef WITH_RIVE_SCRIPTING
    /// True once the aggregate in-band signature has verified against this
    /// asset's content. Defaults to false until TextAssetImporter::resolve
    /// runs. Production (non-tools) consumers should gate use of the content
    /// behind this check.
    bool verified() const { return m_verified; }

protected:
    bool m_verified = false;
#endif
};
} // namespace rive

#endif
