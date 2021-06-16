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

	static const std::vector<char> readFile(const std::string& filepath);
	int addBuffer(const std::string& filename,
	              const std::vector<char>& bytes) const;
	int addBuffer(const std::string& filename,
	              const void* bytes,
	              uint64_t size) const;
	bool isEmpty() const;
	void finalize();

private:
	zip* m_zipArchive;
};