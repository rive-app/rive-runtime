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
	int addBuffer(const std::string& filename, const std::vector<char>& bytes);
	int
	addBuffer(const std::string& filename, const void* bytes, uint64_t size);
	bool isEmpty() const;
	void openArchive(int flag);
	void closeArchive();

private:
	std::string m_ArchivePath;
	zip* m_ZipArchive;
};