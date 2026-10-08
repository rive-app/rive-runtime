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


# Wide counters such as PCM frame clocks, which outgrow u32 within a day.
def f64(name):
    return {'kind': 'f64', 'name': name}


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
    # The Text builtin's box settings, one struct so the op stays inside the
    # argument budget. align 3 and 4 are start and end, direction 0 detects.
    pod('text_layout_desc', [
        u32('sizing'),
        u32('overflow'),
        u32('align'),
        u32('wrap'),
        u32('wordBreak'),
        u32('origin'),
        u32('direction'),
        f32('maxWidth'),
        f32('maxHeight'),
        f32('paragraphSpacing'),
    ]),
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
        u32('samplerNonFiltering'),
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
        u32('labelSize'),
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


def op(name, params=(), ret=None, stub=None, guard=None, boot=None,
       web_js=None):
    # guard: the host only carries the op when this macro is defined. Modules
    # import what they call, so one that never calls it links anywhere.
    # boot: the op traps and module start can reach it, before the exec env
    # carries the vm, so the WAMR wrapper resolves the booting one. With no
    # vm at all it traps with this text.
    # web_js: the page answers the op itself; the expression goes straight
    # into the web import object, so the call never enters librive.
    return {'name': name, 'params': list(params), 'ret': ret, 'stub': stub,
            'guard': guard, 'boot': boot, 'web_js': web_js}


def ns(module, short, ops):
    return {'module': module, 'short': short, 'ops': ops}


NAMESPACES = [
    ns('rive_rt_v1', 'rt', [
        op('log', [i32('level'), string('message', 'length')], stub='hook'),
        op('mark_needs_update', [handle('object')]),
        # Raised by the module's fuel checks when the execution budget
        # passes; the host prints the timeout and traps the op.
        op('budget_exceeded', [u32('ms')], boot='execution exceeded timeout'),
        # A script's throw hands over its message just before it traps, so
        # the trap reports what was thrown.
        op('error', [string('message', 'length')], boot='script threw'),
        # Milliseconds. now is monotonic and times the execution budget and
        # os.clock; date_now is the wall clock since the epoch, for os.time.
        op('now', ret='f64', web_js='performance.now()'),
        op('date_now', ret='f64', web_js='Date.now()'),
        # Line probes a debug bake plants: function entry and exit keep a
        # shadow call stack, and each line asks whether to stop. line
        # returns nonzero when it did, so the module re-arms its budget.
        op('debug_enter', [u32('func'), u32('line')]),
        op('debug_line', [u32('line')], ret='u32', stub='zero'),
        op('debug_leave'),
        # os.date local time at an epoch: the host's UTC offset in seconds,
        # signed, crossing as i32 bits; whether daylight saving is in
        # effect; the zone name under the retrying length contract.
        op('utc_offset', [f64('epochSeconds')], ret='u32', stub='zero'),
        op('is_dst', [f64('epochSeconds')], ret='u32', stub='zero'),
        op('zone_name', [f64('epochSeconds'),
                         mutbuf('char', 'buffer', 'capacity')],
           ret='u32', stub='zero'),
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
        # Artboard properties; 0 on hosts built without fetch() support,
        # like the Luau lane's getArtboard.
        op('vmi_artboard', [handle('vmi'), string('name', 'length')],
           ret='u32'),
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
        # The bindable artboard the property holds, as a rive_file_v1
        # bindable handle; 0 while it names one of the host file's own
        # artboards, or none.
        op('artboard_get', [handle('property')], ret='u32'),
        # Binds the bindable's own view model instance and then the
        # artboard, as the Luau lane's setter does; 0 clears both.
        op('artboard_set', [handle('property'), handle('bindable')]),
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
        # ~0 while the value matches the last string_get on this handle,
        # else its byte length, so a module can keep its copy.
        op('string_changed', [handle('property')], ret='u32'),
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
        # phase is ScrollPhase; returns the HitResult like pointer_event.
        op('scroll_event', [
            handle('artboard'),
            u32('pointerId'),
            f32('x'),
            f32('y'),
            f32('dx'),
            f32('dy'),
            u32('phase'),
            u32('precise'),
            f32('timeStamp'),
        ], ret='u32'),
        # artboard:gamepad*: a GamepadWire payload with its button and axis
        # floats; returns how many scripted drawables took the event.
        op('gamepad_event', [handle('artboard'),
                             buf('uint8_t', 'payload', 'byteCount')],
           ret='u32'),
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
        # artboard:addToPath: appends the visible shapes into a host path.
        op('add_to_path', [handle('artboard'), handle('path'), f32('xx'),
                           f32('xy'), f32('yx'), f32('yy'), f32('tx'),
                           f32('ty')]),
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
        # Retrying copy-out of the node's transform children as new node
        # handles; handles mint only when they all fit.
        op('node_children', [handle('node'),
                             mutbuf('uint32_t', 'out', 'outCount')],
           ret='u32'),
        # The parent as a new node handle, 0 when it is not a transform.
        op('node_parent', [handle('node')], ret='u32'),
        # The key custom properties are read by, ~0u when no property in the
        # file carries that name. Resolve once, it scans the name table.
        op('property_key', [handle('artboard'), string('name', 'length')],
           ret='u32'),
        # draw with a visitor: the host calls the module's host_draw_visit
        # export for each drawable that has custom properties. The drawable
        # handle is borrowed and goes stale when the export returns.
        op('draw_visit', [handle('artboard'), handle('renderer')]),
        # The host sets the color modulation per tagged drawable from its
        # property of this key, with no callback into the module.
        op('draw_modulated', [handle('artboard'), handle('renderer'),
                              u32('key')]),
        op('drawable_draw', [handle('drawable'), handle('renderer')]),
        # Returns the CustomPropertyKind plus one, 0 when absent; out[0]
        # receives the value bits of a number, boolean or color.
        op('drawable_value', [handle('drawable'), u32('key'),
                              mutbuf('uint32_t', 'out', 'outCount')],
           ret='u32'),
        # Retrying copy-out; ~0u means no string property by that key.
        op('drawable_string', [handle('drawable'), u32('key'),
                               mutbuf('char', 'out', 'outCount')],
           ret='u32'),
        # Retrying copy-out of one kind byte, the name and a NUL per
        # property.
        op('drawable_properties', [handle('drawable'),
                                   mutbuf('char', 'out', 'outCount')],
           ret='u32', guard='WITH_RIVE_TOOLS'),
    ]),
    # The Luau Audio surface: sources resolve from the object's file by
    # asset name, sounds come back from the play ops. Frame clocks cross as
    # f64 since the engine's run past u32; fades are frames, as the Luau
    # lane passes them.
    ns('rive_audio_v1', 'audio', [
        op('source', [handle('object'), string('name', 'nameLength')],
           ret='u32'),
        op('source_release', [handle('source')]),
        op('source_duration', [handle('source')], ret='f32'),
        op('source_sample_rate', [handle('source')], ret='u32'),
        op('source_channels', [handle('source')], ret='u32'),
        # Each returns a new sound handle, 0 without an engine or source.
        op('play', [handle('source')], ret='u32'),
        op('play_at_time', [handle('source'), f32('seconds')], ret='u32'),
        op('play_in_time', [handle('source'), f32('seconds')], ret='u32'),
        op('play_at_frame', [handle('source'), f64('frame')], ret='u32'),
        op('play_in_frame', [handle('source'), f64('frame')], ret='u32'),
        op('time', ret='f32'),
        op('time_frame', ret='f64'),
        op('sample_rate', ret='u32'),
        op('sound_release', [handle('sound')]),
        op('sound_play', [handle('sound')]),
        op('sound_pause', [handle('sound')]),
        op('sound_resume', [handle('sound')]),
        op('sound_stop', [handle('sound'), u32('fadeFrames')]),
        op('sound_seek', [handle('sound'), f32('seconds')], ret='u32'),
        op('sound_seek_frame', [handle('sound'), f64('frame')], ret='u32'),
        op('sound_completed', [handle('sound')], ret='u32'),
        op('sound_time', [handle('sound')], ret='f32'),
        op('sound_time_frame', [handle('sound')], ret='f64'),
        op('sound_volume', [handle('sound')], ret='f32'),
        op('sound_set_volume', [handle('sound'), f32('value')]),
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
        # Appends other's host geometry through the affine values, so a path
        # built from many paths never copies points inside the module.
        op('add', [handle('path'), handle('other'), f32('xx'), f32('xy'),
                   f32('yx'), f32('yy'), f32('tx'), f32('ty')]),
        # Host composed geometry read back, size then fill.
        op('verbs', [handle('path'), mutbuf('uint8_t', 'out', 'outCount')],
           ret='u32'),
        op('points', [handle('path'), mutbuf('float', 'out', 'outCount')],
           ret='u32'),
        # The geometry the script's effect update produced, appended into the
        # RawPath the pending callPathEffectUpdate points at; the inverse of
        # update's module-to-host crossing.
        op('effect_result', [
            buf('uint8_t', 'verbs', 'verbCount'),
            buf('float', 'points', 'floatCount'),
        ]),
    ]),
    # Path measurement over module geometry: a whole path measure or a
    # contour iterator, both copying the geometry so the module's path can
    # change underneath. Contour handles come from contour_next and answer
    # the same ops as a path measure.
    ns('rive_measure_v1', 'measure', [
        op('path_new', [
            buf('uint8_t', 'verbs', 'verbCount'),
            buf('float', 'points', 'floatCount'),
        ], ret='u32'),
        op('contours_new', [
            buf('uint8_t', 'verbs', 'verbCount'),
            buf('float', 'points', 'floatCount'),
        ], ret='u32'),
        # The next contour of an iterator handle, 0 past the last.
        op('contour_next', [handle('measure')], ret='u32'),
        op('length', [handle('measure')], ret='f32'),
        op('is_closed', [handle('measure')], ret='u32'),
        # pos.x, pos.y, tan.x, tan.y
        op('pos_tan', [handle('measure'), f32('distance'),
                       mutbuf('float', 'out', 'outCount')]),
        op('warp', [handle('measure'), f32('x'), f32('y'),
                    mutbuf('float', 'out', 'outCount')]),
        # Computes the segment host side and returns its verb count;
        # extract_read copies it out (at most three points per verb).
        op('extract', [handle('measure'), f32('startDistance'),
                       f32('endDistance'), u32('startWithMove')], ret='u32'),
        op('extract_read', [
            handle('measure'),
            mutbuf('uint8_t', 'verbs', 'verbCount'),
            mutbuf('float', 'points', 'floatCount'),
        ], ret='u32'),
        op('release', [handle('measure')]),
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
        op('shader_transform', [
            handle('paint'),
            buf('float', 'values', 'floatCount'),
        ]),
    ]),
    # 2D canvas: an offscreen Rive render target the script draws into with
    # a frame's Renderer and composites with the image it mints. A zero size
    # allocates nothing until resize, and a size requested before the device
    # binds waits for it. Frames record through the deferred canvas host, so
    # begin_frame answers 0 without one.
    ns('rive_canvas_v1', 'canvas', [
        op('new', [u32('width'), u32('height')], ret='u32'),
        op('release', [handle('canvas')]),
        op('width', [handle('canvas')], ret='u32'),
        op('height', [handle('canvas')], ret='u32'),
        # 1 on success; a zero dimension drops the backing.
        op('resize', [handle('canvas'), u32('width'), u32('height')],
           ret='u32'),
        # Mints an image handle over the canvas's presentable RenderImage.
        op('image', [handle('canvas')], ret='u32'),
        # Mints the frame's renderer handle; clearColor is ARGB. end_frame
        # releases it, so a stashed wrapper goes stale.
        op('begin_frame', [handle('canvas'), u32('clearColor')], ret='u32'),
        op('end_frame', [handle('canvas')]),
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
        # A view over the host render target, which renders under Rive's
        # content; props mirror canvas_color_view. Returns current while it
        # still names the target, 0 when the host exposes none.
        op('target_view', [
            u32('current'),
            mutbuf('uint32_t', 'props', 'propCount'),
        ], ret='u32'),
        # The blob is colorCount color attachments, then labelSize bytes of
        # label.
        op('pass_begin', [
            podref('gpu_pass_desc', 'desc'),
            buf('uint8_t', 'blob', 'blobCount'),
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
        # Copies the named ShaderAsset's RSTB container, which modules read
        # only for its entry table; returns the full length for the retrying
        # copy-out convention, 0 when absent.
        op('shader_asset_bytes', [
            handle('object'),
            string('name', 'nameLength'),
            mutbuf('uint8_t', 'out', 'outCount'),
        ], ret='u32'),
        # The asset's core id; re-decoded module-side assets lose it.
        op('shader_asset_id', [
            handle('object'),
            string('name', 'nameLength'),
        ], ret='u32'),
        # The host builds the module from the named ShaderAsset, so no
        # module bytes reach a shader compiler. entry indexes the asset's
        # entry container; a whole module target's module serves them all.
        op('shader_module_from_asset', [
            handle('object'),
            string('name', 'nameLength'),
            u32('entry'),
        ], ret='u32'),
        # Raw module bytes skip the shader signature check, so only edit
        # time hosts link it.
        op('shader_module_new', [
            podref('gpu_shader_module_desc', 'desc'),
            buf('uint8_t', 'blob', 'blobCount'),
        ], ret='u32', guard='WITH_RIVE_TOOLS'),
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
        # The same over a pipeline whose stages live in two modules, so the
        # group covers what either stage binds.
        op('bind_group_layout_from_shaders', [
            handle('vertexModule'),
            handle('fragmentModule'),
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
    # Column major 4x4 products the interpreter lanes would otherwise run
    # op by op; out may alias an input.
    ns('rive_mat4_v1', 'mat4', [
        # 64 byte column-major matrices as byte buffers, so WAMR bounds
        # checks all of each, even while the module is starting.
        op('multiply', [mutbuf('uint8_t', 'out', 'outBytes'),
                        buf('uint8_t', 'a', 'aBytes'),
                        buf('uint8_t', 'b', 'bBytes')]),
        op('invert', [mutbuf('uint8_t', 'out', 'outBytes'),
                      buf('uint8_t', 'src', 'srcBytes')],
           ret='u32', stub='zero'),
    ]),
    ns('rive_buffer_v1', 'buffer', [
        # type and flags are the RenderBufferType/RenderBufferFlags enums.
        op('new', [u32('bufferType'), u32('flags'), u32('sizeInBytes')],
           ret='u32'),
        op('update', [handle('buffer'),
                      buf('uint8_t', 'bytes', 'byteCount')]),
        op('release', [handle('buffer')]),
    ]),
    # Instances of one mesh, drawn in a single call. Unlike a buffer these
    # are meant to be rewritten every frame, so the handle outlives an update
    # and the host resizes in place.
    ns('rive_mesh_instances_v1', 'mesh_instances', [
        op('new', [u32('count')], ret='u32'),
        op('resize', [handle('mesh_instances', 'instances'), u32('count')]),
        op('update', [
            handle('mesh_instances', 'instances'),
            u32('first'),
            buf('uint8_t', 'bytes', 'byteCount'),
        ]),
        op('release', [handle('mesh_instances', 'instances')]),
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
    # The Luau Font builtin: metrics, variable instances and glyph outlines.
    ns('rive_font_v1', 'font', [
        op('from_asset', [handle('object'), string('name', 'length')],
           ret='u32'),
        op('decode', [buf('uint8_t', 'bytes', 'byteCount')], ret='u32', guard='WITH_RIVE_TEXT'),
        op('release', [handle('font')]),
        # ascent, descent, capHeight, xHeight for a 1 point font.
        op('metrics', [handle('font'), mutbuf('float', 'out', 'outCount')]),
        op('weight', [handle('font')], ret='u32'),
        op('is_italic', [handle('font')], ret='u32'),
        op('axis_count', [handle('font')], ret='u32'),
        # tag bits, min, default, max
        op('axis', [handle('font'), u32('index'),
                    mutbuf('float', 'out', 'outCount')]),
        op('axis_value', [handle('font'), u32('tag')], ret='f32'),
        # Feature tags; returns the count whatever the capacity.
        op('features', [handle('font'),
                        mutbuf('uint32_t', 'out', 'outCount')], ret='u32'),
        op('has_glyph', [handle('font'), u32('codepoint')], ret='u32'),
        # Axis coords and features as (tag, value bits) pairs.
        op('with_options', [handle('font'),
                            buf('uint32_t', 'coords', 'coordCount'),
                            buf('uint32_t', 'features', 'featureCount')],
           ret='u32'),
        # A glyph outline wound for a clockwise fill, size then fill.
        op('glyph_verbs', [handle('font'), u32('glyph'),
                           mutbuf('uint8_t', 'out', 'outCount')], ret='u32',
           guard='WITH_RIVE_TEXT'),
        op('glyph_points', [handle('font'), u32('glyph'),
                            mutbuf('float', 'out', 'outCount')], ret='u32',
           guard='WITH_RIVE_TEXT'),
    ]),
    # The Luau Text builtin: styled runs laid out by the runtime shaper, drawn
    # whole or read back as lines of glyphs in visual order.
    ns('rive_text_v1', 'text', [
        op('new', ret='u32', guard='WITH_RIVE_TEXT'),
        op('release', [handle('text')], guard='WITH_RIVE_TEXT'),
        # paint 0 draws nothing for the run unless draw is given a paint.
        op('append', [handle('text'), string('chars', 'length'),
                      handle('font'), handle('paint'), f32('size'),
                      f32('lineHeight'), f32('letterSpacing'),
                      u32('foreground')], guard='WITH_RIVE_TEXT'),
        op('clear', [handle('text')], guard='WITH_RIVE_TEXT'),
        op('layout', [handle('text'), podref('text_layout_desc', 'desc')], guard='WITH_RIVE_TEXT'),
        op('draw', [handle('text'), handle('renderer'), handle('paint')], guard='WITH_RIVE_TEXT'),
        # minX, minY, maxX, maxY
        op('bounds', [handle('text'), mutbuf('float', 'out', 'outCount')], guard='WITH_RIVE_TEXT'),
        op('length', [handle('text')], ret='u32', guard='WITH_RIVE_TEXT'),
        # Flat four byte word records, size then fill; text.as names the
        # words. Glyphs come line by line in visual order.
        op('lines', [handle('text'), mutbuf('uint32_t', 'out', 'outCount')],
           ret='u32', guard='WITH_RIVE_TEXT'),
        op('runs', [handle('text'), mutbuf('uint32_t', 'out', 'outCount')],
           ret='u32', guard='WITH_RIVE_TEXT'),
        op('glyphs', [handle('text'), mutbuf('uint32_t', 'out', 'outCount')],
           ret='u32', guard='WITH_RIVE_TEXT'),
        op('hit_test', [handle('text'), f32('x'), f32('y')], ret='u32', guard='WITH_RIVE_TEXT'),
        # x, top, bottom; returns 0 when the text has no layout.
        op('caret', [handle('text'), u32('index'),
                     mutbuf('float', 'out', 'outCount')], ret='u32', guard='WITH_RIVE_TEXT'),
        # minX, minY, maxX, maxY per rect; returns the float count.
        op('selection_rects', [handle('text'), u32('from'), u32('to'),
                               mutbuf('float', 'out', 'outCount')],
           ret='u32', guard='WITH_RIVE_TEXT'),
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
    # Tests suites' blob(name): the full byte count, size then fill, 0 when
    # the blob is missing or empty; outside test runs every blob is missing.
    ns('rive_test_v1', 'test', [
        op('blob', [string('name', 'nameLength'),
                    mutbuf('uint8_t', 'out', 'outCount')], ret='u32',
           stub='zero'),
    ]),
    # The children a Transition's changed and draw receive, valid for that
    # call only.
    ns('rive_transition_v1', 'transition', [
        op('child_draw', [handle('child'), handle('renderer')]),
        op('child_width', [handle('child')], ret='f32'),
        op('child_height', [handle('child')], ret='f32'),
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
        op('modulate_opacity', [handle('renderer'), f32('opacity')]),
        # color is packed ARGB; replace sets it instead of multiplying.
        op('modulate_color', [handle('renderer'), u32('color'),
                              u32('replace')]),
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
        # Blend is always srcOver and opacity is per instance, which is what
        # buys the room to pass the instances; this sits exactly on the seven
        # integer ceiling, so the data cannot also cross inline as a buf.
        op('draw_image_mesh_instanced', [
            handle('renderer'),
            handle('image'),
            u32('sampler'),
            handle('vertexBuffer'),
            handle('uvBuffer'),
            handle('indexBuffer'),
            handle('instances'),
        ]),
    ]),
    # fetch(): the host runs NetPolicy, the limits and the transport. The
    # request is a NetWire stream; the outcome lands on a later advance
    # through the module's host_fetch_resolved / host_fetch_failed exports,
    # keyed by the module-issued token. fetch returns 0 when the host was
    # built without fetch() support and nothing will ever settle, or when
    # the token is already in flight.
    ns('rive_net_v1', 'net', [
        op('fetch', [buf('uint8_t', 'request', 'requestCount'),
                     u32('token')], ret='u32'),
        op('fetch_cancel', [u32('token')]),
    ]),
    # context:decodeFile and what it returns: decoded files and their
    # bindable artboards are host objects behind handles.
    ns('rive_file_v1', 'file', [
        # 0 when no file results, with statusOut[0] set to the
        # FileDecodeWire status saying why.
        op('decode', [buf('uint8_t', 'bytes', 'byteCount'),
                      mutbuf('uint32_t', 'statusOut', 'statusCount')],
           ret='u32'),
        op('release', [handle('file')]),
        # The artboard count, then each name by index with the retrying
        # length contract; names are file data and may hold any character.
        op('artboard_count', [handle('file')], ret='u32'),
        op('artboard_name', [handle('file'), u32('index'),
                             mutbuf('char', 'buffer', 'capacity')],
           ret='u32'),
        # The named artboard, or the file's default one when useDefault is
        # set, with an instance of its own view model; 0 when absent.
        op('bindable', [handle('file'), string('name', 'nameLength'),
                        u32('useDefault')], ret='u32'),
        op('bindable_release', [handle('bindable')]),
        op('bindable_name', [handle('bindable'),
                             mutbuf('char', 'buffer', 'capacity')],
           ret='u32'),
        # The instance of the artboard's own view model as a rive_data_v1
        # vmi handle; 0 when the artboard has none.
        op('bindable_data', [handle('bindable')], ret='u32'),
        # Both handles carry the same bindable artboard.
        op('bindable_equal', [handle('a'), handle('b')], ret='u32'),
    ]),
]

# Leaf ops never call back into a module (no listeners, no nested scripts),
# so the interpreter runs them without a frame of its own. Setters, triggers
# and list edits notify listeners synchronously and must stay out.
ALL_OPS = None
LEAF_OPS = {
    'rive_data_v1': {
        'vmi_number', 'vmi_boolean', 'vmi_string', 'vmi_trigger', 'vmi_color',
        'vmi_view_model', 'vmi_list', 'vmi_enum', 'vmi_image', 'vmi_font',
        'vmi_blob', 'number_get', 'boolean_get', 'string_get',
        'string_changed', 'color_get', 'enum_get', 'list_length', 'list_get',
        'view_model_get',
    },
    'rive_artboard_v1': {
        'width', 'height', 'bounds', 'node_transform', 'node_world_transform',
    },
    'rive_gpu_v1': {
        'pass_set_pipeline', 'pass_set_vertex_buffer', 'pass_set_index_buffer',
        'pass_set_bind_group', 'pass_set_viewport', 'pass_set_scissor',
        'pass_set_stencil_reference', 'pass_set_blend_color', 'pass_draw',
        'pass_draw_indexed', 'buffer_update',
    },
    'rive_mat4_v1': ALL_OPS,
    'rive_path_v1': ALL_OPS,
    'rive_measure_v1': ALL_OPS,
    'rive_paint_v1': ALL_OPS,
    'rive_shader_v1': ALL_OPS,
    'rive_buffer_v1': ALL_OPS,
    'rive_renderer_v1': ALL_OPS,
}

# A misspelled module would silently mark nothing leaf.
assert set(LEAF_OPS) <= {n['module'] for n in NAMESPACES}, (
    set(LEAF_OPS) - {n['module'] for n in NAMESPACES})

for _namespace in NAMESPACES:
    _names = {o['name'] for o in _namespace['ops']}
    _leaf = LEAF_OPS.get(_namespace['module'], set())
    _leaf = _names if _leaf is ALL_OPS else _leaf
    assert _leaf <= _names, (_namespace['module'], _leaf - _names)
    for _op in _namespace['ops']:
        _op['leaf'] = _op['name'] in _leaf
