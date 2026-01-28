#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"

#include <math.h>
#include <stdio.h>

using namespace rive;

bool ScriptedRenderer::end()
{
    assert(m_renderer != nullptr);
    auto count = m_saveCount;

    while (count)
    {
        m_renderer->restore();
        count--;
    }
    // if (m_saveCount != 0)
    // {
    //     luaL_error(L,
    //                "%s save/restore stack was unbalanced with %i remaining "
    //                "saves to restore.",
    //                ScriptedRenderer::luaName,
    //                m_saveCount);
    // }
    m_renderer = nullptr;
    bool success = m_saveCount == 0;
    m_saveCount = 0;
    return success;
}

void ScriptedRenderer::save(lua_State* L)
{
    validate(L);
    m_renderer->save();
    m_saveCount++;
}

void ScriptedRenderer::restore(lua_State* L)
{
    validate(L);
    if (m_saveCount == 0)
    {
        luaL_error(L,
                   "%s save/restore stack was unbalanced by trying to restore "
                   "more times than saved.",
                   ScriptedRenderer::luaName);
    }
    m_renderer->restore();
    m_saveCount--;
}

void ScriptedRenderer::transform(lua_State* L, const Mat2D& mat2d)
{
    validate(L);
    m_renderer->transform(mat2d);
}

void ScriptedRenderer::clipPath(lua_State* L, ScriptedPathData* path)
{
    validate(L);
    m_renderer->clipPath(path->renderPath(L));
}

static int renderer_drawImage(lua_State* L)
{
    auto scriptedRenderer = lua_torive<ScriptedRenderer>(L, 1);
    auto scriptedImage = lua_torive<ScriptedImage>(L, 2);
    auto scriptedSampler = lua_torive<ScriptedImageSampler>(L, 3);
    auto blendMode = lua_toblendmode(L, 4);
    auto opacity = float(luaL_checknumber(L, 5));

    auto renderer = scriptedRenderer->validate(L);
    renderer->drawImage(scriptedImage->image.get(),
                        scriptedSampler->sampler,
                        blendMode,
                        opacity);

    return 0;
}

static int renderer_drawImageMesh(lua_State* L)
{
    auto scriptedRenderer = lua_torive<ScriptedRenderer>(L, 1);
    auto scriptedImage = lua_torive<ScriptedImage>(L, 2);
    auto scriptedSampler = lua_torive<ScriptedImageSampler>(L, 3);
    auto scriptedVertexBuffer = lua_torive<ScriptedVertexBuffer>(L, 4);
    auto scriptedUVBuffer = lua_torive<ScriptedVertexBuffer>(L, 5);
    auto scriptedTriangleBuffer = lua_torive<ScriptedTriangleBuffer>(L, 6);
    auto blendMode = lua_toblendmode(L, 7);
    auto opacity = float(luaL_checknumber(L, 8));

    // Ensure the buffers are created before drawing
    ScriptingContext* context =
        static_cast<ScriptingContext*>(lua_getthreaddata(L));
    Factory* factory = context->factory();
    scriptedVertexBuffer->update(factory);
    scriptedUVBuffer->update(factory);
    scriptedTriangleBuffer->update(factory);

    auto renderer = scriptedRenderer->validate(L);
    renderer->drawImageMesh(scriptedImage->image.get(),
                            scriptedSampler->sampler,
                            scriptedVertexBuffer->vertexBuffer,
                            scriptedUVBuffer->vertexBuffer,
                            scriptedTriangleBuffer->indexBuffer,
                            (uint32_t)scriptedVertexBuffer->values.size(),
                            (uint32_t)scriptedTriangleBuffer->values.size(),
                            blendMode,
                            opacity);

    return 0;
}

Renderer* ScriptedRenderer::validate(lua_State* L)
{
    if (m_renderer == nullptr)
    {
        luaL_error(L, "%s is no longer valid.", ScriptedRenderer::luaName);
    }
    return m_renderer;
}

static int renderer_drawPath(lua_State* L)
{
    auto scriptedRenderer = lua_torive<ScriptedRenderer>(L, 1);
    auto scriptedPath = lua_torive<ScriptedPath>(L, 2);
    auto scriptedPaint = lua_torive<ScriptedPaint>(L, 3);

    auto renderer = scriptedRenderer->validate(L);
    renderer->drawPath(scriptedPath->renderPath(L),
                       scriptedPaint->renderPaint.get());

    return 0;
}

static int renderer_save(lua_State* L)
{
    auto scriptedRenderer = lua_torive<ScriptedRenderer>(L, 1);
    scriptedRenderer->save(L);

    return 0;
}

static int renderer_restore(lua_State* L)
{
    auto scriptedRenderer = lua_torive<ScriptedRenderer>(L, 1);
    scriptedRenderer->restore(L);

    return 0;
}

static int renderer_clip_path(lua_State* L)
{
    auto scriptedRenderer = lua_torive<ScriptedRenderer>(L, 1);
    auto path = lua_torive<ScriptedPath>(L, 2);
    scriptedRenderer->clipPath(L, path);
    return 0;
}

static int renderer_transform(lua_State* L)
{
    auto scriptedRenderer = lua_torive<ScriptedRenderer>(L, 1);
    auto mat2d = lua_torive<ScriptedMat2D>(L, 2);
    scriptedRenderer->transform(L, mat2d->value);
    return 0;
}

static int renderer_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::drawPath:
                return renderer_drawPath(L);
            case (int)LuaAtoms::save:
                return renderer_save(L);
            case (int)LuaAtoms::restore:
                return renderer_restore(L);
            case (int)LuaAtoms::clipPath:
                return renderer_clip_path(L);
            case (int)LuaAtoms::transform:
                return renderer_transform(L);
            case (int)LuaAtoms::drawImage:
                return renderer_drawImage(L);
            case (int)LuaAtoms::drawImageMesh:
                return renderer_drawImageMesh(L);
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedRenderer::luaName);
    return 0;
}

int luaopen_rive_renderer(lua_State* L)
{
    lua_register_rive<ScriptedRenderer>(L);

    lua_pushcfunction(L, renderer_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable
    return 0;
}

#endif
