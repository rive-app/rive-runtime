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
#define FEATHER_IMAGE_HEAP_OFFSET 6
#define ATLAS_IMAGE_HEAP_OFFSET 7
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
#define NUM_SRV_HEAP_DESCRIPTORS ATLAS_IMAGE_HEAP_OFFSET + 1
// this is for the first flush where we copy everything the things that don't
// change only once
#define NUM_STATIC_SRV_UAV_HEAP_DESCRIPTORS                                    \
    ATOMIC_COVERAGE_HEAP_OFFSET - NUM_DYNAMIC_SRV_HEAP_DESCRIPTORS

#define NUM_SRV_UAV_HEAP_DESCRIPTORS IMAGE_HEAP_OFFSET_START + 1

// all samplers are another heap
#define TESS_SAMPLER_HEAP_OFFSET 0
#define GRAD_SAMPLER_HEAP_OFFSET 1
#define FEATHER_SAMPLER_HEAP_OFFSET 2
#define ATLAS_SAMPLER_HEAP_OFFSET 3
#define IMAGE_SAMPLER_HEAP_OFFSET 4

#define NUM_SAMPLER_HEAP_DESCRIPTORS IMAGE_SAMPLER_HEAP_OFFSET + 1
// constant buffers are bound directly to the sig so we can change it out as we
// go, to change a heap we would have to create a new one and use SetHeaps which
// is very slow
#define FLUSH_UNIFORM_BUFFFER_SIG_INDEX 0
#define IMAGE_UNIFORM_BUFFFER_SIG_INDEX 1
#define VERTEX_DRAW_UNIFORM_SIG_INDEX 2
#define DYNAMIC_SRV_SIG_INDEX 3
#define STATIC_SRV_SIG_INDEX 4
#define IMAGE_SIG_INDEX 5
#define UAV_SIG_INDEX 6
#define SAMPLER_SIG_INDEX 7

#define SRV_START_HEAP_INDEX PATH_BUFFER_HEAP_OFFSET
#define UAV_START_HEAP_INDEX ATOMIC_COLOR_HEAP_OFFSET
// samplers just start at 0

// rtv offsets
#define GRAD_RTV_HEAP_OFFSET 0
#define TESS_RTV_HEAP_OFFSET 1
#define ATLAS_RTV_HEAP_OFFSET 2
#define TARGET_RTV_HEAP_OFFSET 3

// The root signature macro doesn't seem to support string splicing. We need to manually ensure the numbers here stay in sync with the above definitions.
#define ROOT_SIG "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
    "CBV(b0, flags=DATA_VOLATILE, visibility=SHADER_VISIBILITY_ALL), "\
    "CBV(b2, flags=DATA_VOLATILE, visibility=SHADER_VISIBILITY_ALL), "\
    "RootConstants(num32BitConstants=1, b1, visibility=SHADER_VISIBILITY_VERTEX), "\
    "DescriptorTable(SRV(t3), "\
                    "SRV(t4),"\
                    "SRV(t5),"\
                    "SRV(t6), visibility=SHADER_VISIBILITY_ALL), "\
    "DescriptorTable(SRV(t8, flags=DESCRIPTORS_VOLATILE),"\
                    "SRV(t9, flags=DESCRIPTORS_VOLATILE), "\
                    "SRV(t10, flags=DESCRIPTORS_VOLATILE), " \
                    "SRV(t11, flags=DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_ALL), "\
    "DescriptorTable(SRV(t12, flags=DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL), "\
    "DescriptorTable(UAV(u0, flags=DESCRIPTORS_VOLATILE | DATA_VOLATILE),"\
                    "UAV(u1, flags=DATA_VOLATILE),"\
                    "UAV(u2, flags=DESCRIPTORS_VOLATILE | DATA_VOLATILE),"\
                    "UAV(u3, flags=DATA_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL),"\
    "DescriptorTable(Sampler(s8),"\
                    "Sampler(s9),"\
                    "Sampler(s10),"\
                    "Sampler(s11),"\
                    "Sampler(s12), visibility=SHADER_VISIBILITY_ALL)"
