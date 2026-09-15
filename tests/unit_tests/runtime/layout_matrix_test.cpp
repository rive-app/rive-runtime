/*
 * Copyright 2026 Rive
 */

// Data-driven conformance over the generated layout matrix. Every .riv in
// assets/layout/matrix has an .expect sibling; see that folder's README.
// Adding a fixture needs no change here.

#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/node.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/layout_component.hpp"
#include "rive/transform_component.hpp"
#include "rive/world_transform_component.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/renderer.hpp"
#include "utils/no_op_factory.hpp"
#include "rive/simple_array.hpp"
#include "rive/file_asset_loader.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <cmath>
#include <catch.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <iterator>
#include <vector>

namespace
{
struct Check
{
    bool isLegacy = false;
    bool isKnownDefect = false;
    std::string component;
    std::string field;
    std::vector<float> values;
};

struct Manifest
{
    std::string fixture;
    std::string artboard;
    std::string knownDefect;
    std::vector<Check> checks;
};

// One .riv per asset set, one artboard per cell, so `F` starts a block.
std::vector<Manifest> parseManifests(const std::string& path)
{
    std::vector<Manifest> out;
    std::ifstream in(path);
    REQUIRE(in.good());
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;
        if (tag == "F")
        {
            out.emplace_back();
            ls >> out.back().fixture;
            continue;
        }
        REQUIRE(!out.empty());
        Manifest& m = out.back();
        if (tag == "A")
        {
            ls >> m.artboard;
        }
        else if (tag == "D")
        {
            std::getline(ls, m.knownDefect);
        }
        else if (tag == "C" || tag == "L" || tag == "X")
        {
            Check c;
            c.isLegacy = tag == "L";
            c.isKnownDefect = tag == "X";
            ls >> c.component >> c.field;
            float v;
            while (ls >> v)
            {
                c.values.push_back(v);
            }
            m.checks.push_back(c);
        }
    }
    return out;
}

std::vector<std::string> manifests()
{
    std::vector<std::string> out;
    const std::string dir = "assets/layout/matrix";
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (entry.path().extension() == ".expect")
        {
            out.push_back(entry.path().string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

// The tolerance the positive assertions use. Every other comparison has to use
// the same one per component: summing deltas lets several within-tolerance
// differences add up past it, which would read a corrected value as still
// wrong.
constexpr float kMargin = 1e-3f;

// Whether every component is within kMargin. A size mismatch is never a match.
bool matchesWithin(const std::vector<float>& actual,
                   const std::vector<float>& expected)
{
    if (actual.size() != expected.size())
    {
        return false;
    }
    for (size_t i = 0; i < actual.size(); i++)
    {
        const float d = std::abs(actual[i] - expected[i]);
        // Tested for non-finiteness first: `NaN > kMargin` is false, so a NaN
        // would fall through every guard and report as a match.
        if (!std::isfinite(d) || d > kMargin)
        {
            return false;
        }
    }
    return true;
}

// Returns false for a field this component type cannot report; the caller
// counts those rather than passing them silently.
bool readField(rive::Component* component,
               rive::LayoutComponent* container,
               const std::string& field,
               std::vector<float>& out)
{
    if (field == "collected")
    {
        // Asked directly rather than inferred from a solved slot.
        out = {container->collectsForLayout(component) ? 1.0f : 0.0f};
        return true;
    }
    if (field == "world")
    {
        if (!component->is<rive::WorldTransformComponent>())
        {
            return false;
        }
        const rive::Mat2D& w =
            component->as<rive::WorldTransformComponent>()->worldTransform();
        out = {w[0], w[1], w[2], w[3], w[4], w[5]};
        return true;
    }
    if (field == "contentSize")
    {
        // What the layout content-sized this non-participant to. Its own local
        // bounds, not a slot -- it has no layout node.
        if (!component->is<rive::TransformComponent>())
        {
            return false;
        }
        auto b = component->as<rive::TransformComponent>()->localBounds();
        out = {b.width(), b.height()};
        return true;
    }
    // A list takes no slot of its own; what the layout decided is per item.
    if (component->is<rive::ArtboardComponentList>())
    {
        auto* list = component->as<rive::ArtboardComponentList>();
        if (field == "itemCount")
        {
            out = {static_cast<float>(list->artboardCount())};
            return true;
        }
        if (field.size() > 4 && field.compare(0, 4, "item") == 0)
        {
            const int index = field[4] - '0';
            const std::string what = field.substr(5);
            if (index < 0 || index >= (int)list->artboardCount())
            {
                return false;
            }
            if (what == "Bounds")
            {
                auto b = list->layoutBoundsForNode(index);
                out = {b.left(), b.top(), b.width(), b.height()};
                return true;
            }
            if (what == "Pos")
            {
                auto p = list->itemPosition(index);
                out = {p.x, p.y};
                return true;
            }
        }
    }
    // Structural: with the same scale type on both axes a row and a column
    // give the same box, so geometry cannot see a wrong main axis.
    if (field == "isRow" || field == "isStack")
    {
        const bool wantRow = field == "isRow";
        if (component->is<rive::NestedArtboardLayout>())
        {
            auto* n = component->as<rive::NestedArtboardLayout>();
            out = {(wantRow ? n->isRow() : n->isStack()) ? 1.0f : 0.0f};
            return true;
        }
        if (component->is<rive::ArtboardComponentList>())
        {
            auto* l = component->as<rive::ArtboardComponentList>();
            out = {(wantRow ? l->mainAxisIsRow() : l->isStack()) ? 1.0f : 0.0f};
            return true;
        }
        return false;
    }
    // The read path data binding exposes; must agree with the transform and
    // bounds above. Anchored on a LayoutComponent, plain elsewhere.
    if (field.compare(0, 8, "computed") == 0)
    {
        if (!component->is<rive::Node>())
        {
            return false;
        }
        auto* node = component->as<rive::Node>();
        if (field == "computedLocal")
        {
            out = {node->computedLocalX(), node->computedLocalY()};
            return true;
        }
        if (field == "computedWorld")
        {
            out = {node->computedWorldX(), node->computedWorldY()};
            return true;
        }
        if (field == "computedRoot")
        {
            out = {node->computedRootX(), node->computedRootY()};
            return true;
        }
        if (field == "computedSize")
        {
            out = {node->computedWidth(), node->computedHeight()};
            return true;
        }
        return false;
    }
    // Bounds live on three unrelated declarations, so each is read where it
    // actually exists.
    auto emit = [&out](rive::AABB b) {
        out = {b.left(), b.top(), b.width(), b.height()};
        return true;
    };
    if (field == "layoutBounds")
    {
        if (component->is<rive::LayoutComponent>())
        {
            return emit(component->as<rive::LayoutComponent>()->layoutBounds());
        }
        if (auto* provider = rive::LayoutNodeProvider::from(component))
        {
            return emit(provider->layoutBounds());
        }
        return false;
    }
    if (field == "localBounds")
    {
        if (!component->is<rive::TransformComponent>())
        {
            return false;
        }
        return emit(component->as<rive::TransformComponent>()->localBounds());
    }
    if (field == "worldBounds")
    {
        if (component->is<rive::LayoutComponent>())
        {
            return emit(component->as<rive::LayoutComponent>()->worldBounds());
        }
        if (component->is<rive::Shape>())
        {
            return emit(component->as<rive::Shape>()->worldBounds());
        }
        return false;
    }
    return false;
}
// ImageAsset serialises no dimensions and NoOpFactory::decodeImage returns
// nullptr, which would size every Image cell to zero. Decodes dimensions only.
class SizedImage : public rive::RenderImage
{
public:
    SizedImage(int w, int h)
    {
        m_Width = w;
        m_Height = h;
    }
};

class MatrixFactory : public rive::NoOpFactory
{
public:
    rive::rcp<rive::RenderImage> decodeImage(
        rive::Span<const uint8_t> bytes) override
    {
        auto bitmap = Bitmap::decode(bytes.data(), bytes.size());
        if (bitmap == nullptr)
        {
            return nullptr;
        }
        return rive::make_rcp<SizedImage>(static_cast<int>(bitmap->width()),
                                          static_cast<int>(bitmap->height()));
    }
};

// Corpus assets are referenced, not embedded; resolve them from the runtime's
// own test assets so a 1.26 MB font stays out of the corpus.
class MatrixAssetLoader : public rive::FileAssetLoader
{
public:
    // Asserted below: a declared referenced asset must actually resolve, or
    // Text cells pass whether or not the font loaded.
    int resolved = 0;
    int attempted = 0;

    bool loadContents(rive::FileAsset& asset,
                      rive::Span<const uint8_t> inBandBytes,
                      rive::Factory* factory) override
    {
        attempted++;
        for (const auto& path :
             {"assets/" + asset.name() + "." + asset.fileExtension(),
              "assets/" + asset.uniqueFilename()})
        {
            std::ifstream in(path, std::ios::binary);
            if (!in.good())
            {
                continue;
            }
            std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
            rive::SimpleArray<uint8_t> bytes(raw.data(), raw.size());
            if (asset.decode(bytes, factory))
            {
                resolved++;
                return true;
            }
            return false;
        }
        return false;
    }
};

std::string join(const std::vector<float>& v)
{
    std::ostringstream os;
    for (size_t i = 0; i < v.size(); i++)
    {
        os << (i ? ", " : "") << v[i];
    }
    return os.str();
}
} // namespace

TEST_CASE("the generated layout matrix conforms", "[layout]")
{
    auto paths = manifests();
    REQUIRE(!paths.empty());

    size_t checked = 0;
    size_t knownDefectsConfirmed = 0;
    int assetsAttempted = 0;
    int assetsResolved = 0;
    std::vector<std::string> unknownFields;

    for (const auto& path : paths)
    {
        auto rivPath = path.substr(0, path.size() - strlen(".expect")) + ".riv";
        MatrixAssetLoader loader;
        MatrixFactory factory;
        auto file = ReadRiveFile(rivPath.c_str(), &factory, &loader);
        INFO("file " << rivPath);
        CHECK(loader.resolved == loader.attempted);
        // Counts are reported too: resolved == attempted also holds when
        // nothing was attempted.
        assetsAttempted += loader.attempted;
        assetsResolved += loader.resolved;

        for (const auto& manifest : parseManifests(path))
        {
            INFO("fixture " << manifest.fixture);
            auto artboard = file->artboardNamed(manifest.artboard);
            REQUIRE(artboard != nullptr);
            // A list gets its items from a data bind; unbound it reports zero.
            auto viewModelInstance =
                file->createDefaultViewModelInstance(artboard.get());
            if (viewModelInstance != nullptr)
            {
                artboard->bindViewModelInstance(viewModelInstance);
            }
            artboard->advance(0.0f);

            // With no container the artboard IS the layout the subject sits in,
            // and the one that collects it.
            rive::LayoutComponent* container =
                artboard->find<rive::LayoutComponent>("Container");
            if (container == nullptr)
            {
                container = artboard.get();
            }
            REQUIRE(container != nullptr);

            for (const auto& check : manifest.checks)
            {
                INFO("fixture " << manifest.fixture << " component "
                                << check.component << " field " << check.field);
                auto* component =
                    artboard->find<rive::Component>(check.component);
                REQUIRE(component != nullptr);

                std::vector<float> actual;
                if (!readField(component, container, check.field, actual))
                {
                    // Non-fatal so the rest of the corpus still runs, but a
                    // field this runner cannot read is a generated assertion
                    // silently going missing -- never a pass.
                    unknownFields.push_back(check.field);
                    FAIL_CHECK("no reader for field "
                               << check.field << " (component "
                               << check.component << ", fixture "
                               << manifest.fixture
                               << ") -- add one to readField()");
                    continue;
                }
                REQUIRE(actual.size() == check.values.size());

                if (check.isKnownDefect)
                {
                    // The recorded value is the correct one and the assertion
                    // is inverted: a match means it was fixed, so clear the
                    // marker.
                    if (matchesWithin(actual, check.values))
                    {
                        // Non-fatal so one run reports the whole corpus.
                        FAIL_CHECK(
                            "known defect no longer reproduces ("
                            << manifest.knownDefect
                            << ") -- clear knownDefect in the spec. fixture "
                            << manifest.fixture << " " << check.component << "."
                            << check.field << " actual [" << join(actual)
                            << "] == correct [" << join(check.values) << "]");
                    }
                    else
                    {
                        WARN("known defect confirmed ("
                             << manifest.knownDefect << ") in "
                             << manifest.fixture << ": " << check.component
                             << "." << check.field << " is [" << join(actual)
                             << "] but should be [" << join(check.values)
                             << "]");
                        knownDefectsConfirmed++;
                    }
                    checked++;
                    continue;
                }
                if (check.isLegacy)
                {
                    // Assert the value differs from what a wrong one would
                    // give.
                    CHECK(!matchesWithin(actual, check.values));
                }
                else
                {
                    for (size_t i = 0; i < actual.size(); i++)
                    {
                        INFO("index " << i);
                        // CHECK, not REQUIRE, so one run reports every
                        // mismatch.
                        CHECK(actual[i] ==
                              Approx(check.values[i]).margin(kMargin));
                    }
                }
                checked++;
            }
        }
    }

    // Guards against the corpus quietly checking nothing.
    REQUIRE(checked > 0);
    WARN("layout matrix: " << paths.size() << " files, " << checked
                           << " fields checked, " << unknownFields.size()
                           << " not yet readable by this runner, "
                           << knownDefectsConfirmed
                           << " known defects still reproducing, "
                           << assetsResolved << "/" << assetsAttempted
                           << " referenced assets resolved");
}
