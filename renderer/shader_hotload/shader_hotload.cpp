/*
 * Copyright 2025 Rive
 */

#include <fstream>
#include <filesystem>
#include <vector>

#include "shader_hotload.hpp"
#include "rive/math/math_types.hpp"

// We also keep a uint32 header for the size of the bytecode span
static constexpr size_t NUM_EXTRA_BYTECODE_SPAN_BYTES = sizeof(uint32_t);

static size_t write_span_header_into_buffer(char* bytecodeAllocation,
                                            uint32_t fileSize)
{
    const size_t numSpanHeaderBytes = sizeof(fileSize);
    memcpy(bytecodeAllocation, &fileSize, numSpanHeaderBytes);
    return numSpanHeaderBytes;
}

static size_t load_bytecode_file_into_buffer(std::ifstream& shaderBytecodeFile,
                                             size_t fileSize,
                                             char* bytecodeAllocation)
{
    // Read entire file into buffer
    shaderBytecodeFile.read(
        bytecodeAllocation,
        rive::math::lossless_numeric_cast<std::streamsize>(fileSize));
    const size_t numActualBytesRead = shaderBytecodeFile.gcount();
    if (shaderBytecodeFile.good() && numActualBytesRead == fileSize)
    {
        return fileSize;
    }
    return numActualBytesRead;
}

static bool is_little_endian()
{
    uint32_t num = 1;
    uint8_t c[4];
    memcpy(&c[0], &num, 4);
    return (c[0] == 1);
}

static std::vector<uint32_t> spirvAllocation;
#define RIVE_SUBDIR "rive"
rive::Span<const uint32_t> loadNewShaderFileData()
{
    if (!is_little_endian())
    {
        fprintf(stderr,
                "Big endian machine detected! Spir-V data must be little "
                "endian.\n");
        abort();
    }

    printf("Loading updated shader bytecode files...\n");

    std::filesystem::path riveSpirvPath =
        std::filesystem::temp_directory_path() / RIVE_SUBDIR / "spirv";

    // List of all shader bytecode files
    std::filesystem::path spirvFileNames[] = {
        riveSpirvPath / "color_ramp.vert.spirv",
        riveSpirvPath / "color_ramp.frag.spirv",
        riveSpirvPath / "tessellate.vert.spirv",
        riveSpirvPath / "tessellate.frag.spirv",
        riveSpirvPath / "render_atlas.vert.spirv",
        riveSpirvPath / "render_atlas_fill.frag.spirv",
        riveSpirvPath / "render_atlas_stroke.frag.spirv",
        riveSpirvPath / "draw_path.vert.spirv",
        riveSpirvPath / "draw_path.frag.spirv",
        riveSpirvPath / "draw_interior_triangles.vert.spirv",
        riveSpirvPath / "draw_interior_triangles.frag.spirv",
        riveSpirvPath / "draw_atlas_blit.vert.spirv",
        riveSpirvPath / "draw_atlas_blit.frag.spirv",
        riveSpirvPath / "draw_image_mesh.vert.spirv",
        riveSpirvPath / "draw_image_mesh.frag.spirv",

        riveSpirvPath / "atomic_draw_path.vert.spirv",
        riveSpirvPath / "atomic_draw_path.frag.spirv",
        riveSpirvPath / "atomic_draw_path.fixedcolor_frag.spirv",
        riveSpirvPath / "atomic_draw_interior_triangles.vert.spirv",
        riveSpirvPath / "atomic_draw_interior_triangles.frag.spirv",
        riveSpirvPath / "atomic_draw_interior_triangles.fixedcolor_frag.spirv",
        riveSpirvPath / "atomic_draw_atlas_blit.vert.spirv",
        riveSpirvPath / "atomic_draw_atlas_blit.frag.spirv",
        riveSpirvPath / "atomic_draw_atlas_blit.fixedcolor_frag.spirv",
        riveSpirvPath / "atomic_draw_image_rect.vert.spirv",
        riveSpirvPath / "atomic_draw_image_rect.frag.spirv",
        riveSpirvPath / "atomic_draw_image_rect.fixedcolor_frag.spirv",
        riveSpirvPath / "atomic_draw_image_mesh.vert.spirv",
        riveSpirvPath / "atomic_draw_image_mesh.frag.spirv",
        riveSpirvPath / "atomic_draw_image_mesh.fixedcolor_frag.spirv",
        riveSpirvPath / "atomic_resolve.vert.spirv",
        riveSpirvPath / "atomic_resolve.frag.spirv",
        riveSpirvPath / "atomic_resolve.fixedcolor_frag.spirv",
        riveSpirvPath / "atomic_resolve_coalesced.vert.spirv",
        riveSpirvPath / "atomic_resolve_coalesced.frag.spirv",

#ifndef RIVE_ANDROID
        riveSpirvPath / "draw_clockwise_path.vert.spirv",
        riveSpirvPath / "draw_clockwise_path.frag.spirv",
        riveSpirvPath / "draw_clockwise_path.fixedcolor_frag.spirv",
        riveSpirvPath / "draw_clockwise_clip.frag.spirv",
        riveSpirvPath / "draw_clockwise_clip.fixedcolor_frag.spirv",
        riveSpirvPath / "draw_clockwise_interior_triangles.vert.spirv",
        riveSpirvPath / "draw_clockwise_interior_triangles.frag.spirv",
        riveSpirvPath /
            "draw_clockwise_interior_triangles.fixedcolor_frag.spirv",
        riveSpirvPath / "draw_clockwise_interior_triangles_clip.frag.spirv",
        riveSpirvPath /
            "draw_clockwise_interior_triangles_clip.fixedcolor_frag.spirv",
        riveSpirvPath / "draw_clockwise_atlas_blit.vert.spirv",
        riveSpirvPath / "draw_clockwise_atlas_blit.frag.spirv",
        riveSpirvPath / "draw_clockwise_atlas_blit.fixedcolor_frag.spirv",
        riveSpirvPath / "draw_clockwise_image_mesh.vert.spirv",
        riveSpirvPath / "draw_clockwise_image_mesh.frag.spirv",
        riveSpirvPath / "draw_clockwise_image_mesh.fixedcolor_frag.spirv",
#endif

        riveSpirvPath / "draw_clockwise_atomic_path.vert.spirv",
        riveSpirvPath / "draw_clockwise_atomic_path.frag.spirv",
        riveSpirvPath / "draw_clockwise_atomic_interior_triangles.vert.spirv",
        riveSpirvPath / "draw_clockwise_atomic_interior_triangles.frag.spirv",
        riveSpirvPath / "draw_clockwise_atomic_atlas_blit.vert.spirv",
        riveSpirvPath / "draw_clockwise_atomic_atlas_blit.frag.spirv",
        riveSpirvPath / "draw_clockwise_atomic_image_mesh.vert.spirv",
        riveSpirvPath / "draw_clockwise_atomic_image_mesh.frag.spirv",

        riveSpirvPath / "draw_msaa_path.vert.spirv",
        riveSpirvPath / "draw_msaa_path.noclipdistance_vert.spirv",
        riveSpirvPath / "draw_msaa_path.frag.spirv",
        riveSpirvPath / "draw_msaa_path.fixedcolor_frag.spirv",
        riveSpirvPath / "draw_msaa_stencil.vert.spirv",
        riveSpirvPath / "draw_msaa_stencil.frag.spirv",
        riveSpirvPath / "draw_msaa_stencil.fixedcolor_frag.spirv",
        riveSpirvPath / "draw_msaa_atlas_blit.vert.spirv",
        riveSpirvPath / "draw_msaa_atlas_blit.noclipdistance_vert.spirv",
        riveSpirvPath / "draw_msaa_atlas_blit.frag.spirv",
        riveSpirvPath / "draw_msaa_atlas_blit.fixedcolor_frag.spirv",
        riveSpirvPath / "draw_msaa_image_mesh.vert.spirv",
        riveSpirvPath / "draw_msaa_image_mesh.noclipdistance_vert.spirv",
        riveSpirvPath / "draw_msaa_image_mesh.frag.spirv",
        riveSpirvPath / "draw_msaa_image_mesh.fixedcolor_frag.spirv",
        riveSpirvPath / "draw_fullscreen_quad.vert.spirv",
        riveSpirvPath / "draw_input_attachment.frag.spirv",
        riveSpirvPath / "draw_msaa_color_seed_attachment.frag.spirv",
        riveSpirvPath / "draw_msaa_resolve.frag.spirv",
    };
    constexpr size_t numFiles = std::size(spirvFileNames);

    size_t spirvSpanSizes[numFiles] = {};
    size_t totalSpirvCodeSize = 0;
    for (size_t i = 0; i < numFiles; ++i)
    {
        std::string currFileName = spirvFileNames[i].string();
        if (!std::filesystem::exists(currFileName))
        {
            fprintf(stderr,
                    "File %s does not exist! Quitting.\n",
                    currFileName.c_str());
            return rive::make_span<const uint32_t>(0, 0);
        }

        const size_t fileSize = rive::math::lossless_numeric_cast<size_t>(
            std::filesystem::file_size(currFileName));
        if (fileSize > std::numeric_limits<uint32_t>::max())
        {
            fprintf(stderr,
                    "File %s is over 4gb (> 32 bits), aborting!\n",
                    currFileName.c_str());
            abort();
        }
        spirvSpanSizes[i] = fileSize;
        totalSpirvCodeSize += fileSize + NUM_EXTRA_BYTECODE_SPAN_BYTES;
    }

    // Allocate memory for loading files into
    spirvAllocation.resize(totalSpirvCodeSize / sizeof(uint32_t));
    char* currSpirvAllocPtr = reinterpret_cast<char*>(spirvAllocation.data());

    for (size_t i = 0; i < numFiles; ++i)
    {
        const size_t currFileSize = spirvSpanSizes[i];
        if (currFileSize == 0)
        {
            continue;
        }

        const size_t numExpectedBytesToProcess =
            NUM_EXTRA_BYTECODE_SPAN_BYTES + currFileSize;

        char* prevSpirvAllocPtr = currSpirvAllocPtr;

        // Write span header as size of bytecode in uint32s
        currSpirvAllocPtr += write_span_header_into_buffer(
            currSpirvAllocPtr,
            rive::math::lossless_numeric_cast<uint32_t>(currFileSize /
                                                        sizeof(uint32_t)));

        // Try to open the file
        std::ifstream shaderBytecodeFile;
        shaderBytecodeFile.open(spirvFileNames[i],
                                std::ios::in | std::ios::binary);
        if (!shaderBytecodeFile.is_open())
        {
            fprintf(stderr,
                    "Failed to open shader bytecode binary file. Giving up.\n");
            return rive::make_span<const uint32_t>(0, 0);
        }

        currSpirvAllocPtr += load_bytecode_file_into_buffer(shaderBytecodeFile,
                                                            currFileSize,
                                                            currSpirvAllocPtr);
        if (numExpectedBytesToProcess !=
            (currSpirvAllocPtr - prevSpirvAllocPtr))
        {
            fprintf(stderr,
                    "Error while reading spirv file: %s. Giving up.\n",
                    spirvFileNames[i].string().c_str());
            shaderBytecodeFile.close();
            return rive::make_span<const uint32_t>(0, 0);
        }
        shaderBytecodeFile.close();
    }

    printf("...done.\n");
    rive::Span<const uint32_t> bytecodeSpan =
        rive::make_span<const uint32_t>(spirvAllocation.data(),
                                        spirvAllocation.size());
    assert(bytecodeSpan.data());
    assert(bytecodeSpan.size() > 0);
    return bytecodeSpan;
}
