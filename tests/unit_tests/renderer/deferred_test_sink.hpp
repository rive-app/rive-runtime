/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "utils/serializing_factory.hpp"

#include <memory>
#include <unordered_map>

namespace deferred_test
{
// GPU free sink over a serializing factory. Canvas content is unsupported so
// those draws drop without counting. FactoryT may subclass SerializingFactory
// to observe replay side effects.
template <typename FactoryT = rive::SerializingFactory>
class TestSinkT : public rive::cmd::DeferredFrameSink
{
public:
    FactoryT serializingFactory;

    rive::Factory* factory() override { return &serializingFactory; }
    rive::ore::Context* oreContext() override { return nullptr; }
    // A serialized frame per target, so a multi target replay is legible as
    // separate frames instead of one merged stream.
    rive::Renderer* beginScreenFrame(uint64_t target) override
    {
        serializingFactory.frameSize(256, 256);
        serializingFactory.addFrame();
        auto& screen = m_screens[target];
        screen = serializingFactory.makeRenderer();
        return screen.get();
    }

    size_t openedTargets() const { return m_screens.size(); }

private:
    std::unordered_map<uint64_t, std::unique_ptr<rive::Renderer>> m_screens;
};

using TestSink = TestSinkT<>;
} // namespace deferred_test
