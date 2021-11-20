#ifndef _RIVE_FILE_ASSET_CONTENTS_BASE_HPP_
#define _RIVE_FILE_ASSET_CONTENTS_BASE_HPP_
#include <vector>
#include "rive/core.hpp"
#include "rive/core/field_types/core_bytes_type.hpp"
namespace rive
{
	class FileAssetContentsBase : public Core
	{
	protected:
		typedef Core Super;

	public:
		static const uint16_t typeKey = 106;

		/// Helper to quickly determine if a core object extends another without
		/// RTTI at runtime.
		bool isTypeOf(uint16_t typeKey) const override
		{
			switch (typeKey)
			{
				case FileAssetContentsBase::typeKey:
					return true;
				default:
					return false;
			}
		}

		uint16_t coreType() const override { return typeKey; }

		static const uint16_t bytesPropertyKey = 212;

	private:
		std::vector<uint8_t> m_Bytes;
	public:
		inline const std::vector<uint8_t>& bytes() const { return m_Bytes; }
		void bytes(std::vector<uint8_t> value)
		{
			if (m_Bytes == value)
			{
				return;
			}
			m_Bytes = value;
			bytesChanged();
		}

		Core* clone() const override;
		void copy(const FileAssetContentsBase& object)
		{
			m_Bytes = object.m_Bytes;
		}

		bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
		{
			switch (propertyKey)
			{
				case bytesPropertyKey:
					m_Bytes = CoreBytesType::deserialize(reader);
					return true;
			}
			return false;
		}

	protected:
		virtual void bytesChanged() {}
	};
} // namespace rive

#endif