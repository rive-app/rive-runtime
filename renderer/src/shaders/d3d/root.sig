// d3d12 has the concept of descriptor heaps.
// This means we define the "slot" in our root
// signature and then offset our heap to represent positions in that signature.
// These are the offsets needed for each shader resource
// note: rtv's are not considered shader resource but they are a heap

// all srv and uav are one heap.
// srv
// these are dynamic
#define PATH_BUFFER_HEAP_OFFSET 0
#define PAINT_BUFFER_HEAP_OFFSET 1
#define PAINT_AUX_BUFFER_HEAP_OFFSET 2
#define CONTOUR_BUFFER_HEAP_OFFSET 3
// these are static
#define TESS_IMAGE_HEAP_OFFSET 4
#define GRAD_IMAGE_HEAP_OFFSET 5
#define GAUSSIAN_INTEGRAL_IMAGE_HEAP_OFFSET 6
#define FEATHER_ATLAS_IMAGE_HEAP_OFFSET 7
// uavs
#define ATOMIC_COLOR_HEAP_OFFSET 8
#define ATOMIC_CLIP_HEAP_OFFSET 9
#define ATOMIC_SCRATCH_COLOR_HEAP_OFFSET 10
#define ATOMIC_COVERAGE_HEAP_OFFSET 11
// images
#define IMAGE_HEAP_OFFSET_START 12

#define STATIC_SRV_UAV_HEAP_DESCRIPTOPR_START TESS_IMAGE_HEAP_OFFSET
#define DYNAMIC_SRV_UAV_HEAP_DESCRIPTOPR_START PATH_BUFFER_HEAP_OFFSET

// this is for copies that happen every logical flush
#define NUM_DYNAMIC_SRV_HEAP_DESCRIPTORS CONTOUR_BUFFER_HEAP_OFFSET + 1
#define NUM_SRV_HEAP_DESCRIPTORS FEATHER_ATLAS_IMAGE_HEAP_OFFSET + 1
// this is for the first flush where we copy everything the things that don't
// change only once
#define NUM_STATIC_SRV_UAV_HEAP_DESCRIPTORS                                    \
    ATOMIC_COVERAGE_HEAP_OFFSET - NUM_DYNAMIC_SRV_HEAP_DESCRIPTORS

#define NUM_SRV_UAV_HEAP_DESCRIPTORS IMAGE_HEAP_OFFSET_START + 1

// all samplers are another heap
#define TESS_SAMPLER_HEAP_OFFSET 0
#define GRAD_SAMPLER_HEAP_OFFSET 1
#define GAUSSIAN_INTEGRAL_SAMPLER_HEAP_OFFSET 2
#define FEATHER_ATLAS_SAMPLER_HEAP_OFFSET 3
#define IMAGE_SAMPLER_HEAP_OFFSET 4

#define NUM_SAMPLER_HEAP_DESCRIPTORS IMAGE_SAMPLER_HEAP_OFFSET + 1
// constant buffers are bound directly to the sig so we can change it out as we
// go, to change a heap we would have to create a new one and use SetHeaps which
// is very slow
#define FLUSH_UNIFORM_BUFFFER_SIG_INDEX 0
#define VERTEX_DRAW_UNIFORM_SIG_INDEX 1
#define DYNAMIC_SRV_SIG_INDEX 2
#define STATIC_SRV_SIG_INDEX 3
#define IMAGE_SIG_INDEX 4
#define UAV_SIG_INDEX 5
#define SAMPLER_SIG_INDEX 6
#define DYNAMIC_SAMPLER_SIG_INDEX 7

#define SRV_START_HEAP_INDEX PATH_BUFFER_HEAP_OFFSET
#define UAV_START_HEAP_INDEX ATOMIC_COLOR_HEAP_OFFSET
// samplers just start at 0

// rtv offsets
#define GRAD_RTV_HEAP_OFFSET 0
#define TESS_RTV_HEAP_OFFSET 1
#define FEATHER_ATLAS_RTV_HEAP_OFFSET 2
#define TARGET_RTV_HEAP_OFFSET 3

// ROOT_SIG is only consumed by fxc when compiling the root signature. Guard it
// (and its constants.glsl include) out of the C++ translation units that pull in
// this file just for the heap/sig-index constants above -- they neither need
// ROOT_SIG nor should pull the shader constants into their translation unit.
#ifndef __cplusplus

#include "../constants.glsl"
#define RIVE_STRINGIZE(X) #X
#define RIVE_STR(X) RIVE_STRINGIZE(X)

#define ROOT_SIG "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
    "CBV(b" RIVE_STR(FLUSH_UNIFORM_BUFFER_IDX) ", flags=DATA_VOLATILE, visibility=SHADER_VISIBILITY_ALL), "\
    "RootConstants(num32BitConstants=1, b" RIVE_STR(PATH_BASE_INSTANCE_UNIFORM_BUFFER_IDX) ", visibility=SHADER_VISIBILITY_VERTEX), "\
    "DescriptorTable(SRV(t" RIVE_STR(PATH_BUFFER_IDX) "), "\
                    "SRV(t" RIVE_STR(PAINT_BUFFER_IDX) "),"\
                    "SRV(t" RIVE_STR(PAINT_AUX_BUFFER_IDX) "),"\
                    "SRV(t" RIVE_STR(CONTOUR_BUFFER_IDX) "), visibility=SHADER_VISIBILITY_ALL), "\
    "DescriptorTable(SRV(t" RIVE_STR(TESS_VERTEX_TEXTURE_IDX) ", flags=DESCRIPTORS_VOLATILE),"\
                    "SRV(t" RIVE_STR(GRAD_TEXTURE_IDX) ", flags=DESCRIPTORS_VOLATILE), "\
                    "SRV(t" RIVE_STR(GAUSSIAN_INTEGRAL_TEXTURE_IDX) ", flags=DESCRIPTORS_VOLATILE), "\
                    "SRV(t" RIVE_STR(FEATHER_ATLAS_TEXTURE_IDX) ", flags=DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_ALL), "\
    "DescriptorTable(SRV(t" RIVE_STR(IMAGE_TEXTURE_IDX) ", flags=DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL), "\
    "DescriptorTable(UAV(u" RIVE_STR(COLOR_PLANE_IDX) ", flags=DESCRIPTORS_VOLATILE | DATA_VOLATILE),"\
                    "UAV(u" RIVE_STR(CLIP_PLANE_IDX) ", flags=DATA_VOLATILE),"\
                    "UAV(u" RIVE_STR(SCRATCH_COLOR_PLANE_IDX) ", flags=DESCRIPTORS_VOLATILE | DATA_VOLATILE),"\
                    "UAV(u" RIVE_STR(COVERAGE_PLANE_IDX) ", flags=DATA_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL),"\
    "DescriptorTable(Sampler(s" RIVE_STR(TESS_VERTEX_TEXTURE_IDX) "),"\
                    "Sampler(s" RIVE_STR(GRAD_TEXTURE_IDX) "),"\
                    "Sampler(s" RIVE_STR(GAUSSIAN_INTEGRAL_TEXTURE_IDX) "),"\
                    "Sampler(s" RIVE_STR(FEATHER_ATLAS_TEXTURE_IDX) "), visibility=SHADER_VISIBILITY_ALL),"\
    "DescriptorTable(Sampler(s" RIVE_STR(IMAGE_TEXTURE_IDX) ", flags=DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL) "

#endif // __cplusplus
