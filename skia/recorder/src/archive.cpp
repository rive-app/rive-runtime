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
		std::string err_output("Failed to open output file test.zip: ");
		err_output += zip_error_strerror(&zip_error);
		throw std::runtime_error(err_output);
	}
}

Archive::~Archive()
{
	if (zip_archive != nullptr)
	{
		//  The zip_close() function writes any changes made to archive to disk.
		// If archive contains no files, the file is completely removed (no
		// empty archive is written). If successful, archive is freed. Otherwise
		// archive is left unchanged and must still be freed.
		std::cout << "Outtahere!" << std::endl;
		int err = zip_close(zip_archive);
		if (err < 0)
		{
			std::cout << "Couldn't close zip, discarding." +
			                 std::string(zip_strerror(zip_archive))
			          << std::endl;
			zip_discard(zip_archive);
		}
	}
}

int Archive::add_buffer(const std::string& filename,
                        const std::vector<char>& bytes) const
{

	zip_source_t* source;
	// std::vector<char> bytes = read_file(filepath);
	std::cout << "Sourcing buffer!" << std::endl;
	if ((source = zip_source_buffer(zip_archive, &bytes[0], bytes.size(), 0)) ==
	    NULL)
	{
		// std::cout << "FAIL: zip_source_buffer()" << std::endl;
		// std::cout << zip_strerror(zip_archive) << std::endl;
		zip_source_free(source);
		std::string err_output("Archive::add()::zip_source_buffer() failed: ");
		err_output += zip_strerror(zip_archive);
		throw std::runtime_error(err_output);
	}

	std::cout << "Buffer sourced..!" << std::endl;
	std::cout << "Add file?" << std::endl;
	if (zip_file_add(zip_archive, filename.c_str(), source, 0) < 0)
	{
		// std::cout << "FAIL: zip_file_add()" << std::endl;
		// std::cout << zip_strerror(zip_archive) << std::endl;
		zip_source_free(source);
		std::string err_output("Archive::add()::zip_file_add(" + filename +
		                       ") failed: ");
		err_output += zip_strerror(zip_archive);
		throw std::runtime_error(err_output);
	}
	std::cout << "File added!?" << std::endl;
	return 0;
}

const std::vector<char> Archive::read_file(const std::string& filepath)
{
	std::cout << "Reading file: " << filepath << std::endl;
	std::ifstream input(filepath, std::ios::binary);
	input.seekg(0, input.end);
	int size = input.tellg();
	input.seekg(0, input.beg);

	std::vector<char> bytes(size);
	input.read(&bytes[0], size);
	input.close();

	std::cout << "Done reading " << bytes.size() << " bytes. Returning!"
	          << std::endl;

	return bytes;
}