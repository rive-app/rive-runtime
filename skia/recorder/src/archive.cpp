#include "archive.hpp"

Archive::Archive(const std::string& archive_name)
{
	int err = 0;
	// Create the archive
	m_zipArchive = zip_open(archive_name.c_str(), ZIP_CREATE | ZIP_EXCL, &err);
	if (m_zipArchive == nullptr)
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
	if (m_zipArchive != nullptr)
	{
		zip_discard(m_zipArchive);
		m_zipArchive = nullptr;
	}
}

// Closes the stream and saves the contents to disk if the archive is not empty.
void Archive::finalize()
{
	if (zip_close(m_zipArchive) < 0)
	{
		throw std::runtime_error("[Archive::finalize()] close failed:\n\t" +
		                         std::string(zip_strerror(m_zipArchive)));
	}
	else
	{
		m_zipArchive = nullptr;
	}
}

// Adds a buffer [bytes] in [m_zipArchive] with name [filename]
int Archive::addBuffer(const std::string& filename,
                        const std::vector<char>& bytes) const
{
	if (m_zipArchive == nullptr)
	{
		throw std::runtime_error("[Archive::add_buffer()] already finalized!");
	}
	zip_source_t* source;
	if ((source = zip_source_buffer(m_zipArchive, &bytes[0], bytes.size(), 0)) ==
	    NULL)
	{
		zip_source_free(source);
		std::string err_output(
		    "Archive::add()::zip_source_buffer() failed:\n\t");
		err_output += zip_strerror(m_zipArchive);
		throw std::runtime_error(err_output);
	}

	if (zip_file_add(m_zipArchive, filename.c_str(), source, 0) < 0)
	{
		zip_source_free(source);
		std::string err_output("Archive::add()::zip_file_add(" + filename +
		                       ") failed:\n\t");
		err_output += zip_strerror(m_zipArchive);
		throw std::runtime_error(err_output);
	}
	return 0;
}

// Reads a file from [filepath] and returns its bytes.
const std::vector<char> Archive::readFile(const std::string& filepath)
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

bool Archive::isEmpty() const
{
	if (m_zipArchive == nullptr)
	{
		throw std::runtime_error("[Archive::is_empty()] already finalized!");
	}
	int entries = zip_get_num_entries(m_zipArchive, 0);
	if (entries < 0)
	{
		throw std::runtime_error("[Archive::is_empty()] archive is NULL?");
	}

	return entries == 0;
}