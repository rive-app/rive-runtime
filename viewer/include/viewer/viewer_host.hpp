/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_VIEWER_HOST_HPP_
#define _RIVE_VIEWER_HOST_HPP_

#include "rive/factory.hpp"
#include "rive/renderer.hpp"
#include "rive/text_engine.hpp"

#include "sokol_gfx.h"

class ViewerContent;

class ViewerHost
{
public:
    virtual ~ViewerHost() {}

    // subclasses can modify sg_pass_action if they wish, but need not.
    virtual bool init(sg_pass_action*, int width, int height) = 0;

    virtual void handleResize(int width, int height) = 0;

    // subclasses need only override one or the other
    virtual void beforeDefaultPass(ViewerContent*, double) {}
    virtual void afterDefaultPass(ViewerContent*, double) {}

    static std::unique_ptr<ViewerHost> Make();
    static rive::Factory* Factory();
};

#endif
