#ifndef GOLDENS_ARGUMENTS_HPP
#define GOLDENS_ARGUMENTS_HPP

#include <iostream>
#include <memory>
#include <string>

#include "args.hxx"

class GoldensArguments
{

public:
    void parse(int argc, const char* const* argv)
    {
        m_parser = std::make_unique<args::ArgumentParser>(
            "Reads a .riv file, and optional artboard and state machine names, "
            "and generates the corresponding png grid image, "
            "storing it in (output).");

        args::HelpFlag help(*m_parser, "help", "Display this help menu", {'h', "help"});
        args::Group required(*m_parser, "required arguments:", args::Group::Validators::All);
        args::Group optional(*m_parser, "optional arguments:", args::Group::Validators::DontCare);

        args::ValueFlag<std::string> src(optional,
                                         "src",
                                         "source src filename (ignored if --test_harness)",
                                         {'s', "src"});
        args::ValueFlag<std::string> testHarness(optional,
                                                 "test_harness",
                                                 "TCP server address of python test harness",
                                                 {"test_harness"});
        // no default for windows because /dev/null isnt a thing
        args::ValueFlag<std::string> output(optional,
                                            "output",
                                            "output png directory (ignored if --test_harness)",
                                            {'o', "output"});
        args::ValueFlag<std::string> artboard(optional,
                                              "artboard",
                                              "artboard to draw from (only when src != '-')",
                                              {'t', "artboard"});
        args::ValueFlag<std::string> stateMachine(
            optional,
            "name",
            "state machine to be played (only when src != '-')",
            {'S', "stateMachine"});
        args::ValueFlag<std::string> backend(optional,
                                             "backend",
                                             "backend type: [gl, metal, angle_gl, angle_d3d, "
                                             "angle_vk, angle_mtl, coregraphics, skia_raster, rhi]",
                                             {'b', "backend"});
        args::Flag headless(optional,
                            "headless",
                            "perform rendering in an offscreen context",
                            {'d', "headless"});
        args::Flag fastPNG(optional,
                           "fastPNG",
                           "favor speed over space when compressing pngs",
                           {'f', "fast-png"});
        args::Flag interactive(optional,
                               "interactive",
                               "leave the window open for user interaction",
                               {'i', "interactive"});
        args::Flag verbose(optional, "verbose", "verbose output", {'v', "verbose"});

        args::ValueFlag<int> rows(optional, "int", "number of rows in the grid", {'r', "rows"});
        args::ValueFlag<int> cols(optional, "int", "number of columns in the grid", {'c', "cols"});
        args::ValueFlag<int> pngThreads(optional,
                                        "int",
                                        "number of PNG encode threads",
                                        {'p', "png_threads"},
                                        2);

        args::CompletionFlag completion(*m_parser, {"complete"});
        try
        {
            m_parser->ParseCLI(argc, argv);
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
            std::cout << *m_parser;
            throw;
        }
        catch (const args::ParseError& e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << *m_parser;
            throw;
        }
        catch (args::ValidationError e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << *m_parser;
            throw;
        }

        m_testHarness = args::get(testHarness);
        m_src = args::get(src);
        m_output = args::get(output);
        m_artboard = args::get(artboard);
        m_stateMachine = args::get(stateMachine);
        m_backend = args::get(backend);
        m_headless = args::get(headless);
        m_fastPNG = args::get(fastPNG);
        m_interactive = args::get(interactive);
        m_verbose = args::get(verbose);
        m_rows = std::max(args::get(rows), 1);
        m_cols = std::max(args::get(cols), 1);
        m_pngThreads = std::max(args::get(pngThreads), 1);
    }

    const std::string& testHarness() const { return m_testHarness; }
    const std::string& src() const { return m_src; }
    const std::string& output() const { return m_output; }
    const std::string& artboard() const { return m_artboard; }
    const std::string& stateMachine() const { return m_stateMachine; }
    const std::string& backend() const { return m_backend; }
    bool headless() const { return m_headless; }
    bool fastPNG() const { return m_fastPNG; }
    bool interactive() const { return m_interactive; }
    bool verbose() const { return m_verbose; }
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    int pngThreads() const { return m_pngThreads; }

private:
    std::unique_ptr<args::ArgumentParser> m_parser;

    std::string m_testHarness;
    std::string m_src;
    std::string m_output;
    std::string m_artboard;
    std::string m_stateMachine;
    std::string m_backend;
    std::string m_pngServer;
    bool m_headless;
    bool m_fastPNG;
    bool m_interactive;
    bool m_verbose;
    int m_rows;
    int m_cols;
    int m_pngThreads;
};
#endif
