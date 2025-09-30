#include "rive/bindable_artboard.hpp"

using namespace rive;

BindableArtboard::BindableArtboard(rcp<const File> file,
                                   std::unique_ptr<ArtboardInstance> artboard) :
    m_file(file), m_artboard(std::move(artboard))
{}