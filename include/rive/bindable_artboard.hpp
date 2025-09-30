#ifndef _RIVE_BINDABLE_ARTBOARD_HPP_
#define _RIVE_BINDABLE_ARTBOARD_HPP_
#include "rive/refcnt.hpp"
#include "rive/file.hpp"
#include "rive/artboard.hpp"

namespace rive
{

class BindableArtboard : public RefCnt<BindableArtboard>
{
public:
    BindableArtboard(rcp<const File> file,
                     std::unique_ptr<ArtboardInstance> artboard);
    ArtboardInstance* artboard() { return m_artboard.get(); }

private:
    rcp<const File> m_file;
    std::unique_ptr<ArtboardInstance> m_artboard;
};
} // namespace rive

#endif
