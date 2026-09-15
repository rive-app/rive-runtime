#ifndef _RIVE_TEXT_ASSET_HPP_
#define _RIVE_TEXT_ASSET_HPP_
#include "rive/generated/assets/text_asset_base.hpp"
#include <stdio.h>

namespace rive
{
/// Abstract base for assets backed by a single editor CodeFile, compiled to
/// a signed in-band payload. Concrete subclasses: ScriptAsset (Luau
/// bytecode), ShaderAsset (RSTB blob), TextBlobAsset. The signature state
/// and the shared TextAssetImporter live on FileAsset, so export-only
/// payloads with no CodeFile backing (ScriptModuleAsset) participate too.
class TextAsset : public TextAssetBase
{};
} // namespace rive

#endif
