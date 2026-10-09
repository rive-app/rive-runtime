#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/generated/viewmodel/viewmodel_instance_viewmodel_base.hpp"
#include "riv_bytes.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// ViewModelInstanceViewModel::import ignored its Super::import status. With no
// ViewModelInstance open, Super fails on the missing ViewModelInstanceImporter,
// but inside an artboard the import carried on to look up the instance through
// that same importer and dereferenced null.
TEST_CASE("a view model instance view model with no instance open fails "
          "its import",
          "[file][malformed]")
{
    RivBytes riv;
    writeArtboard(riv);
    // No ViewModelInstance has been read, so there is nothing to belong to.
    riv.object(rive::ViewModelInstanceViewModelBase::typeKey);
    riv.end();

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file != nullptr);
    REQUIRE(file->artboard() != nullptr);
    // Dropped, but its index is kept.
    auto& objects = file->artboard()->objects();
    REQUIRE(objects.size() == 2);
    CHECK(objects[1] == nullptr);
}
