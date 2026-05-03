#ifdef WITH_RIVE_SCRIPTING
#include <stdio.h>
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/assets/blob_asset.hpp"
#include "rive/file.hpp"
#ifdef WITH_RIVE_AUDIO
#include "rive/audio/audio_engine.hpp"
#include "rive/assets/audio_asset.hpp"
#endif
#ifdef RIVE_CANVAS
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#ifdef RIVE_ORE
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/assets/shader_asset.hpp"
#endif
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace rive;

// Pushes a GPU features table onto the Lua stack. Queries the ORE context
// when available, otherwise returns conservative defaults. Always returns 1.
int lua_push_gpu_features(lua_State* L)
{
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
    auto* oreCtx = static_cast<ore::Context*>(
        static_cast<ScriptingContext*>(lua_getthreaddata(L))->oreContext());
    if (oreCtx != nullptr)
    {
        const auto& f = oreCtx->features();
        lua_createtable(L, 0, 18);
        lua_pushboolean(L, f.bc);
        lua_setfield(L, -2, "bc");
        lua_pushboolean(L, f.etc2);
        lua_setfield(L, -2, "etc2");
        lua_pushboolean(L, f.astc);
        lua_setfield(L, -2, "astc");
        lua_pushnumber(L, f.maxTextureSize2D);
        lua_setfield(L, -2, "maxTextureSize2D");
        lua_pushnumber(L, f.maxTextureSizeCube);
        lua_setfield(L, -2, "maxTextureSizeCube");
        lua_pushnumber(L, f.maxTextureSize3D);
        lua_setfield(L, -2, "maxTextureSize3D");
        lua_pushboolean(L, f.anisotropicFiltering);
        lua_setfield(L, -2, "anisotropicFiltering");
        lua_pushboolean(L, f.texture3D);
        lua_setfield(L, -2, "texture3D");
        lua_pushboolean(L, f.textureArrays);
        lua_setfield(L, -2, "textureArrays");
        lua_pushboolean(L, f.colorBufferFloat);
        lua_setfield(L, -2, "colorBufferFloat");
        lua_pushboolean(L, f.perTargetBlend);
        lua_setfield(L, -2, "perTargetBlend");
        lua_pushboolean(L, f.perTargetWriteMask);
        lua_setfield(L, -2, "perTargetWriteMask");
        lua_pushboolean(L, f.drawBaseInstance);
        lua_setfield(L, -2, "drawBaseInstance");
        lua_pushboolean(L, f.depthBiasClamp);
        lua_setfield(L, -2, "depthBiasClamp");
        lua_pushnumber(L, f.maxColorAttachments);
        lua_setfield(L, -2, "maxColorAttachments");
        lua_pushnumber(L, f.maxUniformBufferSize);
        lua_setfield(L, -2, "maxUniformBufferSize");
        lua_pushnumber(L, f.maxSamplers);
        lua_setfield(L, -2, "maxSamplers");
        lua_pushnumber(L, f.maxSamples);
        lua_setfield(L, -2, "maxSamples");
        lua_setreadonly(L, -1, true);
        return 1;
    }
#endif
    // Fallback when no ore context is available
    lua_createtable(L, 0, 18);
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "bc");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "etc2");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "astc");
    lua_pushnumber(L, 4096);
    lua_setfield(L, -2, "maxTextureSize2D");
    lua_pushnumber(L, 4096);
    lua_setfield(L, -2, "maxTextureSizeCube");
    lua_pushnumber(L, 256);
    lua_setfield(L, -2, "maxTextureSize3D");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "anisotropicFiltering");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "texture3D");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "textureArrays");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "colorBufferFloat");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "perTargetBlend");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "perTargetWriteMask");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "drawBaseInstance");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "depthBiasClamp");
    lua_pushnumber(L, 4);
    lua_setfield(L, -2, "maxColorAttachments");
    lua_pushnumber(L, 16384);
    lua_setfield(L, -2, "maxUniformBufferSize");
    lua_pushnumber(L, 16);
    lua_setfield(L, -2, "maxSamplers");
    lua_pushnumber(L, 4);
    lua_setfield(L, -2, "maxSamples");
    lua_setreadonly(L, -1, true);
    return 1;
}

ScriptedContext::ScriptedContext(ScriptedObject* scriptedObject) :
    m_scriptedObject(scriptedObject)
{}

int ScriptedContext::pushViewModel(lua_State* state)
{
    if (m_scriptedObject)
    {
        auto dataContext = m_scriptedObject->dataContext();
        if (dataContext && dataContext->viewModelInstance())
        {
            auto viewModelInstance = dataContext->viewModelInstance();
            lua_newrive<ScriptedViewModel>(
                state,
                state,
                ref_rcp(viewModelInstance->viewModel()),
                viewModelInstance);
            return 1;
        }
    }
    m_missingRequestedData = true;
    return 0;
}

int ScriptedContext::pushRootViewModel(lua_State* state)
{
    if (m_scriptedObject)
    {
        auto dataContext = m_scriptedObject->dataContext();
        if (dataContext)
        {
            auto viewModelInstance = dataContext->rootViewModelInstance();
            if (viewModelInstance)
            {
                lua_newrive<ScriptedViewModel>(
                    state,
                    state,
                    ref_rcp(viewModelInstance->viewModel()),
                    viewModelInstance);
                return 1;
            }
        }
    }
    m_missingRequestedData = true;
    return 0;
}

int ScriptedContext::pushDataContext(lua_State* state)
{

    if (m_scriptedObject)
    {
        auto dataContext = m_scriptedObject->dataContext();
        if (dataContext)
        {
            lua_newrive<ScriptedDataContext>(state, state, dataContext);
            return 1;
        }
    }
    m_missingRequestedData = true;
    return 0;
}

static int context_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        auto scriptedContext = lua_torive<ScriptedContext>(L, 1);
        if (scriptedContext->scriptedObject() == nullptr)
        {
            luaL_error(L,
                       "context:%s() called on a disposed context — the "
                       "context passed to init() must not be used after "
                       "init() returns",
                       str);
            return 0;
        }
        switch (atom)
        {
            case (int)LuaAtoms::markNeedsUpdate:
            {
                auto scriptedObject = scriptedContext->scriptedObject();
                scriptedObject->markNeedsUpdate();
                return 0;
            }
            case (int)LuaAtoms::viewModel:
            {
                return scriptedContext->pushViewModel(L);
            }
            case (int)LuaAtoms::rootViewModel:
            {
                return scriptedContext->pushRootViewModel(L);
            }
            case (int)LuaAtoms::image:
            {
                const char* imageName = luaL_checkstring(L, 2);

                // First, try to find the image from the file's assets (runtime)
                auto scriptedObject = scriptedContext->scriptedObject();
                auto scriptAsset = scriptedObject->scriptAsset();
                if (scriptAsset != nullptr)
                {
                    File* file = scriptAsset->file();
                    if (file != nullptr)
                    {
                        // Find ImageAsset by name
                        auto assets = file->assets();
                        for (const auto& asset : assets)
                        {
                            if (asset->is<ImageAsset>())
                            {
                                ImageAsset* imageAsset =
                                    asset->as<ImageAsset>();
                                if (imageAsset->name() == imageName)
                                {
                                    RenderImage* renderImage =
                                        imageAsset->renderImage();
                                    if (renderImage != nullptr)
                                    {
                                        auto scriptedImage =
                                            lua_newrive<ScriptedImage>(L);
                                        // ref_rcp properly refs the RenderImage
                                        // for the rcp<>. When ScriptedImage is
                                        // GC'd, rcp<> destructor will deref()
                                        scriptedImage->image =
                                            ref_rcp(renderImage);
                                        return 1;
                                    }
                                }
                            }
                        }
                    }
                }

                return 0; // return nil if not found
            }
            case (int)LuaAtoms::blob:
            {
                const char* blobName = luaL_checkstring(L, 2);

                auto scriptedObject = scriptedContext->scriptedObject();
                auto scriptAsset = scriptedObject->scriptAsset();
                if (scriptAsset != nullptr)
                {
                    File* file = scriptAsset->file();
                    if (file != nullptr)
                    {
                        auto assets = file->assets();
                        for (const auto& asset : assets)
                        {
                            if (asset->is<BlobAsset>())
                            {
                                BlobAsset* blobAsset = asset->as<BlobAsset>();
                                if (blobAsset->name() == blobName)
                                {
                                    auto bytes = blobAsset->bytes();
                                    if (!bytes.empty())
                                    {
                                        auto scriptedBlob =
                                            lua_newrive<ScriptedBlob>(L);
                                        scriptedBlob->asset = ref_rcp(
                                            static_cast<FileAsset*>(blobAsset));
                                        return 1;
                                    }
                                }
                            }
                        }
                    }
                }

                return 0; // return nil if not found
            }
            case (int)LuaAtoms::dataContext:
            {
                return scriptedContext->pushDataContext(L);
            }
#ifdef WITH_RIVE_AUDIO
            case (int)LuaAtoms::audio:
            {
                const char* audioName = luaL_checkstring(L, 2);

                auto scriptedObject = scriptedContext->scriptedObject();
                auto scriptAsset = scriptedObject->scriptAsset();
                if (scriptAsset != nullptr)
                {
                    File* file = scriptAsset->file();
                    if (file != nullptr)
                    {
                        auto assets = file->assets();
                        for (const auto& asset : assets)
                        {
                            if (asset->is<AudioAsset>())
                            {
                                AudioAsset* audioAsset =
                                    asset->as<AudioAsset>();
                                if (audioAsset->name() == audioName)
                                {
                                    auto audioSource =
                                        audioAsset->audioSource();
                                    if (audioSource != nullptr)
                                    {
                                        auto scriptedAudioSource =
                                            lua_newrive<ScriptedAudioSource>(L);
                                        scriptedAudioSource->source(
                                            audioSource);
                                        return 1;
                                    }
                                }
                            }
                        }
                    }
                }

                return 0; // return nil if not found
            }
#endif

            case (int)LuaAtoms::canvas:
            {
                // context:canvas({ width = w, height = h, clearColor = c })
                luaL_checktype(L, 2, LUA_TTABLE);
                lua_getfield(L, 2, "width");
                uint32_t cw = (uint32_t)luaL_checknumber(L, -1);
                lua_pop(L, 1);
                lua_getfield(L, 2, "height");
                uint32_t ch = (uint32_t)luaL_checknumber(L, -1);
                lua_pop(L, 1);
#ifndef RIVE_CANVAS
                (void)cw;
                (void)ch;
                luaL_error(L, "context:canvas() requires a RIVE_CANVAS build");
                return 0;
#else
                auto* scriptingCtx =
                    static_cast<ScriptingContext*>(lua_getthreaddata(L));
                auto* renderCtx = static_cast<gpu::RenderContext*>(
                    scriptingCtx->renderContext());
                if (renderCtx == nullptr)
                {
                    luaL_error(
                        L,
                        "context:canvas() requires a RenderContext — call "
                        "setRenderContext() first");
                    return 0;
                }
                auto canvas = renderCtx->makeRenderCanvas(cw, ch);
                if (!canvas)
                {
                    luaL_error(
                        L,
                        "context:canvas() failed to create RenderCanvas");
                    return 0;
                }
                auto* handle = lua_newrive<ScriptedCanvas>(L);
                handle->m_L = L;
                handle->canvas = std::move(canvas);
                handle->renderCtx = renderCtx;
                // Create a ScriptedImage backed by canvas->renderImage() so
                // the script can composite it with renderer:drawImage()
                auto* img = lua_newrive<ScriptedImage>(L);
                img->image = ref_rcp(
                    static_cast<RenderImage*>(handle->canvas->renderImage()));
                handle->m_imageRef = lua_ref(L, -1);
                lua_pop(L, 1); // pop image, handle remains on top
                return 1;
#endif
            }
            case (int)LuaAtoms::gpuCanvas:
            {
                // context:gpuCanvas({ width = w, height = h })
                luaL_checktype(L, 2, LUA_TTABLE);
                lua_getfield(L, 2, "width");
                uint32_t gw = (uint32_t)luaL_checknumber(L, -1);
                lua_pop(L, 1);
                lua_getfield(L, 2, "height");
                uint32_t gh = (uint32_t)luaL_checknumber(L, -1);
                lua_pop(L, 1);
#if !defined(RIVE_CANVAS) || !defined(RIVE_ORE)
                (void)gw;
                (void)gh;
                luaL_error(L,
                           "context:gpuCanvas() requires a RIVE_CANVAS + "
                           "RIVE_ORE build");
                return 0;
#else
                auto* gpuScriptingCtx =
                    static_cast<ScriptingContext*>(lua_getthreaddata(L));
                auto* gpuRenderCtx = static_cast<gpu::RenderContext*>(
                    gpuScriptingCtx->renderContext());
                if (gpuRenderCtx == nullptr)
                {
                    luaL_error(
                        L,
                        "context:gpuCanvas() requires a RenderContext — call "
                        "setRenderContext() first");
                    return 0;
                }
                auto* oreCtx =
                    static_cast<ore::Context*>(gpuScriptingCtx->oreContext());
                if (oreCtx == nullptr)
                {
                    luaL_error(
                        L,
                        "context:gpuCanvas() requires a GPU context — call "
                        "scriptingWorkspaceSetOreContext() before requestVM()");
                    return 0;
                }
                auto canvas = gpuRenderCtx->makeRenderCanvas(gw, gh);
                if (!canvas)
                {
                    luaL_error(
                        L,
                        "context:gpuCanvas() failed to create RenderCanvas");
                    return 0;
                }
                auto colorView = oreCtx->wrapCanvasTexture(canvas.get());
                if (!colorView)
                {
                    luaL_error(
                        L,
                        "context:gpuCanvas() failed to wrap canvas texture");
                    return 0;
                }
                auto* handle = lua_newrive<ScriptedGPUCanvas>(L);
                handle->m_L = L;
                handle->canvas = std::move(canvas);
                handle->oreColorView = std::move(colorView);
                handle->renderCtx = gpuRenderCtx;

                // Optional MSAA: context:gpuCanvas({ ..., sampleCount = 4 })
                lua_getfield(L, 2, "sampleCount");
                uint32_t gpuCanvasSamples =
                    lua_isnumber(L, -1)
                        ? static_cast<uint32_t>(lua_tonumber(L, -1))
                        : 1;
                lua_pop(L, 1);
                if (gpuCanvasSamples > 1)
                {
                    if ((gpuCanvasSamples & (gpuCanvasSamples - 1)) != 0)
                        luaL_error(L,
                                   "gpuCanvas sampleCount must be a power of "
                                   "two (got %u)",
                                   gpuCanvasSamples);
                    uint32_t maxSamples = oreCtx->features().maxSamples;
                    if (gpuCanvasSamples > maxSamples)
                        luaL_error(L,
                                   "gpuCanvas sampleCount %u exceeds device "
                                   "maximum of %u — query "
                                   "context:features().maxSamples first",
                                   gpuCanvasSamples,
                                   maxSamples);
                }
                if (gpuCanvasSamples > 1)
                {
                    ore::TextureDesc msaaDesc;
                    msaaDesc.width = gw;
                    msaaDesc.height = gh;
                    msaaDesc.format = handle->oreColorView->texture()->format();
                    msaaDesc.renderTarget = true;
                    msaaDesc.sampleCount = gpuCanvasSamples;
                    msaaDesc.label = "GPUCanvasMSAAColor";
                    handle->oreMSAAColorTexture = oreCtx->makeTexture(msaaDesc);
                    if (!handle->oreMSAAColorTexture)
                    {
                        luaL_error(
                            L,
                            "context:gpuCanvas() failed to create MSAA color "
                            "texture (sampleCount=%u)",
                            gpuCanvasSamples);
                        return 0;
                    }
                    ore::TextureViewDesc msaaViewDesc;
                    msaaViewDesc.texture = handle->oreMSAAColorTexture.get();
                    handle->oreMSAAColorView =
                        oreCtx->makeTextureView(msaaViewDesc);

                    // Depth buffer — sampleCount must match the MSAA color
                    // attachment or Metal/Vulkan will reject the render pass.
                    ore::TextureDesc depthDesc;
                    depthDesc.width = gw;
                    depthDesc.height = gh;
                    depthDesc.format = ore::TextureFormat::depth32float;
                    depthDesc.renderTarget = true;
                    depthDesc.sampleCount = gpuCanvasSamples;
                    depthDesc.label = "GPUCanvasMSAADepth";
                    handle->oreDepthTexture = oreCtx->makeTexture(depthDesc);
                    if (!handle->oreDepthTexture)
                    {
                        luaL_error(
                            L,
                            "context:gpuCanvas() failed to create MSAA depth "
                            "texture (sampleCount=%u)",
                            gpuCanvasSamples);
                        return 0;
                    }
                    ore::TextureViewDesc depthViewDesc;
                    depthViewDesc.texture = handle->oreDepthTexture.get();
                    handle->oreDepthView =
                        oreCtx->makeTextureView(depthViewDesc);
                }
                // Create a ScriptedImage backed by canvas->renderImage() so
                // the script can composite it with renderer:drawImage()
                auto* img = lua_newrive<ScriptedImage>(L);
                img->image = ref_rcp(
                    static_cast<RenderImage*>(handle->canvas->renderImage()));
                handle->m_imageRef = lua_ref(L, -1);
                lua_pop(L, 1); // pop image, handle remains on top
                return 1;
#endif
            }
            case (int)LuaAtoms::features:
                return lua_push_gpu_features(L);

            case (int)LuaAtoms::preferredCanvasFormat:
            {
                // Return the native GPU canvas texture format for this
                // platform. makeRenderCanvas() hardcodes RGBA8 on Metal so that
                // Rive's internal PLS shaders (which work in RGBA space) can
                // render into the canvas without format-dependent swizzling.
                //   Metal  (macOS/iOS)  → rgba8unorm (off-screen canvas, not a
                //   CAMetalLayer surface) D3D11/D3D12        → bgra8unorm
                //   Vulkan/GL/WebGPU   → rgba8unorm (safe default; actual
                //                        format may vary — query canvas.format
                //                        for the authoritative value once a
                //                        canvas has been created)
#if defined(_WIN32)
                lua_pushstring(L, "bgra8unorm");
#else
                lua_pushstring(L, "rgba8unorm");
#endif
                return 1;
            }

            case (int)LuaAtoms::loadShader:
            {
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
                const char* shaderName = luaL_checkstring(L, 2);

                // Runtime path: search file->assets() for a ShaderAsset
                // with the matching name.
                ShaderAsset* fileAsset = nullptr;
                auto scriptedObject = scriptedContext->scriptedObject();
                auto scriptAsset = scriptedObject->scriptAsset();
                if (scriptAsset != nullptr)
                {
                    File* file = scriptAsset->file();
                    if (file != nullptr)
                    {
                        for (const auto& asset : file->assets())
                        {
                            if (asset->is<ShaderAsset>())
                            {
                                auto* sa = asset->as<ShaderAsset>();
                                if (sa->name() == shaderName)
                                {
                                    fileAsset = sa;
                                    break;
                                }
                            }
                        }
                    }
                }

                auto* scriptingCtx =
                    static_cast<ScriptingContext*>(lua_getthreaddata(L));
                auto* scripted = lua_newrive<ScriptedShader>(L);
                if (lua_gpu_load_shader_by_name(scripted,
                                                scriptingCtx,
                                                shaderName,
                                                fileAsset))
                {
                    return 1;
                }
                lua_pop(L, 1);

                // Legacy-compat fallback: pre-ShaderAsset .riv files pack
                // WGSL shaders into ScriptAssets. Detect by RSTB magic and
                // build the shader from those raw bytes.
                if (scriptAsset != nullptr)
                {
                    File* file = scriptAsset->file();
                    if (file != nullptr)
                    {
                        for (const auto& asset : file->assets())
                        {
                            if (!asset->is<ScriptAsset>())
                                continue;
                            auto* sa = asset->as<ScriptAsset>();
                            if (sa->name() != shaderName)
                                continue;
                            auto bc = sa->moduleBytecode();
                            if (bc.size() < 4 || bc[0] != 'R' || bc[1] != 'S' ||
                                bc[2] != 'T' || bc[3] != 'B')
                                continue;
                            auto* legacyScripted =
                                lua_newrive<ScriptedShader>(L);
                            if (lua_gpu_make_shader_from_rstb(
                                    legacyScripted,
                                    scriptingCtx,
                                    bc.data(),
                                    static_cast<uint32_t>(bc.size())))
                            {
                                return 1;
                            }
                            lua_pop(L, 1);
                            break;
                        }
                    }
                }
                return 0; // return nil — shader not found or compile failed
#else
                return 0;
#endif
            }

            case (int)LuaAtoms::decodeImage:
            {
                // Defined in lua_image_decode.cpp.
                extern int context_decodeImage_impl(lua_State * L);
                return context_decodeImage_impl(L);
            }

            default:
                break;
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedContext::luaName);
    return 0;
}

int luaopen_rive_contex(lua_State* L)
{
    {
        lua_register_rive<ScriptedContext>(L);

        lua_pushcfunction(L, context_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }
    return 0;
}

#endif
