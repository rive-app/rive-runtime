#define private public // Expose private fields/methods.
#include "catch.hpp"
#include "archive.hpp"
#include <cstdio> // std::remove()

TEST_CASE("Read the correct number of bytes")
{
	auto bytes = Archive::read_file("./static/51x50.riv");
	REQUIRE(bytes.size() == 76);
}

TEST_CASE("Non-finalized Archive doesnt create zip")
{
	auto file_bytes = Archive::read_file("./static/51x50.riv");
	std::string archive_location("./static/archive_test.zip");
	Archive* archive = new Archive(archive_location);
	archive->add_buffer("buffer.riv", file_bytes);
	delete archive;

	zip* zip_ptr_dealloc = archive->zip_archive;
	REQUIRE(zip_ptr_dealloc == nullptr);

	std::ifstream infile(archive_location);
	REQUIRE(!infile.good());
}

TEST_CASE("Empty Archive doesn't create a file")
{
	std::string archive_location("./static/archive_test.zip");
	auto arc = new Archive(archive_location);

	arc->finalize();
	delete arc;

	zip* zip_ptr_dealloc = arc->zip_archive;
	REQUIRE(zip_ptr_dealloc == nullptr);

	std::ifstream infile(archive_location);
	REQUIRE(!infile.good());
}

TEST_CASE("Test Archive file creation")
{
	auto file_bytes = Archive::read_file("./static/51x50.riv");
	std::string archive_location("./static/archive_test.zip");
	Archive* archive = new Archive(archive_location);
	archive->add_buffer("buffer.riv", file_bytes);
	REQUIRE(!archive->is_empty());
	archive->finalize();
	delete archive;

	zip* zip_ptr_dealloc = archive->zip_archive;
	REQUIRE(zip_ptr_dealloc == nullptr);

	std::ifstream infile(archive_location);
	REQUIRE(infile.good());

	// Cleanup.
	int remove_err = std::remove(archive_location.c_str());
	REQUIRE(remove_err == 0);
}