// Expose fields/methods.
#define private public
#define protected public

#include <fstream>
#include <cstdio>
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

	std::ifstream videoFile(destination);
	REQUIRE(videoFile.good());
	int removeErr = 0;
	removeErr = std::remove(destination.c_str());
	REQUIRE(removeErr == 0);

	std::ifstream snapshotFile(snapshotPath);
	REQUIRE(snapshotFile.good());
	removeErr = std::remove(snapshotPath.c_str());
	REQUIRE(removeErr == 0);

	delete extractor;
}
