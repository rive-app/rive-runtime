#ifndef RECORDER_ARGUMENTS_HPP
#define RECORDER_ARGUMENTS_HPP

#include <iostream>
#include <string>
#include <unordered_map>

#include "args.hxx"
#include "render_format.hpp"

class RecorderArguments
{

public:
	RecorderArguments(int argc, const char** argv)
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

		args::ValueFlag<int> duration(
		    optional, "number", "Duration in frames", {"duration"}, 0);

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

		args::MapFlag<std::string, RenderFormat> formatMapping(
		    optional,
		    "Output Format",
		    "Maps the format string (e.g. 264) to its enum",
		    {"format"},
		    m_renderFormatMap,
		    RenderFormat::h264);

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

		m_Bitrate = args::get(bitrate);
		m_Duration = args::get(duration);
		m_Fps = args::get(fps);
		m_Height = args::get(height);
		m_MaxDuration = args::get(maxDuration);
		m_MaxHeight = args::get(maxHeight);
		m_MaxWidth = args::get(maxWidth);
		m_MinDuration = args::get(minDuration);
		m_NumLoops = args::get(numLoops);
		m_RenderFormat = args::get(formatMapping);
		m_SmallExtentTarget = args::get(smallExtentTarget);
		m_Width = args::get(width);

		m_Animation = args::get(animation);
		m_Artboard = args::get(artboard);
		m_Destination = args::get(destination);
		m_SnapshotPath = args::get(snapshotPath);
		m_Source = args::get(source);
		m_Watermark = args::get(watermark);
	}

	~RecorderArguments()
	{
		if (m_Parser != nullptr)
		{
			delete m_Parser;
		}
	}

	RenderFormat renderFormat() const { return m_RenderFormat; }
	float fps() const { return m_Fps; }
	int bitrate() const { return m_Bitrate; }
	int duration() const { return m_Duration; }
	int height() const { return m_Height; }
	int maxDuration() const { return m_MaxDuration; }
	int maxHeight() const { return m_MaxHeight; }
	int maxWidth() const { return m_MaxWidth; }
	int minDuration() const { return m_MinDuration; }
	int numLoops() const { return m_NumLoops; }
	int smallExtentTarget() const { return m_SmallExtentTarget; }
	int width() const { return m_Width; }
	const std::string& animation() const { return m_Animation; }
	const std::string& artboard() const { return m_Artboard; }
	const std::string& destination() const { return m_Destination; }
	const std::string& snapshotPath() const { return m_SnapshotPath; }
	const std::string& source() const { return m_Source; }
	const std::string& watermark() const { return m_Watermark; }

private:
	args::ArgumentParser* m_Parser;
	float m_Fps;
	int m_Bitrate;
	int m_Duration;
	int m_Height;
	int m_MaxDuration;
	int m_MaxHeight;
	int m_MaxWidth;
	int m_MinDuration;
	int m_NumLoops;
	int m_SmallExtentTarget;
	int m_Width;
	RenderFormat m_RenderFormat;
	std::string m_Animation;
	std::string m_Artboard;
	std::string m_Destination;
	std::string m_SnapshotPath;
	std::string m_Source;
	std::string m_Watermark;

	std::unordered_map<std::string, RenderFormat> m_renderFormatMap{
	    {"h264", RenderFormat::h264},
	    {"png_sequence", RenderFormat::pngSequence}};
};
#endif