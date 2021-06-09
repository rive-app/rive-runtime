#include <iostream>
#include <fstream>
#include <zlib.h>
#include <string>
#include <zip.h>
#include <vector>

class Archive
{
public:
	Archive(const std::string& archive_name);
	~Archive();

	static const std::vector<char> read_file(const std::string& filepath);
	int add_buffer(const std::string& filename,
	               const std::vector<char>& bytes) const;
  void finalize();

private:
	zip* zip_archive;
};