// Expose fields/methods.
#define private public
#define protected public

#include <fstream>
#include <cstdio>
#include <cmath>
#include <chrono>
#include "catch.hpp"
#include "recorder_arguments.hpp"
#include "extractor/extractor_factory.hpp"
#include "extractor/png_extractor.hpp"

// Hidden test (tag starts with a .)
// Run this test with cmd:
// 	$ ./test.sh "[.perf]"
TEST_CASE("PNG Sequence performance testing.", "[.perf]")
{
	const char* argsVector[] = {"rive_recorder",
	                            "-s",
	                            "./static/transparent.riv",
	                            "-d",
	                            "./static/out/transparent.zip",
	                            "--duration",
	                            "100",
	                            "--snapshot-path",
	                            "./static/out/alpha_snap.png",
	                            "-w"
	                            "./static/watermark.png",
	                            "--format",
	                            "png_sequence"};
	unsigned int argc = sizeof(argsVector) / sizeof(argsVector[0]);

	RecorderArguments args(argc, argsVector);
	auto destination = args.destination();

	std::ifstream destinationFile(destination);
	if (destinationFile.good())
	{
		std::remove(destination.c_str());
	}

	PNGExtractor* extractor = (PNGExtractor*)(makeExtractor(args));

	// Timer start:
	std::chrono::steady_clock::time_point begin =
	    std::chrono::steady_clock::now();
	extractor->extractFrames(args.numLoops());
	// Timer end:
	std::chrono::steady_clock::time_point end =
	    std::chrono::steady_clock::now();

	std::cout << "Time difference = "
	          << std::chrono::duration_cast<std::chrono::milliseconds>(end -
	                                                                   begin)
	                 .count()
	          << "[ms]" << std::endl;

	std::ifstream zipFile(destination);
	REQUIRE(zipFile.good());

	delete extractor;
}

TEST_CASE("Check the correct number of digits", "[num_digits]")
{
	int i = 1;
	while (i <= 10)
	{
		unsigned powerOfTen = std::round(pow(10, i - 1));
		unsigned iRand = (rand() % 9) + 1; // num btwn 1-9
		unsigned val = iRand * powerOfTen;
		auto numDigits = PNGExtractor::numDigits(val);
		REQUIRE(numDigits == i);
		i++;
	}
}

TEST_CASE("Generate the zero-padded names", "[zero_pad]")
{
	char durationChar[3];
	unsigned duration = 12;
	sprintf(durationChar, "%d", duration);

	char loopsChar[3];
	unsigned numLoops = 17;
	sprintf(loopsChar, "%d", numLoops);
	const char* argsVector[] = {"rive_recorder",
	                            "-s",
	                            "./static/transparent.riv",
	                            "-d",
	                            "./static/out/transparent.zip",
	                            "--duration",
	                            "100",
	                            "--snapshot-path",
	                            "./static/out/alpha_snap.png",
	                            "-w"
	                            "./static/watermark.png",
	                            "--format",
	                            "png_sequence",
	                            "--duration",
	                            durationChar,
	                            "--num-loops",
	                            loopsChar};
	unsigned int argc = sizeof(argsVector) / sizeof(argsVector[0]);
	RecorderArguments args(argc, argsVector);
	auto destination = args.destination();

	PNGExtractor* extractor = (PNGExtractor*)(makeExtractor(args));
	extractor->extractFrames(args.numLoops());

	// This is the number of the last PNG frame that was extracted.
	unsigned totalFrames = (extractor->totalFrames() * numLoops) - 1;
	REQUIRE(extractor->m_Digits == PNGExtractor::numDigits(totalFrames));

	std::ifstream zipFile(destination);
	REQUIRE(zipFile.good());

	// Check the archive.
	mz_zip_archive zip_archive;
	memset(&zip_archive, 0, sizeof(zip_archive));
	mz_bool status =
	    mz_zip_reader_init_file(&zip_archive, destination.c_str(), 0);
	REQUIRE(status);

	// Check that first and last file have the right names.
	mz_zip_archive_file_stat file_stat;
	status = mz_zip_reader_file_stat(&zip_archive, 0, &file_stat);
	REQUIRE(status);
	REQUIRE(strcmp(file_stat.m_filename, "000.png") == 0);

	int numFiles = mz_zip_reader_get_num_files(&zip_archive);
	memset(&file_stat, 0, sizeof(file_stat));
	status = mz_zip_reader_file_stat(&zip_archive, numFiles - 1, &file_stat);
	REQUIRE(status);
	REQUIRE(strcmp(file_stat.m_filename, "203.png") == 0);
	// Free archive.
	mz_zip_reader_end(&zip_archive);
	// Remove the file.
	int removeErr = 0;
	removeErr = std::remove(destination.c_str());
	REQUIRE(removeErr == 0);
}

TEST_CASE("Generate a zip archive containing a PNG sequence from a riv file",
          "[zip_archive]")
{
	const char* argsVector[] = {"rive_recorder",
	                            "-s",
	                            "./static/animations.riv",
	                            "-d",
	                            "./static/png_out.zip",
	                            "--max-height",
	                            "0",
	                            "--max-width",
	                            "0",
	                            "--min-duration",
	                            "0",
	                            "--max-duration",
	                            "30",
	                            "--duration",
	                            "20",
	                            "-w"
	                            "./static/watermark.png",
	                            "--width",
	                            "0",
	                            "--height",
	                            "0",
	                            "--fps",
	                            "60",
	                            "--snapshot-path",
	                            "./static/snapshot.png",
	                            "--format",
	                            "png_sequence"};
	unsigned int argc = sizeof(argsVector) / sizeof(argsVector[0]);

	RecorderArguments args(argc, argsVector);
	auto destination = args.destination();

	PNGExtractor* extractor = (PNGExtractor*)(makeExtractor(args));
	auto snapshotPath = args.snapshotPath();
	extractor->takeSnapshot(snapshotPath);
	extractor->extractFrames(args.numLoops());

	std::ifstream zipFile(destination);
	REQUIRE(zipFile.good());
	int removeErr = 0;
	removeErr = std::remove(destination.c_str());
	REQUIRE(removeErr == 0);

	std::ifstream snapshotFile(snapshotPath);
	REQUIRE(snapshotFile.good());
	removeErr = std::remove(snapshotPath.c_str());
	REQUIRE(removeErr == 0);

	delete extractor;
}
