#ifndef RECORDER_ARGUMENTS_HPP
#define RECORDER_ARGUMENTS_HPP

#include "args.hxx"
#include "extractor_type.hpp"
#include <iostream>

class RecorderArguments
{

public:
	RecorderArguments(int argc, char** argv)
	{
		// PARSE INPUT
		m_Parser = new args::ArgumentParser(
		    "Record playback of a Rive file as a movie, gif, etc (eventually "
		    "should support image sequences saved in a zip/archive too).",
		    "Experimental....");
		args::HelpFlag help(
		    *m_Parser, "help", "Display this help menu", {'h', "help"});
		args::Group required(
		    *m_Parser, "required arguments:", args::Group::Validators::All);
		args::Group optional(*m_Parser,
		                     "optional arguments:",
		                     args::Group::Validators::DontCare);

		args::ValueFlag<std::string> source(
		    required, "path", "source filename", {'s', "source"});
		args::ValueFlag<std::string> destination(
		    required, "path", "destination filename", {'d', "destination"});
		args::ValueFlag<std::string> animation(
		    optional,
		    "name",
		    "animation to be played, determines the numbers of frames recorded",
		    {'a', "animation"});
		args::ValueFlag<std::string> artboard(
		    optional, "name", "artboard to draw from", {'t', "artboard"});
		args::ValueFlag<std::string> watermark(
		    optional, "path", "watermark filename", {'w', "watermark"});

		args::ValueFlag<int> width(
		    optional, "number", "container width", {'W', "width"}, 0);

		args::ValueFlag<int> height(
		    optional, "number", "container height", {'H', "height"}, 0);

		args::ValueFlag<int> smallExtentTarget(
		    optional,
		    "number",
		    "target size for smaller edge of container",
		    {"small-extent-target"},
		    0);

		args::ValueFlag<int> maxWidth(
		    optional, "number", "maximum container width", {"max-width"}, 0);

		args::ValueFlag<int> maxHeight(
		    optional, "number", "maximum container height", {"max-height"}, 0);

		args::ValueFlag<int> minDuration(optional,
		                                 "number",
		                                 "minimum duration in seconds",
		                                 {"min-duration"},
		                                 0);

		args::ValueFlag<int> maxDuration(optional,
		                                 "number",
		                                 "maximum duration in seconds",
		                                 {"max-duration"},
		                                 0);

		args::ValueFlag<int> numLoops(
		    optional,
		    "number",
		    "number of times this video should be looped for",
		    {"num-loops"},
		    1);

		// 0 will use VBR. By setting a bitrate value, users enforce CBR.
		args::ValueFlag<int> bitrate(
		    optional, "number", "bitrate in kbps", {"bitrate"}, 0);

		args::ValueFlag<std::string> snapshotPath(
		    optional, "path", "destination image filename", {"snapshot-path"});

		args::ValueFlag<float> fps(
		    optional, "number", "frame rate", {"f", "fps"}, 60.0);

		args::CompletionFlag completion(*m_Parser, {"complete"});
		try
		{
			m_Parser->ParseCLI(argc, argv);
		}
		catch (const std::invalid_argument e)
		{
			std::cout << e.what();
			throw;
		}
		catch (const args::Completion& e)
		{
			std::cout << e.what();
			throw;
		}
		catch (const args::Help&)
		{
			std::cout << m_Parser;
			throw;
		}
		catch (const args::ParseError& e)
		{
			std::cerr << e.what() << std::endl;
			std::cerr << m_Parser;
			throw;
		}
		catch (args::ValidationError e)
		{
			std::cerr << e.what() << std::endl;
			std::cerr << m_Parser;
			throw;
		}

		m_Source = args::get(source);
		m_Destination = args::get(destination);
		m_Animation = args::get(animation);
		m_Artboard = args::get(artboard);
		m_Watermark = args::get(watermark);
		m_SnapshotPath = args::get(snapshotPath);
		m_Width = args::get(width);
		m_Height = args::get(height);
		m_MaxWidth = args::get(maxWidth);
		m_MaxHeight = args::get(maxHeight);
		m_SmallExtentTarget = args::get(smallExtentTarget);
		m_MinDuration = args::get(minDuration);
		m_MaxDuration = args::get(maxDuration);
		m_NumLoops = args::get(numLoops);
		m_Bitrate = args::get(bitrate);
		m_Fps = args::get(fps);
	}

	~RecorderArguments()
	{
		if (m_Parser != nullptr)
			delete m_Parser;
	}

	// TODO: support reading this as a param.
	ExtractorType renderType() const { return ExtractorType::h264; }
	float fps() const { return m_Fps; }
	int bitrate() const { return m_Bitrate; }
	int height() const { return m_Height; }
	int maxDuration() const { return m_MaxDuration; }
	int maxHeight() const { return m_MaxHeight; }
	int maxWidth() const { return m_MaxWidth; }
	int minDuration() const { return m_MinDuration; }
	int numLoops() const { return m_NumLoops; }
	int smallExtentTarget() const { return m_SmallExtentTarget; }
	int width() const { return m_Width; }
	std::string animation() const { return m_Animation; }
	std::string artboard() const { return m_Artboard; }
	std::string destination() const { return m_Destination; }
	std::string snapshotPath() const { return m_SnapshotPath; }
	std::string source() const { return m_Source; }
	std::string watermark() const { return m_Watermark; }

private:
	args::ArgumentParser* m_Parser;
	float m_Fps;
	int m_Bitrate;
	int m_Height;
	int m_MaxDuration;
	int m_MaxHeight;
	int m_MaxWidth;
	int m_MinDuration;
	int m_NumLoops;
	int m_SmallExtentTarget;
	int m_Width;
	std::string m_Animation;
	std::string m_Artboard;
	std::string m_Destination;
	std::string m_SnapshotPath;
	std::string m_Source;
	std::string m_Watermark;
};
#endif