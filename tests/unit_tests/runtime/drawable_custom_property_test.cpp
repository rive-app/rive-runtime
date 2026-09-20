#include <rive/artboard.hpp>
#include "rive_file_reader.hpp"
#include <rive/file.hpp>
#include <rive/bindable_artboard.hpp>
#include <rive/assets/manifest_asset.hpp>
#include <rive/custom_property_number.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include <catch.hpp>
#include <algorithm>
#include <vector>

using namespace rive;

namespace
{
// Object ids follow addObject order, the artboard itself being 0.
struct ArtboardBuilder
{
    Artboard& artboard;
    uint32_t nextId = 1;

    uint32_t add(Component* component, uint32_t parentId)
    {
        component->parentId(parentId);
        artboard.addObject(component);
        return nextId++;
    }

    uint32_t addShape()
    {
        uint32_t shapeId = add(new Shape(), 0);
        auto rectangle = new Rectangle();
        rectangle->width(10.0f);
        rectangle->height(10.0f);
        add(rectangle, shapeId);
        return shapeId;
    }
};

struct Visits
{
    std::vector<Drawable*> drawables;
};

void recordVisit(void* context, Drawable* drawable, Renderer* renderer)
{
    static_cast<Visits*>(context)->drawables.push_back(drawable);
    drawable->draw(renderer);
}
} // namespace

TEST_CASE("draw visitor sees only drawables tagged with custom properties",
          "[drawable][custom_property]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    artboard.addObject(&artboard);

    ArtboardBuilder builder{artboard};
    builder.addShape();
    uint32_t taggedId = builder.addShape();
    auto emissive = new CustomPropertyNumber();
    emissive->nameId(3);
    emissive->propertyValue(0.5f);
    builder.add(emissive, taggedId);
    // Beside the tag, as a tagged scripted drawable holds its inputs.
    auto input = new CustomPropertyNumber();
    input->name("input");
    builder.add(input, taggedId);
    // A named property, the shape of a script input, tags nothing.
    uint32_t inputHolderId = builder.addShape();
    auto named = new CustomPropertyNumber();
    named->name("speed");
    builder.add(named, inputHolderId);

    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);

    auto tagged = artboard.resolve(taggedId)->as<Shape>();
    REQUIRE(tagged->hasCustomProperties());
    CHECK(tagged->customProperty(3) == emissive);
    CHECK(tagged->customProperty(4) == nullptr);
    // A missing name's key is what an untagged property holds: it finds none,
    // not the input sitting beside the tag.
    REQUIRE(input->parent() == tagged);
    CHECK(input->nameId() == CustomProperty::noNameId);
    CHECK(tagged->customProperty(CustomProperty::noNameId) == nullptr);
    CHECK(emissive->kind() == CustomPropertyKind::number);
    CHECK(!artboard.resolve(inputHolderId)->as<Shape>()->hasCustomProperties());

    NoOpRenderer renderer;
    Visits visits;
    artboard.drawInternal(&renderer, recordVisit, &visits);
    REQUIRE(visits.drawables.size() == 1);
    CHECK(visits.drawables[0] == tagged);

    visits.drawables.clear();
    artboard.drawInternal(&renderer);
    CHECK(visits.drawables.empty());
}

namespace
{
struct ModulationRecorder : public NoOpRenderer
{
    std::vector<ColorInt> colors;
    int depth = 0;
    void save() override { depth++; }
    void restore() override { depth--; }
    void modulateColor(ColorInt color, bool replace) override
    {
        if (replace)
        {
            colors.push_back(color);
        }
    }
};
} // namespace

TEST_CASE("drawModulated sets the color from each tagged drawable",
          "[drawable][custom_property]")
{
    NoOpFactory factory;
    Artboard artboard(&factory);
    artboard.addObject(&artboard);

    ArtboardBuilder builder{artboard};
    builder.addShape();
    auto dim = new CustomPropertyNumber();
    dim->nameId(3);
    dim->propertyValue(0.4f);
    builder.add(dim, builder.addShape());
    auto bright = new CustomPropertyNumber();
    bright->nameId(3);
    bright->propertyValue(3.0f);
    builder.add(bright, builder.addShape());
    // Tagged, but not with the key being drawn.
    auto other = new CustomPropertyNumber();
    other->nameId(9);
    builder.add(other, builder.addShape());

    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);

    ModulationRecorder renderer;
    artboard.drawModulated(&renderer, 3);
    std::sort(renderer.colors.begin(), renderer.colors.end());
    CHECK(renderer.colors == std::vector<ColorInt>{0xFF666666, 0xFFFFFFFF});
    CHECK(renderer.depth == 0);
}

namespace
{
// Hosts another file's artboard from inside the first visit, the way a
// nested artboard bound in through a view model draws it.
struct HostingVisit
{
    Artboard* host;
    Artboard* hosted;
    bool didHost = false;
    std::vector<Drawable*> visited;
};

void hostingVisitor(void* context, Drawable* drawable, Renderer* renderer)
{
    auto visit = static_cast<HostingVisit*>(context);
    visit->visited.push_back(drawable);
    if (!visit->didHost)
    {
        visit->didHost = true;
        visit->host->drawHosted(visit->hosted, renderer);
    }
    drawable->draw(renderer);
}
} // namespace

TEST_CASE("a property key does not reach into an artboard from another file",
          "[drawable][custom_property]")
{
    auto file = ReadRiveFile("assets/drawable_custom_properties.riv");
    auto otherFile =
        ReadRiveFile("assets/drawable_custom_properties_other.riv");
    REQUIRE(file->manifest() != nullptr);
    REQUIRE(otherFile->manifest() != nullptr);

    // Name ids are per file: the id `emissive` has here names something
    // unrelated in the other file.
    int emissive = file->manifest()->nameId("emissive", 8);
    REQUIRE(emissive >= 0);
    CHECK(otherFile->manifest()->nameId("emissive", 8) == -1);
    CHECK(otherFile->manifest()->resolveName(emissive) == "unrelated");

    auto host = file->artboardDefault();
    auto hosted = otherFile->bindableArtboardDefault();
    REQUIRE(host != nullptr);
    REQUIRE(hosted != nullptr);
    host->advance(0.0f);
    hosted->artboard()->advance(0.0f);
    auto unrelated = hosted->artboard()->find<Shape>("unrelated");
    REQUIRE(unrelated != nullptr);
    // The collision itself: the first file's key finds this property.
    REQUIRE(unrelated->customProperty((uint32_t)emissive) != nullptr);

    NoOpRenderer renderer;
    HostingVisit visit = {host.get(), hosted->artboard()};
    host->drawInternal(&renderer, hostingVisitor, &visit);
    REQUIRE(visit.didHost);
    CHECK(std::find(visit.visited.begin(), visit.visited.end(), unrelated) ==
          visit.visited.end());

    // An instance a script makes does not know its file, so its host names
    // the file the keys belong to. An artboard bound in from that same file
    // is still visited, one from another file still is not.
    auto scripted = file->artboard()->instance();
    scripted->advance(0.0f);
    auto sameFile = file->bindableArtboardDefault();
    sameFile->artboard()->advance(0.0f);
    auto dim = sameFile->artboard()->find<Shape>("dim");
    REQUIRE(dim != nullptr);
    for (auto bound : {sameFile->artboard(), hosted->artboard()})
    {
        HostingVisit scriptedVisit = {scripted.get(), bound};
        scripted->drawInternal(&renderer,
                               hostingVisitor,
                               &scriptedVisit,
                               file.get());
        REQUIRE(scriptedVisit.didHost);
        auto sees = [&](Drawable* drawable) {
            return std::find(scriptedVisit.visited.begin(),
                             scriptedVisit.visited.end(),
                             drawable) != scriptedVisit.visited.end();
        };
        CHECK(sees(dim) == (bound == sameFile->artboard()));
        CHECK(!sees(unrelated));
    }
}
