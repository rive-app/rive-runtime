#define private public // Expose private fields/methods.
#include "catch.hpp"
#include "recorder_arguments.hpp"
#include "render_format.hpp"
#include <cstdio> // std::remove()

TEST_CASE("All defaults are correct")
{
	const char* argsVector[] = {"rive_recorder",
	                            "-s",
	                            "~/source/file.riv",
	                            "-d",
	                            "~/destination/out.mp4",
	                            "-w"
	                            "~/watermark.png",
	                            "--snapshot-path",
	                            "~/snapshot.png"};
	unsigned int argc = sizeof(argsVector) / sizeof(argsVector[0]);
	RecorderArguments args(argc, argsVector);

	REQUIRE(args.fps() == 60);
	REQUIRE(args.bitrate() == 0);
	REQUIRE(args.height() == 0);
	REQUIRE(args.maxDuration() == 0);
	REQUIRE(args.maxHeight() == 0);
	REQUIRE(args.maxWidth() == 0);
	REQUIRE(args.minDuration() == 0);
	REQUIRE(args.numLoops() == 1);
	REQUIRE(args.smallExtentTarget() == 0);
	REQUIRE(args.width() == 0);
	REQUIRE(args.animation().empty());
	REQUIRE(args.artboard().empty());
	REQUIRE(args.renderFormat() == RenderFormat::h264);
}

TEST_CASE("Reads the correct arguments and defaults")
{
	const char* argsVector[] = {"rive_recorder",
	                            "-s",
	                            "~/source/file.riv",
	                            "-d",
	                            "~/destination/out.mp4",
	                            "--max-height",
	                            "0",
	                            "--max-width",
	                            "0",
	                            "--min-duration",
	                            "0",
	                            "--max-duration",
	                            "30",
	                            "-w"
	                            "~/watermark.png",
	                            "--width",
	                            "0",
	                            "--height",
	                            "0",
	                            "--fps",
	                            "60",
	                            "--num-loops",
	                            "3",
	                            "--snapshot-path",
	                            "~/snapshot.png"};
	unsigned int argc = sizeof(argsVector) / sizeof(argsVector[0]);
	RecorderArguments args(argc, argsVector);

	REQUIRE(args.fps() == 60);
	REQUIRE(args.bitrate() == 0);
	REQUIRE(args.height() == 0);
	REQUIRE(args.maxDuration() == 30);
	REQUIRE(args.maxHeight() == 0);
	REQUIRE(args.maxWidth() == 0);
	REQUIRE(args.minDuration() == 0);
	REQUIRE(args.numLoops() == 3);
	REQUIRE(args.smallExtentTarget() == 0);
	REQUIRE(args.width() == 0);
	REQUIRE(args.animation().empty());
	REQUIRE(args.artboard().empty());
	REQUIRE(args.destination() == "~/destination/out.mp4");
	REQUIRE(args.snapshotPath() == "~/snapshot.png");
	REQUIRE(args.source() == "~/source/file.riv");
	REQUIRE(args.watermark() == "~/watermark.png");
	REQUIRE(args.renderFormat() == RenderFormat::h264);
}