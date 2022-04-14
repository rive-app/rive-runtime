#ifndef _RIVE_MGR_HPP_
#define _RIVE_MGR_HPP_

#include "rive/animation/animation.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include <memory>

class RiveMgr {
    std::unique_ptr<rive::File> m_File;
    std::unique_ptr<rive::ArtboardInstance> m_Artboard;
    std::unique_ptr<rive::LinearAnimationInstance> m_Animation;

public:
    RiveMgr() {}

    bool load(const char path[], const char artboard[], const char animation[]);

    rive::File* file() const { return m_File.get(); }
    rive::ArtboardInstance* artboard() const { return m_Artboard.get(); }
    rive::LinearAnimationInstance* animation() const { return m_Animation.get(); }

    static std::unique_ptr<rive::File> loadFile(const char filename[]);
};

#endif
