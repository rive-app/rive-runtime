#ifdef TESTING
#else

#include "font.h"
#include "font_arguments.hpp"

void readFile(const char* path, uint8_t*& bytes, long& length)
{
	FILE* fp = fopen(path, "r");

	if (fp == nullptr)
	{
		fclose(fp);
		std::ostringstream errorStream;
		errorStream << "Failed to open file " << path;
		throw std::invalid_argument(errorStream.str());
	}
	fseek(fp, 0, SEEK_END);
	length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	bytes = new uint8_t[length];

	if (fread(bytes, 1, length, fp) != length)
	{
		fclose(fp);
		delete[] bytes;
		std::ostringstream errorStream;
		errorStream << "Failed to read file into bytes array " << path;
		throw std::invalid_argument(errorStream.str());
	}

	fclose(fp);
}

int main(int argc, const char* argv[])
{
	try
	{
		FontArguments args(argc, argv);
		RiveFont* font = new RiveFont();

		uint8_t* bytes;
		long length = 0;
		readFile(args.source().c_str(), bytes, length);
		// font->decode(bytes, length);
		// font->encode();

		FILE* pFile;
		pFile = fopen(args.destination().c_str(), "wb");
		fwrite(bytes, sizeof(char), length, pFile);
		fclose(pFile);

		delete[] bytes;
	}
	catch (const args::Completion& e)
	{
		return 0;
	}
	catch (const args::Help&)
	{
		return 0;
	}
	catch (const args::ParseError& e)
	{
		return 1;
	}
	catch (args::ValidationError e)
	{
		return 1;
	}

	return 0;
}

#endif
