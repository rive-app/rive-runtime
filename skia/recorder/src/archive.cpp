#include "archive.hpp"

Archive::Archive(const std::string& archive_name)
{
	int err = 0;
	// Create the archive
	zip_archive = zip_open(archive_name.c_str(), ZIP_CREATE | ZIP_EXCL, &err);
	if (zip_archive == nullptr)
	{
		zip_error_t zip_error;
		zip_error_init_with_code(&zip_error, err);
		std::string err_output("[Archive::Archive()] zip_open failed:\n\t");
		err_output += zip_error_strerror(&zip_error);
		throw std::runtime_error(err_output);
	}
}

// If this Archive hasn't been [finalize()]'d, it'll discard its contents.
Archive::~Archive()
{
	if (zip_archive != nullptr)
	{
		zip_discard(zip_archive);
		zip_archive = nullptr;
	}
}

// Closes the stream and saves the contents to disk if the archive is not empty.
void Archive::finalize()
{
	if (zip_close(zip_archive) < 0)
	{
		throw std::runtime_error("[Archive::finalize()] close failed:\n\t" +
		                         std::string(zip_strerror(zip_archive)));
	}
	else
	{
		zip_archive = nullptr;
	}
}

// Adds a buffer [bytes] in [zip_archive] with name [filename]
int Archive::add_buffer(const std::string& filename,
                        const std::vector<char>& bytes) const
{
	if (zip_archive == nullptr)
	{
		throw std::runtime_error("[Archive::add_buffer()] already finalized!");
	}
	zip_source_t* source;
	if ((source = zip_source_buffer(zip_archive, &bytes[0], bytes.size(), 0)) ==
	    NULL)
	{
		zip_source_free(source);
		std::string err_output(
		    "Archive::add()::zip_source_buffer() failed:\n\t");
		err_output += zip_strerror(zip_archive);
		throw std::runtime_error(err_output);
	}

	if (zip_file_add(zip_archive, filename.c_str(), source, 0) < 0)
	{
		zip_source_free(source);
		std::string err_output("Archive::add()::zip_file_add(" + filename +
		                       ") failed:\n\t");
		err_output += zip_strerror(zip_archive);
		throw std::runtime_error(err_output);
	}
	return 0;
}

// Reads a file from [filepath] and returns its bytes.
const std::vector<char> Archive::read_file(const std::string& filepath)
{
	std::ifstream input(filepath, std::ios::binary);
	input.seekg(0, input.end);
	int size = input.tellg();
	input.seekg(0, input.beg);

	std::vector<char> bytes(size);
	input.read(&bytes[0], size);
	input.close();

	return bytes;
}