#define private public // Expose private fields/methods.
#include "catch.hpp"
#include "archive.hpp"
#include <cstdio> // std::remove()

TEST_CASE("Read the correct number of bytes", "[.]")
{
	auto bytes = Archive::readFile("./static/51x50.riv");
	REQUIRE(bytes.size() == 76);
}

TEST_CASE("Empty Archive doesn't create a file", "[.]")
{
	std::string archive_location("./static/archive_test.zip");
	auto arc = new Archive(archive_location);
	delete arc;

	zip* zip_ptr_dealloc = arc->m_ZipArchive;
	REQUIRE(zip_ptr_dealloc == nullptr);

	std::ifstream infile(archive_location);
	REQUIRE(!infile.good());
}

TEST_CASE("Test Archive file creation", "[.]")
{
	auto file_bytes = Archive::readFile("./static/51x50.riv");
	std::string archive_location("./static/archive_test.zip");
	Archive* archive = new Archive(archive_location);
	archive->addBuffer("buffer.riv", file_bytes);
	archive->openArchive(ZIP_RDONLY);
	REQUIRE(archive->m_ZipArchive != nullptr);
	REQUIRE(!archive->isEmpty());
	delete archive;

	zip* zip_ptr_dealloc = archive->m_ZipArchive;
	REQUIRE(zip_ptr_dealloc == nullptr);

	std::ifstream infile(archive_location);
	REQUIRE(infile.good());

	// Cleanup.
	int remove_err = std::remove(archive_location.c_str());
	REQUIRE(remove_err == 0);
}