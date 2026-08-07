#ifdef WITH_RIVE_SCRIPTING
#include <stdio.h>
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/assets/blob_asset.hpp"
#include "rive/file.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/view_model_type.hpp"
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
//
// Errors instead of answering when the context is recording and does not yet
// know its replay device. Conservative defaults would be the wrong answer to
// give: they are indistinguishable from a real low end device, so a script
// cannot tell it is being guessed at, and the branch it picks is written into
// a stream that replays flawlessly on hardware that contradicts it. Failing at
// the read is the only signal that fits through this API.
int lua_push_gpu_features(lua_State* L)
{
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
    auto* oreCtx = static_cast<ore::Context*>(
        static_cast<ScriptingContext*>(lua_getthreaddata(L))->oreContext());
    if (oreCtx != nullptr && !oreCtx->featuresKnown())
    {
        luaL_error(L,
                   "context.features is not available yet: this script is "
                   "recording for a GPU device that has not been attached, so "
                   "no capability can be reported without guessing at it. "
                   "Read features from a method that runs after the first "
                   "frame instead of at module scope");
    }
    if (oreCtx != nullptr)
    {
        const auto& f = oreCtx->features();
        lua_createtable(L, 0, 19);
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
        lua_pushboolean(L, f.colorBufferHalfFloat);
        lua_setfield(L, -2, "colorBufferHalfFloat");
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
    lua_createtable(L, 0, 19);
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
    lua_setfield(L, -2, "colorBufferHalfFloat");
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
        if (dataContext && dataContext->mainViewModelInstance())
        {
            auto viewModelInstance = dataContext->mainViewModelInstance();
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

// Resolve a global view model instance by name from a script's data context.
static rcp<ViewModelInstance> resolveGlobalViewModel(
    const rcp<DataContext>& dataContext,
    File* file,
    const char* name)
{
    // Validate the name up front (mirrors
    // Artboard::setGlobalViewModelInstance): an unknown name makes viewModelId
    // return viewModelCount() (an invalid slot key), and a non-global name must
    // never resolve — even if something else populated that slot. Only after
    // confirming the id is in range and the referenced view model is global do
    // we attempt slot / identity resolution.
    uint32_t slotKey = file->viewModelId(name);
    if (slotKey >= file->viewModelCount())
    {
        return nullptr;
    }
    ViewModel* globalViewModel = file->viewModel(slotKey);
    if (globalViewModel == nullptr ||
        static_cast<ViewModelType>(globalViewModel->viewModelType()) !=
            ViewModelType::global)
    {
        return nullptr;
    }

    // Fast path: the pure runtime slots globals onto the root (top-most) data
    // context.
    DataContext* root = dataContext.get();
    while (root->parent() != nullptr)
    {
        root = root->parent().get();
    }
    auto viewModelInstance = root->instanceForSlot(slotKey);
    if (viewModelInstance != nullptr)
    {
        return viewModelInstance;
    }

    // Fallback: the editor (and nested-artboard propagation) build unslotted
    // contexts, so match the global view model against the contexts' instances.
    for (DataContext* ctx = dataContext.get(); ctx != nullptr;
         ctx = ctx->parent().get())
    {
        for (const auto& instance : ctx->viewModelInstances())
        {
            if (instance != nullptr && instance->viewModel() == globalViewModel)
            {
                return instance;
            }
        }
    }
    return nullptr;
}

int ScriptedContext::pushGlobalViewModel(lua_State* state)
{
    const char* name = luaL_checkstring(state, 2);
    if (m_scriptedObject)
    {
        auto scriptAsset = m_scriptedObject->scriptAsset();
        auto dataContext = m_scriptedObject->dataContext();
        if (scriptAsset != nullptr && dataContext != nullptr)
        {
            File* file = scriptAsset->file();
            if (file != nullptr)
            {
                auto viewModelInstance =
                    resolveGlobalViewModel(dataContext, file, name);
                if (viewModelInstance != nullptr)
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
    }
    return 0;
}

int ScriptedContext::pushGlobalViewModelNames(lua_State* state)
{
    std::vector<std::string> names;
    if (m_scriptedObject)
    {
        auto scriptAsset = m_scriptedObject->scriptAsset();
        if (scriptAsset != nullptr)
        {
            File* file = scriptAsset->file();
            if (file != nullptr)
            {
                names = file->globalViewModelNames();
            }
        }
    }
    lua_createtable(state, (int)names.size(), 0);
    int index = 1;
    for (const auto& name : names)
    {
        lua_pushstring(state, name.c_str());
        lua_rawseti(state, -2, index++);
    }
    return 1;
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
            case (int)LuaAtoms::globalViewModel:
            {
                return scriptedContext->pushGlobalViewModel(L);
            }
            case (int)LuaAtoms::globalViewModelNames:
            {
                return scriptedContext->pushGlobalViewModelNames(L);
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
                ScriptingContext::ScopedAssetReference reference(L, blobName);

                auto scriptedObject = scriptedContext->scriptedObject();
                auto scriptAsset = scriptedObject->scriptAsset();
                if (scriptAsset != nullptr)
                {
                    File* file = scriptAsset->file();
                    if (file != nullptr)
                    {
                        BlobAsset* found = nullptr;
                        int bestRank = 0;
                        for (const auto& asset : file->assets())
                        {
                            if (!asset->is<BlobAsset>())
                            {
                                continue;
                            }
                            BlobAsset* blobAsset = asset->as<BlobAsset>();
                            int rank = reference.match(blobAsset->name(),
                                                       blobAsset->name());
                            if (rank > bestRank && !blobAsset->bytes().empty())
                            {
                                bestRank = rank;
                                found = blobAsset;
                            }
                        }
                        if (found != nullptr)
                        {
                            auto scriptedBlob = lua_newrive<ScriptedBlob>(L);
                            scriptedBlob->asset =
                                ref_rcp(static_cast<FileAsset*>(found));
                            return 1;
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
                // Descriptor is optional; missing or zero width/height yields
                // a deferred canvas with no backing texture. Use :resize() to
                // allocate once the real layout size is known.
                uint32_t cw = 0;
                uint32_t ch = 0;
                if (!lua_isnoneornil(L, 2))
                {
                    luaL_checktype(L, 2, LUA_TTABLE);
                    lua_getfield(L, 2, "width");
                    if (!lua_isnil(L, -1))
                        cw = (uint32_t)luaL_checknumber(L, -1);
                    lua_pop(L, 1);
                    lua_getfield(L, 2, "height");
                    if (!lua_isnil(L, -1))
                        ch = (uint32_t)luaL_checknumber(L, -1);
                    lua_pop(L, 1);
                }
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
                auto* handle = lua_newrive<ScriptedCanvas>(L);
                handle->m_L = L;
                handle->renderCtx = renderCtx;

                // A size-less canvas allocates nothing, so it needs no device.
                // Checked before the context, or a layout script that does not
                // know its size at init is refused for a device it will only
                // need at resize().
                if (cw == 0 || ch == 0)
                {
                    return 1;
                }
                if (renderCtx == nullptr)
                {
                    // A recording session binds its device after import, and
                    // generators size their canvas at construction, before any
                    // texture exists. Record the request; satisfyPending
                    // materializes it on first use once the device arrives.
                    if (scriptingCtx->deferredCanvasHost() != nullptr)
                    {
                        handle->pendingWidth = cw;
                        handle->pendingHeight = ch;
                        return 1;
                    }
                    luaL_error(
                        L,
                        "context:canvas() requires a RenderContext — call "
                        "setRenderContext() first");
                    return 0;
                }

                auto canvas =
                    allocScriptRenderCanvas(renderCtx, scriptingCtx, cw, ch);
                if (!canvas)
                {
                    luaL_error(
                        L,
                        "context:canvas() failed to create RenderCanvas");
                    return 0;
                }
                handle->canvas = std::move(canvas);
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
                // Descriptor is optional; missing or zero width/height yields
                // a deferred canvas with no backing texture. Use :resize() to
                // allocate once the real layout size is known.
                uint32_t gw = 0;
                uint32_t gh = 0;
                if (!lua_isnoneornil(L, 2))
                {
                    luaL_checktype(L, 2, LUA_TTABLE);
                    lua_getfield(L, 2, "width");
                    if (!lua_isnil(L, -1))
                        gw = (uint32_t)luaL_checknumber(L, -1);
                    lua_pop(L, 1);
                    lua_getfield(L, 2, "height");
                    if (!lua_isnil(L, -1))
                        gh = (uint32_t)luaL_checknumber(L, -1);
                    lua_pop(L, 1);
                }
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
                if (gpuScriptingCtx->gpuCanvasDeferOnly())
                {
                    // Headless detection (editor method-detection VM): there is
                    // no real RenderContext/GPU device. Hand back a deferred
                    // canvas with no backing texture regardless of requested
                    // size, so a generator that creates a sized canvas at
                    // construction runs without reaching makeRenderCanvas.
                    auto* handle = lua_newrive<ScriptedGPUCanvas>(L);
                    handle->m_L = L;
                    handle->renderCtx = nullptr;
                    return 1;
                }
                auto* gpuRenderCtx = static_cast<gpu::RenderContext*>(
                    gpuScriptingCtx->renderContext());
                auto* handle = lua_newrive<ScriptedGPUCanvas>(L);
                handle->m_L = L;
                handle->renderCtx = gpuRenderCtx;

                // The documented size-less contract: no descriptor means no
                // backing texture, so nothing here touches a device. Checked
                // ahead of the contexts, or a layout script that learns its
                // size at resize() is refused for a device it does not use.
                if (gw == 0 || gh == 0)
                {
                    return 1;
                }
                if (gpuRenderCtx == nullptr)
                {
                    // Same late-device contract as canvas() above.
                    if (gpuScriptingCtx->deferredCanvasHost() != nullptr)
                    {
                        handle->pendingWidth = gw;
                        handle->pendingHeight = gh;
                        return 1;
                    }
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

                auto canvas = allocScriptRenderCanvas(gpuRenderCtx,
                                                      gpuScriptingCtx,
                                                      gw,
                                                      gh);
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
                handle->canvas = std::move(canvas);
                handle->oreColorView = std::move(colorView);

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

            case (int)LuaAtoms::shader:
            {
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
                const char* shaderName = luaL_checkstring(L, 2);
                ScriptingContext::ScopedAssetReference reference(L, shaderName);

                // Runtime path: the file's ShaderAsset best matching the
                // scoped reference.
                ShaderAsset* fileAsset = nullptr;
                auto scriptedObject = scriptedContext->scriptedObject();
                auto scriptAsset = scriptedObject->scriptAsset();
                if (scriptAsset != nullptr)
                {
                    fileAsset = lua_gpu_find_shader_asset(scriptAsset->file(),
                                                          reference);
                }

                auto* scriptingCtx =
                    static_cast<ScriptingContext*>(lua_getthreaddata(L));
                auto* scripted = lua_newrive<ScriptedShader>(L);
                if (lua_gpu_load_shader_by_name(scripted,
                                                scriptingCtx,
                                                reference,
                                                fileAsset))
                {
                    return 1;
                }
                lua_pop(L, 1);
                return 0; // return nil, shader not found or compile failed
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
