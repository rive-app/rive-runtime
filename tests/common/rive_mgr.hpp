#ifndef _RIVE_MGR_HPP_
#define _RIVE_MGR_HPP_

#include "rive/scene.hpp"
#include <memory>

namespace rive
{
class Factory;
class File;
class ArtboardInstance;
} // namespace rive

class RiveMgr
{
    rive::Factory* m_Factory;
    std::unique_ptr<rive::File> m_File;
    std::unique_ptr<rive::ArtboardInstance> m_Artboard;
    std::unique_ptr<rive::Scene> m_Scene;

    bool loadArtboard(const char path[], const char artboard[]);

public:
    RiveMgr(rive::Factory* factory);
    ~RiveMgr();

    bool loadAnimation(const char path[], const char artboard[], const char animation[]);
    bool loadMachine(const char path[], const char artboard[], const char machine[]);

    rive::Scene* scene() const { return m_Scene.get(); }

    static std::unique_ptr<rive::File> loadFile(const char filename[], rive::Factory*);
};

#endif
