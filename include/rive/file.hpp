#ifndef _RIVE_FILE_HPP_
#define _RIVE_FILE_HPP_

#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/runtime_header.hpp"
#include "rive/file_asset_resolver.hpp"
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
		static const int majorVersion = 7;
		/// Minor version number supported by the runtime.
		static const int minorVersion = 0;

	private:
		/// The file's backboard. All Rive files have a single backboard
		/// where the artboards live.
		Backboard* m_Backboard = nullptr;

		/// List of artboards in the file. Each artboard encapsulates a set of
		/// Rive components and animations.
		std::vector<Artboard*> m_Artboards;

		/// The helper used to resolve assets when they're not provided in-band
		/// with the file.
		FileAssetResolver* m_AssetResolver;

		File(FileAssetResolver* assetResolver);

	public:
		~File();

		///
		/// Imports a Rive file from a binary buffer.
		/// @param reader a pointer to a binary reader attached to the file.
		/// @param importedFile a handle to a file that will contain the
		/// imported data.
		/// @param assetResolver is an optional helper to resolve assets which
		/// cannot be found in-band.
		/// @returns whether the import was successful or an error occurred.
		static ImportResult import(BinaryReader& reader,
		                           File** importedFile,
		                           FileAssetResolver* assetResolver = nullptr);

		/// @returns the file's backboard. All files have exactly one backboard.
		Backboard* backboard() const;

		/// @returns the default artboard. This is typically the first artboard
		/// found in the file's artboard list.
		Artboard* artboard() const;

		/// @returns the named artboard. If no artboard is found with that name,
		/// the null pointer is returned.
		Artboard* artboard(std::string name) const;

		/// @returns the artboard at the specified index, or the nullptr if the
		/// index is out of range.
		Artboard* artboard(size_t index) const;

		/// @returns the number of artboards in the file.
		size_t artboardCount() const { return m_Artboards.size(); }

	private:
		ImportResult read(BinaryReader& reader, const RuntimeHeader& header);
	};
} // namespace rive
#endif