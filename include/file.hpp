#ifndef _RIVE_FILE_HPP_
#define _RIVE_FILE_HPP_

#include "artboard.hpp"
#include "backboard.hpp"
#include "core/binary_reader.hpp"
#include "runtime_header.hpp"
#include <vector>

///
/// Default namespace for Rive Cpp runtime code.
///
namespace rive
{
	///
	/// Tracks the success/failure result when importing a Rive file.
	///
	enum class ImportResult
	{
		/// Indicates that a file's been successfully imported.
		success,
		/// Indicates that the Rive file is not supported by this runtime.
		unsupportedVersion,
		/// Indicates that the there is a formatting problem in the file itself.
		malformed
	};

	///
	/// A Rive file.
	///
	class File
	{
	public:
		/// Major version number supported by the runtime.
		static const int majorVersion = 5;
		/// Minor version number supported by the runtime.
		static const int minorVersion = 1;

	private:
		/// The file's backboard. All Rive files have a single backboard
		/// where the artboards live.
		Backboard* m_Backboard = nullptr;

		/// List of artboards in the file. Each artboard encapsulates a set of
		/// Rive components and animations.
		std::vector<Artboard*> m_Artboards;

	public:
		~File();

		///
		/// Imports a Rive file from a binary buffer.
		/// @param reader a pointer to a binary reader attached to the file.
		/// @param importedFile a handle to a file that will contain the
		/// imported data.
		/// @returns whether the import was successful or an error occurred.
		static ImportResult import(BinaryReader& reader, File** importedFile);

		/// @returns the file's backboard. All files have exactly one backboard.
		Backboard* backboard() const;

		/// @returns the default artboard. This is typically the first artboard
		/// found in the file's artboard list.
		Artboard* artboard() const;

		/// @returns the named artboard. If no artboard is found with that name,
		/// the null pointer is returned
		Artboard* artboard(std::string name) const;

	private:
		ImportResult read(BinaryReader& reader);
	};
} // namespace rive
#endif