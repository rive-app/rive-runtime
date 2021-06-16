// Expose fields/methods.
#define private public
#define protected public

#include <fstream>
#include <cstdio>
#include "catch.hpp"
#include "recorder_arguments.hpp"
#include "extractor/png_extractor.hpp"

TEST_CASE("Generate a zip archive containing a PNG sequence from a riv file")
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
	                            "-w"
	                            "./static/watermark.png",
	                            "--width",
	                            "0",
	                            "--height",
	                            "0",
	                            "--fps",
	                            "60",
	                            "--num-loops",
	                            "3",
	                            "--snapshot-path",
	                            "./static/snapshot.png"};
	unsigned int argc = sizeof(argsVector) / sizeof(argsVector[0]);
	RecorderArguments args(argc, argsVector);

	auto source = args.source();
	auto artboard = args.artboard();
	auto animation = args.animation();
	auto watermark = args.watermark();
	auto destination = args.destination();
	auto width = args.width();
	auto height = args.height();
	auto smallExtentTarget = args.smallExtentTarget();
	auto maxWidth = args.maxWidth();
	auto maxHeight = args.maxHeight();
	auto duration = args.duration();
	auto minDuration = args.minDuration();
	auto maxDuration = args.maxDuration();
	auto fps = args.fps();

	PNGExtractor extractor(source,
	                       artboard,
	                       animation,
	                       watermark,
	                       destination,
	                       width,
	                       height,
	                       smallExtentTarget,
	                       maxWidth,
	                       maxHeight,
	                       duration,
	                       minDuration,
	                       maxDuration,
	                       fps);
	auto snapshotPath = args.snapshotPath();
	extractor.takeSnapshot(snapshotPath);
	extractor.extractFrames(args.numLoops());

	std::ifstream videoFile(destination);
	REQUIRE(videoFile.good());
	// int removeErr = std::remove(destination.c_str());
	// REQUIRE(removeErr == 0);

	std::ifstream snapshotFile(snapshotPath);
	REQUIRE(snapshotFile.good());
	std::cout << "PNG Extracted alright!" << std::endl;
	// removeErr = std::remove(snapshotPath.c_str());
	// REQUIRE(removeErr == 0);
}
