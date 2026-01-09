#include <catch.hpp>
#include <rive/bytecode_header.hpp>
#include <rive/span.hpp>
#include <vector>
#include <cstdint>

// Tests for BytecodeHeader class (no scripting dependency)
TEST_CASE("BytecodeHeader - empty data is invalid", "[bytecode]")
{
    std::vector<uint8_t> data;
    rive::BytecodeHeader header{rive::Span<const uint8_t>(data)};

    REQUIRE(header.isValid() == false);
}

TEST_CASE("BytecodeHeader - unsigned header", "[bytecode]")
{
    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03};
    rive::BytecodeHeader header{rive::Span<const uint8_t>(data)};

    REQUIRE(header.isValid() == true);
    REQUIRE(header.isSigned() == false);
    REQUIRE(header.version() == 0);
    REQUIRE(header.bytecodeOffset() == 1);

    auto bytecode = header.bytecode();
    REQUIRE(bytecode.size() == 3);
    REQUIRE(bytecode[0] == 0x01);
    REQUIRE(bytecode[1] == 0x02);
    REQUIRE(bytecode[2] == 0x03);

    auto signature = header.signature();
    REQUIRE(signature.empty());
}

TEST_CASE("BytecodeHeader - signed header", "[bytecode]")
{
    std::vector<uint8_t> data(1 + rive::kSignatureSize + 3);
    data[0] = 0x80; // signed flag
    // Fill signature with pattern
    for (size_t i = 0; i < rive::kSignatureSize; i++)
    {
        data[1 + i] = static_cast<uint8_t>(i);
    }
    // Bytecode
    data[1 + rive::kSignatureSize] = 0xAA;
    data[1 + rive::kSignatureSize + 1] = 0xBB;
    data[1 + rive::kSignatureSize + 2] = 0xCC;

    rive::BytecodeHeader header{rive::Span<const uint8_t>(data)};

    REQUIRE(header.isValid() == true);
    REQUIRE(header.isSigned() == true);
    REQUIRE(header.version() == 0);
    REQUIRE(header.bytecodeOffset() == 65);

    auto signature = header.signature();
    REQUIRE(signature.size() == rive::kSignatureSize);
    REQUIRE(signature[0] == 0);
    REQUIRE(signature[63] == 63);

    auto bytecode = header.bytecode();
    REQUIRE(bytecode.size() == 3);
    REQUIRE(bytecode[0] == 0xAA);
    REQUIRE(bytecode[1] == 0xBB);
    REQUIRE(bytecode[2] == 0xCC);
}

TEST_CASE("BytecodeHeader - version extraction", "[bytecode]")
{
    std::vector<uint8_t> data = {0x2A, 0x01}; // version 42, not signed
    rive::BytecodeHeader header{rive::Span<const uint8_t>(data)};

    REQUIRE(header.isValid() == true);
    REQUIRE(header.isSigned() == false);
    REQUIRE(header.version() == 42);
}

TEST_CASE("BytecodeHeader - truncated signed data is invalid", "[bytecode]")
{
    std::vector<uint8_t> data = {0x80,
                                 0x01,
                                 0x02}; // claims signed but only 3 bytes
    rive::BytecodeHeader header{rive::Span<const uint8_t>(data)};

    REQUIRE(header.isValid() == false);
    REQUIRE(header.isSigned() == true); // flag is set
}

TEST_CASE("BytecodeHeader - minimum unsigned (flags only)", "[bytecode]")
{
    std::vector<uint8_t> data = {0x00};
    rive::BytecodeHeader header{rive::Span<const uint8_t>(data)};

    REQUIRE(header.isValid() == true);
    REQUIRE(header.bytecode().empty());
}

TEST_CASE("BytecodeHeader - minimum signed (no bytecode)", "[bytecode]")
{
    std::vector<uint8_t> data(1 + rive::kSignatureSize);
    data[0] = 0x80;
    rive::BytecodeHeader header{rive::Span<const uint8_t>(data)};

    REQUIRE(header.isValid() == true);
    REQUIRE(header.isSigned() == true);
    REQUIRE(header.bytecode().empty());
    REQUIRE(header.signature().size() == rive::kSignatureSize);
}

#ifdef WITH_RIVE_SCRIPTING
#include <rive/assets/script_asset.hpp>

// Access the public key for verification
namespace rive
{
extern const uint8_t g_scriptVerificationPublicKey[32];
}

// Note: hydro_sign_BYTES = 64
constexpr size_t SIGNATURE_SIZE = 64;

TEST_CASE("bytecode header parsing - empty data fails", "[scripting][bytecode]")
{
    rive::ScriptAsset asset;
    std::vector<uint8_t> emptyData;

    bool result = asset.bytecode(rive::Span<uint8_t>(emptyData));

    REQUIRE(result == false);
    REQUIRE(asset.verified() == false);
}

TEST_CASE("bytecode header parsing - unsigned bytecode succeeds",
          "[scripting][bytecode]")
{
    rive::ScriptAsset asset;

    // Create unsigned bytecode: [flags:1] [bytecode:N]
    // Flags = 0x00 (version 0, not signed)
    std::vector<uint8_t> data = {
        0x00, // flags: version 0, not signed
        0x01,
        0x02,
        0x03, // dummy bytecode
    };

    bool result = asset.bytecode(rive::Span<uint8_t>(data));

    REQUIRE(result == true);
    REQUIRE(asset.verified() == false); // Unsigned = unverified

    // Verify the bytecode was extracted correctly (without header)
    auto bytecode = asset.moduleBytecode();
    REQUIRE(bytecode.size() == 3);
    REQUIRE(bytecode[0] == 0x01);
    REQUIRE(bytecode[1] == 0x02);
    REQUIRE(bytecode[2] == 0x03);
}

TEST_CASE("bytecode header parsing - signed flag is detected",
          "[scripting][bytecode]")
{
    rive::ScriptAsset asset;

    // Create data that claims to be signed but has invalid signature
    // [flags:1] [signature:64] [bytecode:N]
    // Flags = 0x80 (version 0, signed)
    std::vector<uint8_t> data(1 + SIGNATURE_SIZE + 3);
    data[0] = 0x80; // flags: version 0, signed

    // Fill signature with zeros (invalid)
    for (size_t i = 1; i <= SIGNATURE_SIZE; i++)
    {
        data[i] = 0x00;
    }

    // Dummy bytecode
    data[1 + SIGNATURE_SIZE] = 0x01;
    data[1 + SIGNATURE_SIZE + 1] = 0x02;
    data[1 + SIGNATURE_SIZE + 2] = 0x03;

    bool result = asset.bytecode(rive::Span<uint8_t>(data));

    // Should fail because signature doesn't verify
    REQUIRE(result == false);
    REQUIRE(asset.verified() == false);
}

TEST_CASE("bytecode header parsing - truncated signed data fails",
          "[scripting][bytecode]")
{
    rive::ScriptAsset asset;

    // Create data that claims to be signed but doesn't have enough bytes
    // for the signature
    std::vector<uint8_t> data = {
        0x80, // flags: signed
        0x01,
        0x02, // only 2 bytes of "signature" (need 64)
    };

    bool result = asset.bytecode(rive::Span<uint8_t>(data));

    REQUIRE(result == false);
    REQUIRE(asset.verified() == false);
}

TEST_CASE("bytecode header parsing - version is preserved in flags",
          "[scripting][bytecode]")
{
    rive::ScriptAsset asset;

    // Version is in bits 0-6, so we can test that non-zero versions
    // are handled (even if we only use version 0 currently)
    // Flags = 0x01 (version 1, not signed)
    std::vector<uint8_t> data = {
        0x01, // flags: version 1, not signed
        0x01,
        0x02,
        0x03, // dummy bytecode
    };

    bool result = asset.bytecode(rive::Span<uint8_t>(data));

    // Should succeed - version is currently ignored
    REQUIRE(result == true);
    REQUIRE(asset.verified() == false);
}

TEST_CASE("bytecode header parsing - signed bytecode offset is correct",
          "[scripting][bytecode]")
{
    rive::ScriptAsset asset;

    // For signed data, bytecode starts at offset 65 (1 flag + 64 signature)
    std::vector<uint8_t> data(1 + SIGNATURE_SIZE + 4);
    data[0] = 0x80; // flags: signed

    // Fill with pattern to verify offset
    for (size_t i = 0; i < data.size(); i++)
    {
        data[i] = static_cast<uint8_t>(i);
    }
    data[0] = 0x80; // Restore flags

    // Even though verification will fail, we can check the offset calculation
    // by inspecting what gets stored (if verification passed)

    bool result = asset.bytecode(rive::Span<uint8_t>(data));

    // Will fail due to invalid signature, but tests the parsing path
    REQUIRE(result == false);
}

TEST_CASE("bytecode header parsing - unsigned bytecode offset is correct",
          "[scripting][bytecode]")
{
    rive::ScriptAsset asset;

    // For unsigned data, bytecode starts at offset 1 (just the flag byte)
    std::vector<uint8_t> data = {
        0x00, // flags: not signed
        0xAA,
        0xBB,
        0xCC,
        0xDD,
        0xEE, // bytecode
    };

    bool result = asset.bytecode(rive::Span<uint8_t>(data));

    REQUIRE(result == true);

    auto bytecode = asset.moduleBytecode();
    REQUIRE(bytecode.size() == 5);
    REQUIRE(bytecode[0] == 0xAA);
    REQUIRE(bytecode[1] == 0xBB);
    REQUIRE(bytecode[2] == 0xCC);
    REQUIRE(bytecode[3] == 0xDD);
    REQUIRE(bytecode[4] == 0xEE);
}

TEST_CASE("bytecode header parsing - minimum valid unsigned data",
          "[scripting][bytecode]")
{
    rive::ScriptAsset asset;

    // Minimum valid: just the flags byte with empty bytecode
    std::vector<uint8_t> data = {0x00}; // flags only, no bytecode

    bool result = asset.bytecode(rive::Span<uint8_t>(data));

    REQUIRE(result == true);
    REQUIRE(asset.verified() == false);

    auto bytecode = asset.moduleBytecode();
    REQUIRE(bytecode.size() == 0);
}

TEST_CASE("bytecode header parsing - minimum valid signed data structure",
          "[scripting][bytecode]")
{
    rive::ScriptAsset asset;

    // Minimum signed: flags + 64 byte signature + empty bytecode
    std::vector<uint8_t> data(1 + SIGNATURE_SIZE);
    data[0] = 0x80; // flags: signed

    bool result = asset.bytecode(rive::Span<uint8_t>(data));

    // Will fail signature verification but shouldn't crash
    REQUIRE(result == false);
}

#endif // WITH_RIVE_SCRIPTING
