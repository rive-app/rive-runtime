/*
 * Copyright 2026 Rive
 */

// A host RawPath re-enters the recorder through the per verb builders, so they
// have to drop the same zero length segments RiveRenderPath does. No GPU.

#include "rive/renderer/cmd/deferred_render_factory.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/math/raw_path.hpp"

#include <catch.hpp>

using namespace rive;

TEST_CASE("recorded per verb geometry drops empty segments",
          "[cmd][deferred][path]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();

    // A closed contour of coincident points: what a zero sized shape hands us.
    RawPath degenerate;
    degenerate.move({304, 160});
    degenerate.line({304, 160});
    degenerate.line({304, 160});
    degenerate.line({304, 160});
    degenerate.close();
    degenerate.addTo(path.get());

    // Draws flush the pending per verb geometry into the stream.
    session.screenRenderer()->drawPath(path.get(), paint.get());

    // This path's verbs are the only blob the session recorded, and they lead
    // it: move then close, with the three empty lines gone.
    Span<const uint8_t> blobs = session.commandBuffer().blobBytes();
    REQUIRE(blobs.size() >= 2);
    CHECK(static_cast<PathVerb>(blobs[0]) == PathVerb::move);
    CHECK(static_cast<PathVerb>(blobs[1]) == PathVerb::close);
}
