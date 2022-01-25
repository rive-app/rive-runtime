#ifndef RECORDER_ARGUMENTS_HPP
#define RECORDER_ARGUMENTS_HPP

#include <iostream>
#include <string>
#include <unordered_map>

#include "args.hxx"

class FontArguments
{

public:
	FontArguments(int argc, const char** argv)
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

		m_Destination = args::get(destination);
		m_Source = args::get(source);
	}

	~FontArguments()
	{
		if (m_Parser != nullptr)
		{
			delete m_Parser;
		}
	}

	const std::string& destination() const { return m_Destination; }
	const std::string& source() const { return m_Source; }

private:
	args::ArgumentParser* m_Parser;
	std::string m_Destination;
	std::string m_Source;
};
#endif