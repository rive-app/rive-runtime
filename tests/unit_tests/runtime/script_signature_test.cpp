#ifdef WITH_RIVE_SCRIPTING
#include "catch.hpp"
#include "rive_file_reader.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/assets/script_module_asset.hpp"
#include "rive/assets/shader_asset.hpp"
#ifdef WITH_RIVE_SCRIPTING_WASM
#include "rive/wasm/wasm_scripting_vm.hpp"
#endif
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace rive;

namespace
{
// The AnimaScript fixtures were exported with SampleSigningContext, whose key
// only builds carrying its public half accept; the Luau one was signed in
// production.
#ifdef WITH_RIVE_TEST_SIGNATURE
constexpr bool kSampleKeyVerifies = true;
#else
constexpr bool kSampleKeyVerifies = false;
#endif
constexpr bool kProductionKeyVerifies = !kSampleKeyVerifies;
#ifdef WITH_RIVE_TOOLS
constexpr bool kUnsignedAccepted = true;
#else
constexpr bool kUnsignedAccepted = false;
#endif

// AnimaScript only, so nothing in it is signed.
constexpr const char* kUnsigned = "assets/animascript.riv";
// AnimaScript beside a shader, whose signature covers the shader alone.
constexpr const char* kShaderSigned = "assets/animascript_shader_signed.riv";
// A Luau script and two shaders signed together.
constexpr const char* kLuauShader = "assets/luau_shader_signed.riv";

rcp<File> importBytes(const std::vector<uint8_t>& bytes,
                      bool requireSignedScripts = false)
{
    ImportResult result;
    auto file = File::import(bytes,
                             &gNoOpFactory,
                             &result,
                             nullptr,
                             nullptr,
                             requireSignedScripts);
    REQUIRE(result == ImportResult::success);
    REQUIRE(file != nullptr);
    return file;
}

template <typename T> std::vector<T*> assetsOf(File* file)
{
    std::vector<T*> found;
    for (auto asset : file->assets())
    {
        if (asset->is<T>())
        {
            found.push_back(asset->as<T>());
        }
    }
    return found;
}

std::vector<uint8_t>::iterator find(std::vector<uint8_t>& bytes,
                                    const char* text)
{
    auto at =
        std::search(bytes.begin(), bytes.end(), text, text + std::strlen(text));
    REQUIRE(at != bytes.end());
    return at;
}

// The file's signature property; it is the last one its contents object
// writes.
std::vector<uint8_t>::iterator findSignature(std::vector<uint8_t>& bytes)
{
    const uint8_t key[] = {0x8F, 0x07, 64};
    static_assert(FileAssetContentsBase::signaturePropertyKey == 0x38F,
                  "the key bytes above encode the signature property");
    auto at = std::search(bytes.begin(), bytes.end(), key, key + 3);
    REQUIRE(at != bytes.end());
    REQUIRE(at[3 + 64] == 0);
    return at;
}

void stripSignature(std::vector<uint8_t>& bytes)
{
    auto at = findSignature(bytes);
    bytes.erase(at, at + 3 + 64);
}

#ifndef __has_feature
#define __has_feature(x) 0
#endif

void checkModuleRuns(File* file)
{
    REQUIRE(assetsOf<ScriptModuleAsset>(file).size() == 1);
    // ASan's larger frames overflow WAMR's native stack check at instantiate.
#if defined(WITH_RIVE_SCRIPTING_WASM) && !defined(__SANITIZE_ADDRESS__) &&     \
    !__has_feature(address_sanitizer)
    REQUIRE(file->wasmScriptingVM() != nullptr);
    for (auto script : assetsOf<ScriptAsset>(file))
    {
        CHECK(script->isModule() == (script->generatorFunctionRef() == 0));
    }
#endif
}

bool shaderUsed(const ShaderAsset* shader)
{
    bool exposed = !shader->rstb().empty();
    if (!exposed)
    {
        CHECK(shader->textureSamplerPairs().empty());
        for (uint8_t target = 0; target <= 16; target++)
        {
            CHECK(shader->findShader(target).empty());
        }
    }
    return exposed;
}

void checkShadersUsed(File* file, bool used)
{
    auto shaders = assetsOf<ShaderAsset>(file);
    REQUIRE_FALSE(shaders.empty());
    for (auto shader : shaders)
    {
        CHECK(shaderUsed(shader) == used);
    }
}

#ifdef WITH_RIVE_SCRIPTING_LUAU
bool luauRuns(File* file) { return file->scriptingVM() != nullptr; }
#endif

#ifndef _WIN32
// What [run] prints to stderr.
template <typename Run> std::string stderrOf(Run run)
{
    fflush(stderr);
    FILE* capture = tmpfile();
    REQUIRE(capture != nullptr);
    int saved = dup(fileno(stderr));
    dup2(fileno(capture), fileno(stderr));
    run();
    fflush(stderr);
    dup2(saved, fileno(stderr));
    close(saved);
    std::string text;
    rewind(capture);
    for (int c = fgetc(capture); c != EOF; c = fgetc(capture))
    {
        text.push_back((char)c);
    }
    fclose(capture);
    return text;
}

size_t countOf(const std::string& text, const std::string& needle)
{
    size_t count = 0;
    for (size_t at = text.find(needle); at != std::string::npos;
         at = text.find(needle, at + 1))
    {
        count++;
    }
    return count;
}
#endif
} // namespace

TEST_CASE("an unsigned AnimaScript module runs", "[signing]")
{
    for (const char* path : {kUnsigned, kShaderSigned})
    {
        for (bool requireSigned : {false, true})
        {
            auto file = importBytes(ReadFile(path), requireSigned);
            checkModuleRuns(file.get());
        }
    }
}

#if defined(WITH_RIVE_SCRIPTING_WASM) && !defined(__EMSCRIPTEN__)
TEST_CASE("a module slot holding AOT code is refused", "[signing]")
{
    // Unsigned, a file must never hand WAMR precompiled native code to run.
    const uint8_t aot[] = {0, 'a', 'o', 't', 1, 0, 0, 0};
    std::string error;
    CHECK(WasmScriptingVM::make(Span<const uint8_t>(aot, sizeof(aot)),
                                &gNoOpFactory,
                                error) == nullptr);
    CHECK(error == "script module is not wasm bytecode");

    auto bytes = ReadFile(kUnsigned);
    const uint8_t wasm[] = {0, 'a', 's', 'm'};
    auto at = std::search(bytes.begin(), bytes.end(), wasm, wasm + 4);
    REQUIRE(at != bytes.end());
    std::copy(aot, aot + 4, at);
    for (bool requireSigned : {false, true})
    {
        auto file = importBytes(bytes, requireSigned);
        CHECK(file->wasmScriptingVM() == nullptr);
    }
}
#endif

TEST_CASE("the signature beside an AnimaScript module covers the shader alone",
          "[signing]")
{
    auto file = importBytes(ReadFile(kShaderSigned));
    auto shaders = assetsOf<ShaderAsset>(file.get());
    REQUIRE(shaders.size() == 1);
    CHECK(shaders.front()->verified() == kSampleKeyVerifies);
    // Records carry no contents, so they are no part of the signed group.
    for (auto script : assetsOf<ScriptAsset>(file.get()))
    {
        CHECK_FALSE(script->verified());
    }
    checkShadersUsed(file.get(), kUnsignedAccepted || kSampleKeyVerifies);
    // The test key proves nothing about bytes the embedder did not supply.
    auto required = importBytes(ReadFile(kShaderSigned), true);
    checkShadersUsed(required.get(), false);
}

TEST_CASE("a signed file's shaders are used", "[signing]")
{
    auto file = importBytes(ReadFile(kLuauShader));
    checkShadersUsed(file.get(), kUnsignedAccepted || kProductionKeyVerifies);
    auto required = importBytes(ReadFile(kLuauShader), true);
    checkShadersUsed(required.get(), kProductionKeyVerifies);
}

TEST_CASE("a tampered shader is refused with the Luau it was signed with",
          "[signing]")
{
    for (const char* path : {kShaderSigned, kLuauShader})
    {
        auto bytes = ReadFile(path);
        find(bytes, "VertexOut")[0] ^= 0x01;
        for (bool requireSigned : {false, true})
        {
            auto file = importBytes(bytes, requireSigned);
            for (auto shader : assetsOf<ShaderAsset>(file.get()))
            {
                CHECK_FALSE(shader->verified());
            }
            checkShadersUsed(file.get(), kUnsignedAccepted && !requireSigned);
            for (auto script : assetsOf<ScriptAsset>(file.get()))
            {
                CHECK_FALSE(script->verified());
            }
#ifdef WITH_RIVE_SCRIPTING_LUAU
            if (path == kLuauShader && requireSigned)
            {
                CHECK_FALSE(luauRuns(file.get()));
            }
#endif
        }
    }
}

TEST_CASE("a shader without its signature is refused unless unsigned content "
          "is accepted",
          "[signing]")
{
    for (const char* path : {kShaderSigned, kLuauShader})
    {
        auto bytes = ReadFile(path);
        stripSignature(bytes);
        for (bool requireSigned : {false, true})
        {
            auto file = importBytes(bytes, requireSigned);
            for (auto shader : assetsOf<ShaderAsset>(file.get()))
            {
                CHECK_FALSE(shader->verified());
            }
            checkShadersUsed(file.get(), kUnsignedAccepted && !requireSigned);
        }
    }
}

TEST_CASE("a referenced shader verifies its own signature", "[signing]")
{
    // The fixture's group holds the shader alone, so its signature is the one
    // a referenced export embeds.
    auto bytes = ReadFile(kShaderSigned);
    auto file = importBytes(bytes);
    auto shaders = assetsOf<ShaderAsset>(file.get());
    REQUIRE(shaders.size() == 1);
    auto rstb = shaders.front()->rstb();
    REQUIRE_FALSE(rstb.empty());
    auto signature = findSignature(bytes) + 3;
    std::vector<uint8_t> envelope = {0x80};
    envelope.insert(envelope.end(), signature, signature + 64);
    envelope.insert(envelope.end(), rstb.begin(), rstb.end());

    auto tampered = envelope;
    tampered.back() ^= 0x01;
    for (bool requireSigned : {false, true})
    {
        ShaderAsset referenced;
        referenced.importedWith(requireSigned);
        REQUIRE(referenced.decode(envelope, nullptr));
        CHECK(referenced.verified() == kSampleKeyVerifies);
        CHECK(shaderUsed(&referenced) ==
              (!requireSigned && (kUnsignedAccepted || kSampleKeyVerifies)));

        ShaderAsset forged;
        forged.importedWith(requireSigned);
        REQUIRE(forged.decode(tampered, nullptr));
        CHECK_FALSE(forged.verified());
        CHECK(shaderUsed(&forged) == (!requireSigned && kUnsignedAccepted));
    }
}

TEST_CASE("a Luau and shader file signed together verifies its Luau",
          "[signing]")
{
    auto file = importBytes(ReadFile(kLuauShader));
    auto scripts = assetsOf<ScriptAsset>(file.get());
    REQUIRE_FALSE(scripts.empty());
    for (auto script : scripts)
    {
        CHECK(script->verified() == kProductionKeyVerifies);
    }
    for (auto shader : assetsOf<ShaderAsset>(file.get()))
    {
        CHECK(shader->verified() == kProductionKeyVerifies);
    }
#ifdef WITH_RIVE_SCRIPTING_LUAU
    auto required = importBytes(ReadFile(kLuauShader), true);
    CHECK(luauRuns(required.get()) == kProductionKeyVerifies);
#endif
}

TEST_CASE("a tampered Luau script is refused", "[signing]")
{
    auto bytes = ReadFile(kLuauShader);
    find(bytes, "add WGSL assets")[0] ^= 0x01;
    auto file = importBytes(bytes, /*requireSignedScripts*/ true);
    for (auto script : assetsOf<ScriptAsset>(file.get()))
    {
        CHECK_FALSE(script->verified());
    }
    checkShadersUsed(file.get(), false);
#ifdef WITH_RIVE_SCRIPTING_LUAU
    CHECK_FALSE(luauRuns(file.get()));
#endif
}
#ifndef _WIN32
TEST_CASE("a refused shader is reported once its file is read, never on reads",
          "[signing]")
{
    auto bytes = ReadFile(kLuauShader);
    find(bytes, "VertexOut")[0] ^= 0x01;
    const std::string refusal = "is unavailable: its signature did not verify";
    rcp<File> file;
    std::string imported = stderrOf(
        [&] { file = importBytes(bytes, /*requireSignedScripts*/ true); });
    auto shaders = assetsOf<ShaderAsset>(file.get());
    CHECK(countOf(imported, refusal) == shaders.size());
    std::string read = stderrOf([&] { checkShadersUsed(file.get(), false); });
    CHECK(countOf(read, refusal) == 0);
}
#endif
#endif
