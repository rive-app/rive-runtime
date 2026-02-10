#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/assets/blob_asset.hpp"
#include "rive/file.hpp"
#ifdef WITH_RIVE_AUDIO
#include "rive/audio/audio_engine.hpp"
#include "rive/assets/audio_asset.hpp"
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace rive;

ScriptedContext::ScriptedContext(ScriptedObject* scriptedObject) :
    m_scriptedObject(scriptedObject)
{}

static int context_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        auto scriptedContext = lua_torive<ScriptedContext>(L, 1);
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
                auto scriptedObject = scriptedContext->scriptedObject();
                auto dataContext = scriptedObject->dataContext();
                if (dataContext && dataContext->viewModelInstance())
                {
                    auto viewModelInstance = dataContext->viewModelInstance();
                    lua_newrive<ScriptedViewModel>(
                        L,
                        L,
                        ref_rcp(viewModelInstance->viewModel()),
                        viewModelInstance);
                    return 1;
                }
                return 0;
            }
            case (int)LuaAtoms::rootViewModel:
            {
                auto scriptedObject = scriptedContext->scriptedObject();
                auto dataContext = scriptedObject->dataContext();
                if (dataContext)
                {
                    auto viewModelInstance =
                        dataContext->rootViewModelInstance();
                    if (viewModelInstance)
                    {
                        lua_newrive<ScriptedViewModel>(
                            L,
                            L,
                            ref_rcp(viewModelInstance->viewModel()),
                            viewModelInstance);
                        return 1;
                    }
                }
                return 0;
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
                auto scriptedObject = scriptedContext->scriptedObject();
                auto dataContext = scriptedObject->dataContext();
                if (dataContext)
                {
                    lua_newrive<ScriptedDataContext>(L, L, dataContext);
                    return 1;
                }
                return 0;
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
