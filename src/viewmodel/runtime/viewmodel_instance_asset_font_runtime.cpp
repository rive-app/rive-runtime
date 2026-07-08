
#include "rive/viewmodel/runtime/viewmodel_instance_asset_font_runtime.hpp"

// Default namespace for Rive Cpp code
using namespace rive;

void ViewModelInstanceAssetFontRuntime::value(Font* font)
{
    m_viewModelInstanceValue->as<ViewModelInstanceAssetFont>()->value(font);
}

#ifdef TESTING
Font* ViewModelInstanceAssetFontRuntime::testing_value()
{
    return m_viewModelInstanceValue->as<ViewModelInstanceAssetFont>()
        ->asset()
        ->font()
        .get();
}
#endif
