#ifndef GOLDENS_ARGUMENTS_HPP
#define GOLDENS_ARGUMENTS_HPP

#include <iostream>
#include <memory>
#include <string>

#include "args.hxx"

class GoldensArguments {

public:
    GoldensArguments(int argc, const char** argv) {
        m_Parser = std::make_unique<args::ArgumentParser>(
            "Reads a (source) .riv file, and optional artboard and animations names, "
            "and generates the corresponding png grid image, "
            "storing it in (destination).");

        args::HelpFlag help(*m_Parser, "help", "Display this help menu", {'h', "help"});
        args::Group required(*m_Parser, "required arguments:", args::Group::Validators::All);
        args::Group optional(*m_Parser, "optional arguments:", args::Group::Validators::DontCare);

        args::ValueFlag<std::string> source(
            required, "path", "source riv filename", {'s', "source"});
        args::ValueFlag<std::string> destination(
            required, "path", "destination png filename", {'d', "destination"});

        args::ValueFlag<std::string> artboard(
            optional, "name", "artboard to draw from", {'t', "artboard"});
        args::ValueFlag<std::string> animation(
            optional, "name", "animation to be played, determines", {'a', "animation"});

        args::CompletionFlag completion(*m_Parser, {"complete"});
        try {
            m_Parser->ParseCLI(argc, argv);
        } catch (const std::invalid_argument e) {
            std::cout << e.what();
            throw;
        } catch (const args::Completion& e) {
            std::cout << e.what();
            throw;
        } catch (const args::Help&) {
            std::cout << *m_Parser;
            throw;
        } catch (const args::ParseError& e) {
            std::cerr << e.what() << std::endl;
            std::cerr << *m_Parser;
            throw;
        } catch (args::ValidationError e) {
            std::cerr << e.what() << std::endl;
            std::cerr << *m_Parser;
            throw;
        }

        m_Destination = args::get(destination);
        m_Source = args::get(source);
    }

    const std::string& source() const { return m_Source; }
    const std::string& destination() const { return m_Destination; }
    const std::string& artboard() const { return m_Artboard; }
    const std::string& animation() const { return m_Animation; }

private:
    std::unique_ptr<args::ArgumentParser> m_Parser;

    std::string m_Source;
    std::string m_Destination;
    std::string m_Artboard;
    std::string m_Animation;
};
#endif
