#include "rive/scripted/decoded_file.hpp"
#include "rive/bindable_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"

using namespace rive;

rcp<File> rive::importDecodedFile(Span<const uint8_t> bytes,
                                  Factory* factory,
                                  ImportResult* result)
{
    return File::import(bytes,
                        factory,
                        result,
                        nullptr,
                        nullptr,
                        /*requireSignedScripts*/ true);
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
