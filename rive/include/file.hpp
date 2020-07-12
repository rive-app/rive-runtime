#ifndef _RIVE_FILE_HPP_
#define _RIVE_FILE_HPP_

#include "core/binary_reader.hpp"
#include "runtime_header.hpp"
#include "backboard.hpp"
#include "artboard.hpp"
#include <vector>

namespace rive
{
	enum class ImportResult
	{
		success,
		unsupportedVersion,
		malformed
	};

	class File
	{
    public:
		static const int majorVersion = 1;
		static const int minorVersion = 0;

    private:
        Backboard* m_Backboard;
        std::vector<Artboard*> m_Artboards;

	public:
		static ImportResult import(BinaryReader& reader, File** importedFile);

        Backboard* backboard() const;
        Artboard* artboard() const;
        Artboard* artboard(std::string name) const;

    private:
        ImportResult read(BinaryReader& reader);
	};
} // namespace rive
#endif