// Expose fields/methods.
#define private public
#define protected public

#include <fstream>
#include <cstdio>
#include "catch.hpp"
#include "extractor/video_extractor.hpp"

RiveFrameExtractor* makeExtractor(RecorderArguments& args);

TEST_CASE("Test extractor source not found")
{
	REQUIRE_THROWS_WITH(new VideoExtractor("missing.riv", // source
	                                       "",            // artboard
	                                       "",            // animation
	                                       "",            // watermark
	                                       "",            // destination
	                                       0,             // width
	                                       0              // height
	                                       ),
	                    "Failed to open file missing.riv");
}

TEST_CASE("Test extractor no animation")
{
	REQUIRE_THROWS_WITH(
	    new VideoExtractor("./static/no_animation.riv", // source
	                       "",                          // artboard
	                       "",                          // animation
	                       "",                          // watermark
	                       "",                          // destination
	                       0,                           // width
	                       0                            // height
	                       ),
	    "Artboard doesn't contain a default animation.");
}

TEST_CASE("Test extractor no artboard")
{
	REQUIRE_THROWS_WITH(new VideoExtractor("./static/no_artboard.riv", // source
	                                       "", // artboard
	                                       "", // animation
	                                       "", // watermark
	                                       "", // destination
	                                       0,  // width
	                                       0   // height
	                                       ),
	                    "File doesn't contain a default artboard.");
}

TEST_CASE("Test extractor odd width")
{
	VideoExtractor rive("./static/51x50.riv", // source
	                    "",                   // artboard
	                    "",                   // animation
	                    "",                   // watermark
	                    "fake.mp4",           // destination
	                    0,                    // width
	                    0                     // height
	);
	REQUIRE(rive.width() == 52);
	REQUIRE(rive.height() == 50);
}

TEST_CASE("Test extractor odd height")
{
	VideoExtractor rive("./static/50x51.riv", // source
	                    "",                   // artboard
	                    "",                   // animation
	                    "",                   // watermark
	                    "fake.mp4",           // destination
	                    0,                    // width
	                    0                     // height
	);
	REQUIRE(rive.width() == 50);
	REQUIRE(rive.height() == 52);
}

TEST_CASE("Test extractor width override")
{
	VideoExtractor rive("./static/50x51.riv", // source
	                    "",                   // artboard
	                    "",                   // animation
	                    "",                   // watermark
	                    "fake.mp4",           // destination
	                    100,                  // width
	                    0                     // height
	);
	REQUIRE(rive.width() == 100);
	REQUIRE(rive.height() == 52);
}

TEST_CASE("Test extractor height override")
{
	VideoExtractor rive("./static/50x51.riv", // source
	                    "",                   // artboard
	                    "",                   // animation
	                    "",                   // watermark
	                    "fake.mp4",           // destination
	                    0,                    // width
	                    100                   // height
	);
	REQUIRE(rive.width() == 50);
	REQUIRE(rive.height() == 100);
}

TEST_CASE("Test small extent target (width)")
{
	VideoExtractor rive("./static/50x51.riv", // source
	                    "",                   // artboard
	                    "",                   // animation
	                    "",                   // watermark
	                    "fake.mp4",           // destination
	                    50,                   // width
	                    100,                  // height
	                    720                   // smallExtentTarget
	);
	REQUIRE(rive.width() == 720);
	REQUIRE(rive.height() == 1440);
}

TEST_CASE("Test small extent target maxed (width)")
{
	VideoExtractor rive("./static/50x51.riv", // source
	                    "",                   // artboard
	                    "",                   // animation
	                    "",                   // watermark
	                    "fake.mp4",           // destination
	                    50,                   // width
	                    100,                  // height
	                    720,                  // smallExtentTarget
	                    1080,                 // maxWidth
	                    1080                  // maxHeight
	);
	REQUIRE(rive.width() == 540);
	REQUIRE(rive.height() == 1080);
}

TEST_CASE("Test small extent target (height)")
{
	VideoExtractor rive("./static/50x51.riv", // source
	                    "",                   // artboard
	                    "",                   // animation
	                    "",                   // watermark
	                    "fake.mp4",           // destination
	                    100,
	                    50,
	                    720);
	REQUIRE(rive.height() == 720);
	REQUIRE(rive.width() == 1440);
}

TEST_CASE("Test small extent target maxed (height)")
{
	VideoExtractor rive("./static/50x51.riv", // source
	                    "",                   // artboard
	                    "",                   // animation
	                    "",                   // watermark
	                    "fake.mp4",           // destination
	                    100,                  // width
	                    50,                   // height
	                    720,                  // smallExtentTarget
	                    1080,                 // maxWidth
	                    1080                  // maxHeight
	);
	REQUIRE(rive.height() == 540);
	REQUIRE(rive.width() == 1080);
}

TEST_CASE("Test 1s_oneShot min 10s")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "1s_oneShot",              // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    0,                         // duration
	                    10                         // minDuration
	);
	REQUIRE(rive.totalFrames() == 600);
}

TEST_CASE("Test 1s oneShot animation @60fps with custom 2s duration")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "1s_oneShot",              // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    120 // duration (60 fps * 2s = 120ff)
	);

	REQUIRE(rive.totalFrames() == 60 * 2);
}

TEST_CASE("Test 1s oneShot animation @120fps with custom 2s duration")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "1s_oneShot",              // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    240, // duration (60 fps * 2s = 120ff)
	                    0,   // minDuration
	                    0,   // maxDuration
	                    120  // fps
	);

	REQUIRE(rive.totalFrames() == 120 * 2);
	REQUIRE(rive.m_Fps == 120);
}

TEST_CASE("Test 1s oneShot animation @60fps with custom 2s duration min 3s")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "1s_oneShot",              // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    120, // duration (60 fps * 2s = 120ff)
	                    3    // minDuration
	);

	REQUIRE(rive.totalFrames() == 60 * 3);
}

TEST_CASE("Test 1s oneShot animation @60fps with custom 2s duration max 1s")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "1s_oneShot",              // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    120, // duration (60 fps * 15s = 120ff)
	                    0,   // minDuration
	                    1    // maxDuration
	);

	REQUIRE(rive.totalFrames() == 60);
}

TEST_CASE(
    "Test 1s oneShot animation @60fps with custom 2s duration min 3s max 4s")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "1s_oneShot",              // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    120, // duration (60 fps * 2s = 120ff)
	                    3,   // minDuration
	                    4    // maxDuration
	);

	REQUIRE(rive.totalFrames() == 60 * 3);
}

TEST_CASE("Test 2s_loop min 5s")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "2s_loop",                 // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    0,                         // duration
	                    5                          // minDuration
	);
	REQUIRE(rive.totalFrames() == 360);
}

TEST_CASE("Test 2s_loop min 5s max 5s")
{
	// give it something dumb, it'll do something dumb.
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "2s_loop",                 // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    0,                         // duration
	                    5,                         // minDuration
	                    5                          // maxDuration
	);
	REQUIRE(rive.totalFrames() == 300);
}

TEST_CASE("Test 2s_pingpong min 5s")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "2s_pingpong",             // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    0,                         // duration
	                    5                          // minDuration
	);
	REQUIRE(rive.totalFrames() == 480);
}

TEST_CASE("Test 2s_pingpong")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "2s_pingpong",             // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    0,                         // duration
	                    0                          // minDuration
	);
	REQUIRE(rive.totalFrames() == 2 * 60);
}

TEST_CASE("Test 100s_oneShot animation min duration 10s")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "100s_oneShot",            // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    0,                         // duration
	                    10                         // minDuration
	);
	REQUIRE(rive.totalFrames() == 6000);
}

TEST_CASE("Test 100s_loop animation max duration 10s")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "100s_loop",               // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    0,                         // duration
	                    0,                         // minDuration
	                    10                         // maxDuration
	);
	REQUIRE(rive.totalFrames() == 600);
}

TEST_CASE("Test 100s_pingpong animation min duration 10s")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "100s_pingpong",           // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    0,                         // duration
	                    10                         // minDuration
	);
	REQUIRE(rive.totalFrames() == 6000);
}

TEST_CASE("Test 1s_oneShot animation custom fps 120")
{
	VideoExtractor rive("./static/animations.riv", // source
	                    "",                        // artboard
	                    "100s_oneShot",            // animation
	                    "",                        // watermark
	                    "fake.mp4",                // destination
	                    0,                         // width
	                    0,                         // height
	                    0,                         // smallExtentTarget
	                    0,                         // maxWidth
	                    0,                         // maxHeight
	                    0,                         // duration
	                    0,                         // minDuration
	                    10,                        // maxDuration
	                    120                        // fps
	);
	REQUIRE(rive.totalFrames() == 1200);
}

TEST_CASE("Test frames: 3s_loop work_area start_16 duration_1s")
{
	VideoExtractor rive("./static/work_area.riv", // source
	                    "",                       // artboard
	                    "Animation 1",            // animation
	                    "",                       // watermark
	                    "fake.mp4",               // destination
	                    0,                        // width
	                    0,                        // height
	                    0,                        // smallExtentTarget
	                    0,                        // maxWidth
	                    0,                        // maxHeight
	                    0,                        // duration
	                    0,                        // minDuration
	                    0                         // maxDuration
	);
	REQUIRE(rive.totalFrames() == 60);
}

TEST_CASE("Test frames: 3s_loop work_area start_16 duration_1s min 5s")
{
	VideoExtractor rive("./static/work_area.riv", // source
	                    "",                       // artboard
	                    "Animation 1",            // animation
	                    "",                       // watermark
	                    "fake.mp4",               // destination
	                    0,                        // width
	                    0,                        // height
	                    0,                        // smallExtentTarget
	                    0,                        // maxWidth
	                    0,                        // maxHeight
	                    0,                        // duration
	                    5,                        // minDuration
	                    0                         // maxDuration
	);
	REQUIRE(rive.totalFrames() == 300);
}

TEST_CASE("Generate a video from a riv file", "[video]")
{
	const char* argsVector[] = {"rive_recorder",
	                            "-s",
	                            "./static/animations.riv",
	                            "-d",
	                            "./static/animations.out.mp4",
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

	auto destination = args.destination();
	auto extractor = (VideoExtractor*)makeExtractor(args);

	auto snapshotPath = args.snapshotPath();
	extractor->takeSnapshot(snapshotPath);
	extractor->extractFrames(args.numLoops());

	int removeErr = 1;
	std::ifstream videoFile(destination);
	REQUIRE(videoFile.good());
	removeErr = std::remove(destination.c_str());
	REQUIRE(removeErr == 0);

	std::ifstream snapshotFile(snapshotPath);
	REQUIRE(snapshotFile.good());
	removeErr = std::remove(snapshotPath.c_str());
	REQUIRE(removeErr == 0);

	// TODO: Run mediainfo to validate this?
	delete extractor;
}

TEST_CASE("Generate a video when posting to Community", "[community]")
{
	// This is the 'typical' payload when trying to generate a Community video.
	const char* argsVector[] = {"rive_recorder",
	                            "-s",
	                            "./static/animations.riv",
	                            "-d",
	                            "./static/animations.out.mp4",
	                            "--max-height",
	                            "1440",
	                            "--max-width",
	                            "1440",
	                            "--min-duration",
	                            "3",
	                            "--max-duration",
	                            "30",
	                            "--snapshot-path",
	                            "./static/snapshot.png",
	                            "-t",
	                            "animations",
	                            "-a",
	                            "1s_oneShot",
	                            "-w",
	                            "./static/watermark.png",
	                            "--width",
	                            "0",
	                            "--height",
	                            "0",
	                            "--fps",
	                            "0.0",
	                            "--bitrate",
	                            "0",
	                            "--num-loops",
	                            "1",
	                            "--format",
	                            "h264"};
	unsigned int argc = sizeof(argsVector) / sizeof(argsVector[0]);
	RecorderArguments args(argc, argsVector);

	auto destination = args.destination();
	auto extractor = (VideoExtractor*)makeExtractor(args);

	auto snapshotPath = args.snapshotPath();
	extractor->takeSnapshot(snapshotPath);
	extractor->extractFrames(args.numLoops());

	int removeErr = 1;
	std::ifstream videoFile(destination);
	REQUIRE(videoFile.good());
	removeErr = std::remove(destination.c_str());
	REQUIRE(removeErr == 0);

	std::ifstream snapshotFile(snapshotPath);
	REQUIRE(snapshotFile.good());
	removeErr = std::remove(snapshotPath.c_str());
	REQUIRE(removeErr == 0);

	delete extractor;
}
