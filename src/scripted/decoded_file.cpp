#include "rive/scripted/decoded_file.hpp"
#include "rive/bindable_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"

using namespace rive;

rcp<File> rive::importDecodedFile(Span<const uint8_t> bytes,
                                  Factory* factory,
                                  ImportResult* result)
{
    auto file = File::import(bytes,
                             factory,
                             result,
                             nullptr,
                             nullptr,
                             /*requireSignedScripts*/ true);
#if defined(__EMSCRIPTEN__) && defined(WITH_RIVE_SCRIPTING_WASM)
    // No host awaits a prepare step for a file a script decodes, so its
    // modules instantiate inline here.
    if (file != nullptr)
    {
        file->startScripts();
    }
#endif
    return file;
}

DecodedBindable rive::decodedBindable(const File& file, const char* name)
{
    DecodedBindable bindable;
    bindable.artboard = name != nullptr ? file.bindableArtboardNamed(name)
                                        : file.bindableArtboardDefault();
    if (bindable.artboard != nullptr)
    {
        bindable.viewModel =
            file.createDefaultViewModelInstance(bindable.artboard->artboard());
    }
    return bindable;
}
