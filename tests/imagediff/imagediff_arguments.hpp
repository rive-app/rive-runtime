/**
 *  Copyright 2022 Rive Inc.
 */

#ifndef _IMAGE_DIFF_ARGUMENTS_HPP_
#define _IMAGE_DIFF_ARGUMENTS_HPP_

#include <iostream>
#include <memory>
#include <string>

#include "args.hxx"

class ImageDiffArguments
{

public:
    ImageDiffArguments(int argc, const char** argv)
    {
        m_Parser = std::make_unique<args::ArgumentParser>(
            "Reads two png files and compares them, "
            "writing the status to a status file, "
            "and writing any diff images to a diff directory");

        args::HelpFlag help(*m_Parser, "help", "Display this help menu", {'h', "help"});
        args::Group required(*m_Parser, "required arguments:", args::Group::Validators::All);
        args::Group optional(*m_Parser, "optional arguments:", args::Group::Validators::DontCare);

        args::ValueFlag<std::string> name(required, "name", "image/test name", {'n', "name"});
        args::ValueFlag<std::string> golden(required,
                                            "path",
                                            "original file path",
                                            {'g', "golden"});
        args::ValueFlag<std::string> candidate(required,
                                               "path",
                                               "candidate file path",
                                               {'c', "candidate"});

        args::ValueFlag<std::string> status(optional,
                                            "path",
                                            "status file to append",
                                            {'s', "status"});
        args::ValueFlag<std::string> output(optional, "path", "diff directory", {'o', "output"});

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
            std::cout << *m_Parser;
            throw;
        }
        catch (const args::ParseError& e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << *m_Parser;
            throw;
        }
        catch (args::ValidationError e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << *m_Parser;
            throw;
        }

        m_Name = args::get(name);
        m_Golden = args::get(golden);
        m_Candidate = args::get(candidate);
        m_Status = args::get(status);
        m_Output = args::get(output);
    }

    const std::string& name() const { return m_Name; }
    const std::string& golden() const { return m_Golden; }
    const std::string& candidate() const { return m_Candidate; }
    const std::string& status() const { return m_Status; }
    const std::string& output() const { return m_Output; }

private:
    std::unique_ptr<args::ArgumentParser> m_Parser;

    std::string m_Name;
    std::string m_Golden;
    std::string m_Candidate;
    std::string m_Status;
    std::string m_Output;
};
#endif
