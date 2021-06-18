#include "archive.hpp"

Archive::Archive(const std::string& archivePath) :
    m_ArchivePath(archivePath), m_ZipArchive(nullptr)
{
	this->openArchive(ZIP_CREATE);
}

// If this Archive hasn't been [finalize()]'d, it'll discard its contents.
Archive::~Archive()
{
	if (m_ZipArchive != nullptr)
	{
		zip_discard(m_ZipArchive);
		m_ZipArchive = nullptr;
	}
}

void Archive::openArchive(int flag = ZIP_CREATE)
{
	if (m_ZipArchive != nullptr)
	{
		return;
	}
	int err = 0;
	// Create the archive
	m_ZipArchive = zip_open(m_ArchivePath.c_str(), flag, &err);
	if (m_ZipArchive == nullptr)
	{
		zip_error_t zip_error;
		zip_error_init_with_code(&zip_error, err);
		std::string err_output("[Archive::Archive()] zip_open failed:\n\t");
		err_output += zip_error_strerror(&zip_error);
		throw std::runtime_error(err_output);
	}
}

// Closes the stream and saves the contents to disk if the archive is not empty.
void Archive::closeArchive()
{
	if (zip_close(m_ZipArchive) < 0)
	{
		throw std::runtime_error("[Archive::closeArchive()] close failed:\n\t" +
		                         std::string(zip_strerror(m_ZipArchive)));
	}
	else
	{
		m_ZipArchive = nullptr;
	}
}

// Adds a buffer [bytes] in [m_ZipArchive] with name [filename]
int Archive::addBuffer(const std::string& filename,
                       const std::vector<char>& bytes)
{
	return addBuffer(filename, &bytes[0], bytes.size());
}

int Archive::addBuffer(const std::string& filename,
                       const void* bytes,
                       uint64_t size)
{
	this->openArchive();
	zip_source_t* source;
	if ((source = zip_source_buffer(m_ZipArchive, bytes, size, 0)) == NULL)
	{
		zip_source_free(source);
		std::string err_output(
		    "Archive::add()::zip_source_buffer() failed:\n\t");
		err_output += zip_strerror(m_ZipArchive);
		throw std::runtime_error(err_output);
	}

	if (zip_file_add(m_ZipArchive, filename.c_str(), source, 0) < 0)
	{
		zip_source_free(source);
		std::string err_output("Archive::add()::zip_file_add(" + filename +
		                       ") failed:\n\t");
		err_output += zip_strerror(m_ZipArchive);
		throw std::runtime_error(err_output);
	}
	this->closeArchive();
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
	if (m_ZipArchive == nullptr)
	{
		throw std::runtime_error("[Archive::is_empty()] Missing archive! Call "
		                         "Archive::open() first.");
	}
	int entries = zip_get_num_entries(m_ZipArchive, 0);
	if (entries < 0)
	{
		throw std::runtime_error("[Archive::is_empty()] archive is NULL?");
	}

	return entries == 0;
}