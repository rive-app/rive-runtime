// Copyright 2020 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef RIVE_DAWN

#include "webgpu/webgpu_glfw.h"

#import <QuartzCore/CAMetalLayer.h>
#include "GLFW/glfw3.h"

#include <cstdlib>

#define GLFW_EXPOSE_NATIVE_COCOA
#include "GLFW/glfw3native.h"

float GetDawnWindowBackingScaleFactor(GLFWwindow* window, bool retina)
{
    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    return retina ? nsWindow.backingScaleFactor : 1;
}

std::unique_ptr<wgpu::ChainedStruct> SetupDawnWindowAndGetSurfaceDescriptor(GLFWwindow* window,
                                                                            bool retina)
{
    @autoreleasepool
    {
        NSWindow* nsWindow = glfwGetCocoaWindow(window);
        NSView* view = [nsWindow contentView];

        // Create a CAMetalLayer that covers the whole window that will be passed to
        // CreateSurface.
        [view setWantsLayer:YES];
        [view setLayer:[CAMetalLayer layer]];

        // Use retina if the window was created with retina support.
        [[view layer] setContentsScale:GetDawnWindowBackingScaleFactor(window, retina)];

        std::unique_ptr<wgpu::SurfaceDescriptorFromMetalLayer> desc =
            std::make_unique<wgpu::SurfaceDescriptorFromMetalLayer>();
        desc->layer = (__bridge void*)[view layer];
        return std::move(desc);
    }
}

#endif
