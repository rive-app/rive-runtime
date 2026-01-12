#ifndef _RIVE_ARTBOARD_REFERENCER_HPP_
#define _RIVE_ARTBOARD_REFERENCER_HPP_
#include "rive/refcnt.hpp"
#include <stdio.h>
namespace rive
{
class Artboard;
class ViewModelInstanceArtboard;
class File;
class Core;

class ArtboardReferencer
{
protected:
    Artboard* findArtboard(ViewModelInstanceArtboard* viewModelInstanceArtboard,
                           Artboard* parentArtboard,
                           File* file);
    Artboard* m_referencedArtboard = nullptr;

public:
    virtual ~ArtboardReferencer() = default;
    virtual void updateArtboard(
        ViewModelInstanceArtboard* viewModelInstanceArtboard) = 0;
    virtual int referencedArtboardId() = 0;
    static ArtboardReferencer* from(Core*);
    virtual void referencedArtboard(Artboard* artboard)
    {
        m_referencedArtboard = artboard;
    }
};
} // namespace rive

#endif