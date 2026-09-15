/*
 * Copyright 2026 Rive
 *
 * The GM ore_layout_reuse shows an authored layout working across two
 * pipelines. These assert object identity instead, which no golden can show.
 */

#include "common/testing_window.hpp"
#include "gm/ore_gm_helper.hpp"
#include <catch.hpp>

#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_VK) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_D3D12)
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#define ORE_LAYOUT_INTERN_TEST_ACTIVE
#endif

#ifdef ORE_LAYOUT_INTERN_TEST_ACTIVE
using namespace rive;
using namespace rive::ore;

namespace
{
const TestingWindow::Backend kOreBackends[] = {
#if defined(ORE_BACKEND_METAL)
    TestingWindow::Backend::metal,
#endif
#if defined(ORE_BACKEND_D3D12)
    TestingWindow::Backend::d3d12,
#endif
#if defined(ORE_BACKEND_VK)
    TestingWindow::Backend::vk,
#endif
#if defined(ORE_BACKEND_WGPU)
    TestingWindow::Backend::dawn,
#endif
};

void runScenario(TestingWindow* window)
{
    auto* renderContext = window->renderContext();
    ore_gm::OreGMContext oreGM;
    REQUIRE(oreGM.ensureContext(renderContext));
    auto& ctx = *renderContext->getOreContext();

    auto shader = ore_gm::loadShader(ctx, ore_gm::kBindingWitness);
    REQUIRE(shader.vsModule);

    // Without a baked id every assertion below would pass vacuously.
    REQUIRE(shader.vsModule->m_bindingMap.layoutIdForGroup(0) !=
            BindingMap::kNoLayoutId);

    auto first = makeBindGroupLayoutFromShader(ctx, shader.vsModule.get(), 0);
    REQUIRE(first != nullptr);
    CHECK(makeBindGroupLayoutFromShader(ctx, shader.vsModule.get(), 0).get() ==
          first.get());

    // The payoff: a pipeline with its own module still binds against the
    // first one's bind groups.
    auto other = ore_gm::loadShader(ctx, ore_gm::kBindingWitness);
    REQUIRE(other.vsModule);
    REQUIRE(other.vsModule.get() != shader.vsModule.get());
    CHECK(makeBindGroupLayoutFromShader(ctx, other.vsModule.get(), 0).get() ==
          first.get());

    auto multi = ore_gm::loadShader(ctx, ore_gm::kMultiGroupWitness);
    REQUIRE(multi.vsModule);
    const BindingMap& multiMap = multi.vsModule->m_bindingMap;
    REQUIRE(multiMap.layoutIdForGroup(0) != multiMap.layoutIdForGroup(1));
    auto g0 = makeBindGroupLayoutFromShader(ctx, multi.vsModule.get(), 0);
    auto g1 = makeBindGroupLayoutFromShader(ctx, multi.vsModule.get(), 1);
    REQUIRE(g0 != nullptr);
    REQUIRE(g1 != nullptr);
    CHECK(g0.get() != g1.get());

    // A short buffer still reports what the group needs, so no caller can
    // build a layout missing bindings.
    BindGroupLayoutEntry room[16]{};
    const uint32_t needed =
        populateBindGroupLayoutEntriesFromShader(room,
                                                 16,
                                                 shader.vsModule.get(),
                                                 0);
    REQUIRE(needed > 0);
    CHECK(populateBindGroupLayoutEntriesFromShader(nullptr,
                                                   0,
                                                   shader.vsModule.get(),
                                                   0) == needed);

    // A dynamic offset is script-authored, so the baked id does not cover it.
    const uint32_t dynamicBindings[] = {0};
    auto dynamic = makeBindGroupLayoutFromShader(ctx,
                                                 shader.vsModule.get(),
                                                 0,
                                                 dynamicBindings,
                                                 1);
    REQUIRE(dynamic != nullptr);
    CHECK(dynamic.get() != first.get());
    // And it must not have poisoned the table for later plain requests.
    CHECK(makeBindGroupLayoutFromShader(ctx, shader.vsModule.get(), 0).get() ==
          first.get());
}
} // namespace

TEST_CASE("ore layouts intern by baked id", "[ore]")
{
    int ran = 0;
    for (auto backend : kOreBackends)
    {
        auto* window = TestingWindow::Init(backend,
                                           {},
                                           TestingWindow::Visibility::headless);
        if (window == nullptr || window->renderContext() == nullptr ||
            !ore_gm::isOreBackendActive())
        {
            TestingWindow::Destroy();
            continue;
        }
        INFO("backend " << TestingWindow::BackendName(backend));
        runScenario(window);
        ++ran;
        TestingWindow::Destroy();
    }
    if (ran == 0)
        WARN("no Ore backend available headless; skipping");
}
#endif // ORE_LAYOUT_INTERN_TEST_ACTIVE
