#include <rive/file.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_blob.hpp>
#include <rive/data_bind/data_values/data_value_asset_blob.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/assets/blob_asset.hpp>
#include <rive/simple_array.hpp>
#include <rive_file_reader.hpp>
#include <utils/serializing_factory.hpp>
#include <catch.hpp>
#include <cstdio>
#include <cstring>

using namespace rive;

static rcp<BlobAsset> makeBlob(const char* name, std::vector<uint8_t> data)
{
    auto blob = make_rcp<BlobAsset>();
    blob->name(name);
    SimpleArray<uint8_t> bytes(data.data(), data.size());
    blob->decode(bytes, nullptr);
    return blob;
}

// Deterministic coverage of the ViewModelInstanceAssetBlob value API: assigning
// a blob stores it on the property, swapping updates it, and null clears it.
TEST_CASE("Blob data bind stores and clears the blob on the property",
          "[data binding]")
{
    ViewModelInstanceAssetBlob blobProperty;
    // A freshly constructed (unset / id-bound) property has no backing blob.
    CHECK(blobProperty.asset() == nullptr);

    auto blob = makeBlob("first", {1, 2, 3, 4});
    blobProperty.value(blob.get());
    REQUIRE(blobProperty.asset() != nullptr);
    CHECK(blobProperty.asset()->bytes().size() == 4);
    CHECK(blobProperty.asset().get() == blob.get());

    auto blob2 = makeBlob("second", {9, 9});
    blobProperty.value(blob2.get());
    CHECK(blobProperty.asset().get() == blob2.get());
    CHECK(blobProperty.asset()->bytes().size() == 2);

    blobProperty.value(nullptr);
    CHECK(blobProperty.asset() == nullptr);
}

// DataValueAssetBlob is the value carried across a bind; applyValue should read
// the blob out of it and store it on the instance property.
TEST_CASE("Blob data value applies to the instance property", "[data binding]")
{
    ViewModelInstanceAssetBlob blobProperty;

    auto blob = makeBlob("payload", {5, 6, 7});
    DataValueAssetBlob dataValue;
    dataValue.blobValue(blob.get());
    CHECK(dataValue.blobValue() == blob.get());

    blobProperty.applyValue(&dataValue);
    REQUIRE(blobProperty.asset() != nullptr);
    CHECK(blobProperty.asset().get() == blob.get());
    CHECK(blobProperty.asset()->bytes().size() == 3);
}

// Regression: an id-based bind carries only an asset id (no live blob). The
// data value's blobValue() must be null so applyValue writes propertyValue
// instead of taking the live-blob branch and dropping the id.
TEST_CASE("Blob data value with only an id applies the id", "[data binding]")
{
    ViewModelInstanceAssetBlob blobProperty;

    DataValueAssetBlob dataValue(7);
    CHECK(dataValue.blobValue() == nullptr);

    blobProperty.applyValue(&dataValue);
    CHECK(blobProperty.propertyValue() == 7);
}

// The live-vs-id-bound distinction is carried by asset() being non-null, not by
// byte count: a fresh/id-bound property has a null asset, while any
// directly-set blob — including a legitimately empty one — produces a non-null
// asset.
TEST_CASE("Directly-set blob yields a non-null asset even when empty",
          "[data binding]")
{
    ViewModelInstanceAssetBlob blobProperty;
    CHECK(blobProperty.asset() == nullptr);

    // A blob with bytes is directly set.
    auto live = makeBlob("live", {1, 2, 3});
    blobProperty.value(live.get());
    REQUIRE(blobProperty.asset() != nullptr);
    CHECK_FALSE(blobProperty.asset()->bytes().empty());

    // A legitimately empty blob (e.g. `prop.value = ""`) is still a runtime
    // value: the asset is non-null with zero bytes, not treated as id-bound.
    auto empty = makeBlob("empty", {});
    blobProperty.value(empty.get());
    REQUIRE(blobProperty.asset() != nullptr);
    CHECK(blobProperty.asset().get() == empty.get());
    CHECK(blobProperty.asset()->bytes().empty());
}

TEST_CASE("Data bind blobs internally and externally", "[silver]")
{
    SerializingFactory silver;
    auto file = ReadRiveFile("assets/data_bind_blob_test.riv", &silver);

    auto artboard = file->artboardDefault();

    silver.frameSize(artboard->width(), artboard->height());

    auto renderer = silver.makeRenderer();

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createViewModelInstance(artboard.get()->viewModelId(), 0);
    auto blobProp = vmi->propertyValue("xml")->as<ViewModelInstanceAssetBlob>();

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.1f);
    artboard->draw(renderer.get());

    int frames = (int)(2.0f / 0.5f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.5f);
        artboard->draw(renderer.get());
    }

    // Load an external file and set it as the blob property's value.
    std::vector<uint8_t> blobBytes = ReadFile("assets/data_enum_roundtrip.rml");
    auto blob = make_rcp<BlobAsset>();
    blob->name("data_enum_roundtrip.rml");
    SimpleArray<uint8_t> bytes(blobBytes.data(), blobBytes.size());
    blob->decode(bytes, nullptr);
    blobProp->value(blob.get());
    CHECK(blobProp->asset().get() == blob.get());
    CHECK(blobProp->asset()->bytes().size() == blobBytes.size());
    silver.addFrame();
    stateMachine->advanceAndApply(0.5f);
    artboard->draw(renderer.get());

    CHECK(silver.matches("data_bind_blob_test"));
}