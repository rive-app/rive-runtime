"""Single source for the rive_*_v1 wasm binding contract.

generate.py projects this table into the public C import header
(include/rive/wasm/rive_bindings_v1.h, the third party contract), the WAMR
NativeSymbol tables with drift checked prototypes (wasm_natives_gen.hpp), and
the node test stubs (luau_wasm/tests/vm_host_stubs.mjs). luau_wasm/test.sh
fails when the generated files drift from this source.
"""


def u32(name):
    return {'kind': 'u32', 'name': name}


def i32(name):
    return {'kind': 'i32', 'name': name}


def f32(name):
    return {'kind': 'f32', 'name': name}


def handle(tag, name=None):
    return {'kind': 'handle', 'tag': tag, 'name': name or tag}


# Wasm validated pointer plus element count; arrives host side translated.
def buf(elem, name, count):
    return {'kind': 'buf', 'elem': elem, 'name': name, 'count': count}


# Same validation but the host writes into it.
def mutbuf(elem, name, count):
    return {'kind': 'mutbuf', 'elem': elem, 'name': name, 'count': count}


# Raw guest address the host validates and translates itself, for pointer
# pairs sharing one count.
def addr(elem, name):
    return {'kind': 'addr', 'elem': elem, 'name': name}


def string(name, count):
    return {'kind': 'str', 'name': name, 'count': count}


# Descriptor struct crossing as pointer plus byte count; u32 and f32 fields
# only, all four bytes, so the layout is identical module and host side. The
# byte count doubles as the struct version: hosts reject anything smaller
# than the fields they know.
def pod(name, fields):
    return {'name': name, 'fields': fields}


def podref(podname, name):
    return {'kind': 'pod', 'pod': podname, 'name': name}


PODS = [
    pod('gpu_texture_desc', [
        u32('width'),
        u32('height'),
        u32('depthOrArrayLayers'),
        u32('format'),
        u32('textureType'),
        u32('renderTarget'),
        u32('numMipmaps'),
        u32('sampleCount'),
    ]),
    pod('gpu_texture_upload', [
        u32('bytesPerRow'),
        u32('rowsPerImage'),
        u32('mipLevel'),
        u32('layer'),
        u32('x'),
        u32('y'),
        u32('z'),
        u32('width'),
        u32('height'),
        u32('depth'),
    ]),
    pod('gpu_sampler_desc', [
        u32('minFilter'),
        u32('magFilter'),
        u32('mipmapFilter'),
        u32('wrapU'),
        u32('wrapV'),
        u32('wrapW'),
        u32('compare'),
        f32('minLod'),
        f32('maxLod'),
        u32('maxAnisotropy'),
    ]),
    pod('gpu_texture_view_desc', [
        u32('dimension'),
        u32('aspect'),
        u32('baseMipLevel'),
        u32('mipCount'),
        u32('baseLayer'),
        u32('layerCount'),
    ]),
    # Sizes slice the op's blob argument in field order; absent parts are 0.
    pod('gpu_shader_module_desc', [
        u32('language'),
        u32('stage'),
        u32('codeSize'),
        u32('hlslSourceSize'),
        u32('hlslEntryPointSize'),
        u32('bindingMapSize'),
        u32('glFixupSize'),
        u32('shaderAssetId'),
    ]),
    # Array element pods cross via buf() with a byte count; the count must be
    # an exact multiple of the struct size.
    pod('gpu_bind_group_layout_entry', [
        u32('binding'),
        u32('kind'),
        u32('visibility'),
        u32('hasDynamicOffset'),
        u32('textureViewDim'),
        u32('textureSampleType'),
        u32('textureMultisampled'),
        u32('minBindingSize'),
        u32('nativeSlotVS'),
        u32('nativeSlotFS'),
        u32('nativeSlotCS'),
    ]),
    pod('gpu_bind_group_ubo', [
        u32('slot'),
        u32('buffer'),
        u32('offset'),
        u32('size'),
    ]),
    pod('gpu_bind_group_texture', [
        u32('slot'),
        u32('view'),
    ]),
    pod('gpu_bind_group_sampler', [
        u32('slot'),
        u32('sampler'),
    ]),
    pod('gpu_color_target', [
        u32('format'),
        u32('blendEnabled'),
        u32('srcColor'),
        u32('dstColor'),
        u32('colorOp'),
        u32('srcAlpha'),
        u32('dstAlpha'),
        u32('alphaOp'),
        u32('writeMask'),
    ]),
    pod('gpu_vertex_buffer_layout', [
        u32('stride'),
        u32('stepMode'),
        u32('attributeCount'),
    ]),
    pod('gpu_vertex_attribute', [
        u32('format'),
        u32('offset'),
        u32('shaderSlot'),
    ]),
    pod('gpu_pass_color_attachment', [
        u32('view'),
        u32('resolveTarget'),
        u32('loadOp'),
        u32('storeOp'),
        f32('clearR'),
        f32('clearG'),
        f32('clearB'),
        f32('clearA'),
    ]),
    pod('gpu_pass_desc', [
        u32('colorCount'),
        u32('depthView'),
        u32('depthLoadOp'),
        u32('depthStoreOp'),
        f32('depthClearValue'),
        u32('stencilLoadOp'),
        u32('stencilStoreOp'),
        u32('stencilClearValue'),
    ]),
    # Sizes and counts slice the op's blob in field order: vertex entry
    # string, fragment entry string, color targets, vertex buffer layouts,
    # their attributes (concatenated in layout order), then bind group layout
    # handles as u32s (0 = null group).
    pod('gpu_pipeline_desc', [
        u32('vertexModule'),
        u32('fragmentModule'),
        u32('vertexEntrySize'),
        u32('fragmentEntrySize'),
        u32('colorCount'),
        u32('vertexBufferCount'),
        u32('attributeCount'),
        u32('bindGroupLayoutCount'),
        u32('topology'),
        u32('indexFormat'),
        u32('cullMode'),
        u32('winding'),
        u32('depthFormat'),
        u32('depthCompare'),
        u32('depthWriteEnabled'),
        u32('depthBias'),
        f32('depthBiasSlopeScale'),
        f32('depthBiasClamp'),
        u32('stencilFrontCompare'),
        u32('stencilFrontFailOp'),
        u32('stencilFrontDepthFailOp'),
        u32('stencilFrontPassOp'),
        u32('stencilBackCompare'),
        u32('stencilBackFailOp'),
        u32('stencilBackDepthFailOp'),
        u32('stencilBackPassOp'),
        u32('stencilReadMask'),
        u32('stencilWriteMask'),
        u32('sampleCount'),
    ]),
]


def op(name, params=(), ret=None, stub=None):
    return {'name': name, 'params': list(params), 'ret': ret, 'stub': stub}


def ns(module, short, ops):
    return {'module': module, 'short': short, 'ops': ops}


NAMESPACES = [
    ns('rive_rt_v1', 'rt', [
        op('log', [i32('level'), string('message', 'length')], stub='hook'),
        op('mark_needs_update', [handle('object')]),
        # Raised by the module's fuel checks when the execution budget
        # passes; the host prints the timeout and traps the op.
        op('budget_exceeded', [u32('ms')]),
    ]),
    # Data binding tracer: numbers only; more property types, listeners, and
    # the valueChanged callback channel land with the properties namespace.
    ns('rive_data_v1', 'data', [
        op('view_model', [handle('object')], ret='u32'),
        # context:rootViewModel(): the data context's root instance.
        op('root_view_model', [handle('object')], ret='u32'),
        # context:globalViewModel(name): resolved through the object's file
        # and data context like the Luau lane.
        op('global_view_model', [handle('object'),
                                 string('name', 'nameLength')], ret='u32'),
        # Newline-joined names with the retrying length contract.
        op('global_view_model_names', [
            handle('object'),
            mutbuf('char', 'buffer', 'capacity'),
        ], ret='u32'),
        # context:dataContext() and the DataContext surface.
        op('context', [handle('object')], ret='u32'),
        op('context_parent', [handle('dataContext')], ret='u32'),
        op('context_view_model', [handle('dataContext')], ret='u32'),
        op('context_release', [handle('dataContext')]),
        # The Data global: constructors over the file's view models. Empty
        # template means a default instance.
        op('has_view_model', [string('name', 'nameLength')], ret='u32'),
        op('new_view_model', [string('name', 'nameLength'),
                              string('templateName', 'templateLength')],
           ret='u32'),
        op('vmi_release', [handle('vmi')]),
        op('vmi_number', [handle('vmi'), string('name', 'length')],
           ret='u32'),
        op('vmi_boolean', [handle('vmi'), string('name', 'length')],
           ret='u32'),
        op('vmi_string', [handle('vmi'), string('name', 'length')],
           ret='u32'),
        op('vmi_trigger', [handle('vmi'), string('name', 'length')],
           ret='u32'),
        op('vmi_color', [handle('vmi'), string('name', 'length')],
           ret='u32'),
        op('vmi_view_model', [handle('vmi'), string('name', 'length')],
           ret='u32'),
        # Dynamic vm.name reads: kindOut[0] receives the DataPropertyWire
        # kind, kindOut[1] the symbol list index value when that is the kind.
        op('vmi_property', [handle('vmi'), string('name', 'length'),
                            mutbuf('uint32_t', 'kindOut', 'kindCount')],
           ret='u32'),
        # vm:instance(name?): a fresh instance of the same view model; empty
        # name means the default instance.
        op('vmi_instance', [handle('vmi'), string('name', 'nameLength')],
           ret='u32'),
        # vm:getIndex(); ~0u when the instance carries no symbol list index.
        op('vmi_symbol_index', [handle('vmi')], ret='u32'),
        # __eq: both handles wrap the same core instance.
        op('vmi_equal', [handle('a'), handle('b')], ret='u32'),
        # The property's .value: the referenced instance as a new vmi handle.
        op('view_model_get', [handle('property')], ret='u32'),
        op('vmi_list', [handle('vmi'), string('name', 'length')], ret='u32'),
        op('vmi_enum', [handle('vmi'), string('name', 'length')], ret='u32'),
        op('vmi_image', [handle('vmi'), string('name', 'length')], ret='u32'),
        op('vmi_font', [handle('vmi'), string('name', 'length')], ret='u32'),
        op('vmi_blob', [handle('vmi'), string('name', 'length')], ret='u32'),
        # The property's image as a rive_image_v1 handle, resolved through
        # the instance's embedded asset or the file registry; 0 when none.
        op('image_get', [handle('property')], ret='u32'),
        op('image_set', [handle('property'), handle('image')]),
        # Fonts cross as opaque handles: the module has no font surface, so
        # the value only round trips through get/set.
        op('font_get', [handle('property')], ret='u32'),
        op('font_set', [handle('property'), handle('font')]),
        op('font_release', [handle('font')]),
        # 1 when a blob resolves, including a zero byte runtime-set blob;
        # distinguishes nil from empty since blob_get returns 0 for both.
        op('blob_present', [handle('property')], ret='u32'),
        op('blob_get', [handle('property'),
                        mutbuf('uint8_t', 'buffer', 'capacity')], ret='u32'),
        op('blob_name', [handle('property'),
                         mutbuf('char', 'buffer', 'capacity')], ret='u32'),
        op('blob_set', [handle('property'),
                        buf('uint8_t', 'bytes', 'byteCount')]),
        op('blob_clear', [handle('property')]),
        # Same retrying length contract as string_get; the value is the
        # current key resolved through the property's data enum.
        op('enum_get', [handle('property'),
                        mutbuf('char', 'buffer', 'capacity')], ret='u32'),
        op('enum_set', [handle('property'), string('value', 'length')]),
        # enum:values(): newline-joined keys with the retrying contract.
        op('enum_values', [handle('property'),
                           mutbuf('char', 'buffer', 'capacity')], ret='u32'),
        op('prop_release', [handle('property')]),
        op('trigger_fire', [handle('property')]),
        op('list_length', [handle('property')], ret='u32'),
        op('list_push', [handle('property'), handle('vmi')]),
        # Removed ends return the item's instance as a new vmi handle, 0 when
        # empty.
        op('list_pop', [handle('property')], ret='u32'),
        op('list_shift', [handle('property')], ret='u32'),
        op('list_clear', [handle('property')]),
        # Indices are zero based on the wire; the module owns the script's
        # one based convention and range errors.
        op('list_swap', [handle('property'), u32('index1'), u32('index2')]),
        op('list_insert', [handle('property'), handle('vmi'), u32('index')]),
        op('list_remove', [handle('property'), handle('vmi')]),
        op('list_remove_at', [handle('property'), u32('index')]),
        op('list_remove_all_of', [handle('property'), handle('vmi')]),
        # list[i]: the item's instance as a new vmi handle, 0 out of range.
        op('list_get', [handle('property'), u32('index')], ret='u32'),
        op('view_model_set', [handle('property'), handle('vmi')]),
        op('color_get', [handle('property')], ret='u32'),
        op('color_set', [handle('property'), u32('value')]),
        op('number_get', [handle('property')], ret='f32'),
        op('number_set', [handle('property'), f32('value')]),
        op('boolean_get', [handle('property')], ret='u32'),
        op('boolean_set', [handle('property'), u32('value')]),
        # Returns the full byte length; copies what fits, callers retry with
        # a larger buffer when truncated.
        op('string_get', [handle('property'),
                          mutbuf('char', 'buffer', 'capacity')], ret='u32'),
        op('string_set', [handle('property'), string('value', 'length')]),
        # Value change delivery: the host calls the module's exported
        # host_data_value_changed(token) whenever a watched property's value
        # changes.
        op('watch', [handle('property'), u32('token')]),
        op('unwatch', [handle('property')]),
        # The converted value produced inside host_obj_data_convert, written
        # into the ScriptDataResult the pending callDataConvert points at;
        # kind values are DataConvertWire's.
        op('convert_result', [
            u32('kind'),
            f32('number'),
            u32('booleanValue'),
            u32('color'),
            string('value', 'length'),
        ]),
    ]),
    # Artboard inputs: the host owns the instance (and its state machine and
    # bound view model); the module's Artboard userdata mirrors the Luau
    # lane's surface over these ops.
    ns('rive_artboard_v1', 'artboard', [
        op('release', [handle('artboard')]),
        op('advance', [handle('artboard'), f32('seconds')], ret='u32'),
        op('draw', [handle('artboard'), handle('renderer')]),
        # vmi 0 auto-creates the clone's instance like the Luau lane.
        op('instance', [handle('artboard'), handle('vmi')], ret='u32'),
        # The bound view model instance as a new vmi handle.
        op('data', [handle('artboard')], ret='u32'),
        op('width', [handle('artboard')], ret='f32'),
        op('height', [handle('artboard')], ret='f32'),
        op('set_width', [handle('artboard'), f32('value')]),
        op('set_height', [handle('artboard'), f32('value')]),
        op('frame_origin', [handle('artboard')], ret='u32'),
        op('set_frame_origin', [handle('artboard'), u32('value')]),
        # minX, minY, maxX, maxY.
        op('bounds', [handle('artboard'), mutbuf('float', 'out', 'outCount')]),
        # kind is ArtboardWire's pointer kind; returns the HitResult, 0 when
        # the artboard has no state machine.
        op('pointer_event', [
            handle('artboard'),
            u32('kind'),
            u32('pointerId'),
            f32('x'),
            f32('y'),
        ], ret='u32'),
        op('animation', [handle('artboard'), string('name', 'length')],
           ret='u32'),
        op('animation_release', [handle('animation')]),
        op('animation_duration', [handle('animation')], ret='f32'),
        op('animation_advance', [handle('animation'), f32('seconds')],
           ret='u32'),
        # mode is ArtboardWire's time mode (seconds/frames/percentage).
        op('animation_set_time', [
            handle('animation'),
            f32('value'),
            u32('mode'),
        ]),
        op('node', [handle('artboard'), string('name', 'length')], ret='u32'),
        op('node_release', [handle('node')]),
        # x, y, rotation, scaleX, scaleY.
        op('node_transform', [handle('node'),
                              mutbuf('float', 'out', 'outCount')]),
        # field is ArtboardWire's node field; vector fields use v0, v1.
        op('node_set', [handle('node'), u32('field'), f32('v0'), f32('v1')]),
        op('node_world_transform', [handle('node'),
                                    mutbuf('float', 'out', 'outCount')]),
        op('node_set_world_transform', [
            handle('node'),
            buf('float', 'values', 'floatCount'),
        ]),
        # node:decompose(mat): the world matrix crosses, the parent-space
        # decomposition applies host side.
        op('node_decompose', [handle('node'),
                              buf('float', 'values', 'floatCount')]),
        # Retrying copy-out; ~0u means the node is not a Path.
        op('node_path_verbs', [handle('node'),
                               mutbuf('uint8_t', 'out', 'outCount')],
           ret='u32'),
        op('node_path_points', [handle('node'),
                                mutbuf('float', 'out', 'outCount')],
           ret='u32'),
        # style, join, cap, blendMode, color, thicknessBits, featherBits;
        # returns 0 when the node carries no shape paint.
        op('node_paint', [handle('node'),
                          mutbuf('uint32_t', 'out', 'outCount')], ret='u32'),
    ]),
    ns('rive_path_v1', 'path', [
        op('new', ret='u32'),
        op('update', [
            handle('path'),
            buf('uint8_t', 'verbs', 'verbCount'),
            buf('float', 'points', 'floatCount'),
            u32('fillRule'),
        ]),
        op('release', [handle('path')]),
        # The geometry the script's effect update produced, appended into the
        # RawPath the pending callPathEffectUpdate points at; the inverse of
        # update's module-to-host crossing.
        op('effect_result', [
            buf('uint8_t', 'verbs', 'verbCount'),
            buf('float', 'points', 'floatCount'),
        ]),
    ]),
    ns('rive_paint_v1', 'paint', [
        op('new', ret='u32'),
        op('release', [handle('paint')]),
        op('style', [handle('paint'), u32('value')]),
        op('color', [handle('paint'), u32('value')]),
        op('thickness', [handle('paint'), f32('value')]),
        op('join', [handle('paint'), u32('value')]),
        op('cap', [handle('paint'), u32('value')]),
        op('blend_mode', [handle('paint'), u32('value')]),
        op('feather', [handle('paint'), f32('value')]),
        op('shader', [handle('paint'), handle('shader')]),
    ]),
    # GPU tracer subset: one canvas, one clear pass. The full surface lands
    # with descriptor PODs from ore_resource_commands.hpp.
    ns('rive_gpu_v1', 'gpu', [
        # Host GPU capability snapshot in context:features() declaration
        # order, bools as 0/1. Returns values written; 0 means no ore
        # context so the module answers conservative defaults, ~0u means
        # the recording carries no ReplayCaps yet so the module raises
        # like the Luau backend.
        op('features', [mutbuf('uint32_t', 'out', 'outCount')], ret='u32'),
        op('canvas_new', [u32('width'), u32('height')], ret='u32'),
        op('canvas_release', [handle('canvas')]),
        # Mints a view handle over the canvas's own color view; props receives
        # width, height, format, sampleCount so module-side attachment
        # validation sees real metadata.
        op('canvas_color_view', [
            handle('canvas'),
            mutbuf('uint32_t', 'props', 'propCount'),
        ], ret='u32'),
        # Mints an image handle over the canvas's presentable RenderImage,
        # the canvas .image surface.
        op('canvas_image', [handle('canvas')], ret='u32'),
        # Recreates the host canvas at a new size and mints a view over the
        # fresh color texture; props mirror canvas_color_view. The canvas
        # handle itself stays valid.
        op('canvas_resize', [
            handle('canvas'),
            u32('width'),
            u32('height'),
            mutbuf('uint32_t', 'props', 'propCount'),
        ], ret='u32'),
        op('pass_begin', [
            podref('gpu_pass_desc', 'desc'),
            buf('rive_gpu_pass_color_attachment_v1', 'colors',
                'colorByteCount'),
        ], ret='u32'),
        op('pass_set_pipeline', [handle('pass'), handle('pipeline')]),
        op('pass_set_vertex_buffer', [
            handle('pass'),
            u32('slot'),
            handle('buffer'),
            u32('offset'),
        ]),
        op('pass_set_index_buffer', [
            handle('pass'),
            handle('buffer'),
            u32('indexFormat'),
            u32('offset'),
        ]),
        op('pass_set_bind_group', [
            handle('pass'),
            u32('groupIndex'),
            handle('bindGroup'),
            buf('uint32_t', 'dynamicOffsets', 'dynamicOffsetByteCount'),
        ]),
        op('pass_set_viewport', [
            handle('pass'),
            f32('x'),
            f32('y'),
            f32('width'),
            f32('height'),
            f32('minDepth'),
            f32('maxDepth'),
        ]),
        op('pass_set_scissor', [
            handle('pass'),
            u32('x'),
            u32('y'),
            u32('width'),
            u32('height'),
        ]),
        op('pass_set_stencil_reference', [handle('pass'), u32('ref')]),
        op('pass_set_blend_color', [
            handle('pass'),
            f32('r'),
            f32('g'),
            f32('b'),
            f32('a'),
        ]),
        op('pass_draw', [
            handle('pass'),
            u32('vertexCount'),
            u32('instanceCount'),
            u32('firstVertex'),
            u32('firstInstance'),
        ]),
        op('pass_draw_indexed', [
            handle('pass'),
            u32('indexCount'),
            u32('instanceCount'),
            u32('firstIndex'),
            i32('baseVertex'),
            u32('firstInstance'),
        ]),
        op('pass_finish', [handle('pass')]),
        op('pass_release', [handle('pass')]),
        # Recording-side Image:view(): the host records the wrap by resource
        # id and the module gets a sampling view handle.
        op('image_view', [
            handle('image'),
            u32('width'),
            u32('height'),
        ], ret='u32'),
        # ore BufferDesc fields; usage is the BufferUsage enum, dataCount 0
        # means no initial contents.
        op('buffer_new', [
            u32('usage'),
            u32('sizeInBytes'),
            u32('immutable'),
            buf('uint8_t', 'data', 'dataCount'),
        ], ret='u32'),
        op('buffer_update', [
            handle('buffer'),
            u32('dstOffset'),
            buf('uint8_t', 'data', 'dataCount'),
        ]),
        op('buffer_release', [handle('buffer')]),
        op('texture_new', [podref('gpu_texture_desc', 'desc')], ret='u32'),
        op('texture_upload', [
            handle('texture'),
            podref('gpu_texture_upload', 'region'),
            buf('uint8_t', 'data', 'dataCount'),
        ]),
        op('texture_release', [handle('texture')]),
        op('sampler_new', [podref('gpu_sampler_desc', 'desc')], ret='u32'),
        op('sampler_release', [handle('sampler')]),
        op('texture_view_new', [
            handle('texture'),
            podref('gpu_texture_view_desc', 'desc'),
        ], ret='u32'),
        op('texture_view_release', [handle('view')]),
        # Which RSTB variant the replay backend consumes; the module mirrors
        # it so entry selection matches the host.
        op('shader_target', ret='u32'),
        # Copies the named ShaderAsset's RSTB container; returns the full
        # length for the retrying copy-out convention, 0 when absent.
        op('shader_asset_bytes', [
            handle('object'),
            string('name', 'nameLength'),
            mutbuf('uint8_t', 'out', 'outCount'),
        ], ret='u32'),
        # The asset's core id; re-decoded module-side assets lose it, and it
        # rides the recorded shader module descriptor.
        op('shader_asset_id', [
            handle('object'),
            string('name', 'nameLength'),
        ], ret='u32'),
        op('shader_module_new', [
            podref('gpu_shader_module_desc', 'desc'),
            buf('uint8_t', 'blob', 'blobCount'),
        ], ret='u32'),
        op('shader_module_release', [handle('shaderModule')]),
        op('bind_group_layout_new', [
            u32('groupIndex'),
            buf('rive_gpu_bind_group_layout_entry_v1', 'entries',
                'entryByteCount'),
        ], ret='u32'),
        op('bind_group_layout_release', [handle('layout')]),
        # Host derives the group's entries from the shader's binding map;
        # dynamicUBOs lists @binding values whose UBOs take dynamic offsets.
        op('bind_group_layout_from_shader', [
            handle('shaderModule'),
            u32('groupIndex'),
            buf('uint32_t', 'dynamicUBOs', 'dynamicUBOCount'),
        ], ret='u32'),
        op('bind_group_new', [
            handle('layout'),
            buf('rive_gpu_bind_group_ubo_v1', 'ubos', 'uboByteCount'),
            buf('rive_gpu_bind_group_texture_v1', 'textures',
                'textureByteCount'),
            buf('rive_gpu_bind_group_sampler_v1', 'samplers',
                'samplerByteCount'),
        ], ret='u32'),
        op('bind_group_release', [handle('bindGroup')]),
        op('pipeline_new', [
            podref('gpu_pipeline_desc', 'desc'),
            buf('uint8_t', 'blob', 'blobCount'),
        ], ret='u32'),
        op('pipeline_release', [handle('pipeline')]),
    ]),
    ns('rive_buffer_v1', 'buffer', [
        # type and flags are the RenderBufferType/RenderBufferFlags enums.
        op('new', [u32('bufferType'), u32('flags'), u32('sizeInBytes')],
           ret='u32'),
        op('update', [handle('buffer'),
                      buf('uint8_t', 'bytes', 'byteCount')]),
        op('release', [handle('buffer')]),
    ]),
    ns('rive_blob_v1', 'blob', [
        # Resolves a non-empty BlobAsset by name from the object's file,
        # the context:blob surface. Returns the full byte count; callers
        # retry with a larger buffer when it exceeds capacity.
        op('asset_bytes', [
            handle('object'),
            string('name', 'nameLength'),
            mutbuf('uint8_t', 'out', 'outCount'),
        ], ret='u32'),
    ]),
    ns('rive_image_v1', 'image', [
        # Resolves an ImageAsset by name from the object's file, pinning its
        # decoded RenderImage.
        op('from_asset', [handle('object'), string('name', 'length')],
           ret='u32'),
        op('width', [handle('image')], ret='u32'),
        op('height', [handle('image')], ret='u32'),
        op('release', [handle('image')]),
        # Starts a host-side async decode of encoded image bytes for
        # context:decodeImage. Completion lands on a later advance through
        # the module's host_image_decoded / host_image_decode_failed
        # exports, keyed by the module-issued token. Returns 0 when the
        # host build carries no decoders.
        op('decode', [buf('uint8_t', 'bytes', 'byteCount'), u32('token')],
           ret='u32', stub='hook'),
        op('decode_cancel', [u32('token')]),
    ]),
    ns('rive_shader_v1', 'shader', [
        op('linear', [
            f32('sx'),
            f32('sy'),
            f32('ex'),
            f32('ey'),
            addr('uint32_t', 'colors'),
            addr('float', 'stops'),
            u32('count'),
        ], ret='u32'),
        op('radial', [
            f32('cx'),
            f32('cy'),
            f32('radius'),
            addr('uint32_t', 'colors'),
            addr('float', 'stops'),
            u32('count'),
        ], ret='u32'),
        op('release', [handle('shader')]),
    ]),
    ns('rive_renderer_v1', 'renderer', [
        op('save', [handle('renderer')]),
        op('restore', [handle('renderer')]),
        op('transform', [
            handle('renderer'),
            f32('xx'),
            f32('xy'),
            f32('yx'),
            f32('yy'),
            f32('tx'),
            f32('ty'),
        ]),
        op('draw_path', [
            handle('renderer'),
            handle('path'),
            handle('paint'),
        ]),
        op('clip_path', [handle('renderer'), handle('path')]),
        # sampler is ImageSampler::asKey(); blend is the BlendMode enum.
        op('draw_image', [
            handle('renderer'),
            handle('image'),
            u32('sampler'),
            u32('blend'),
            f32('opacity'),
        ]),
        # Counts derive from the buffer sizes host side; more than seven
        # integer args would spill to the stack, which WAMR's generic native
        # invocation drops.
        op('draw_image_mesh', [
            handle('renderer'),
            handle('image'),
            u32('sampler'),
            handle('vertexBuffer'),
            handle('uvBuffer'),
            handle('indexBuffer'),
            u32('blend'),
            f32('opacity'),
        ]),
    ]),
]
