## Runs minify.py on the whole batch of .glsl files in this folder.
##
## Premake can't do this build step because it has multiple inputs AND multiple outputs.
##
## The files have to be processed in batch in to ensure consistent renaming.
OUT := out/generated
FLAGS :=

## Shader minification.
MINIFY_INPUTS := $(wildcard *.glsl) $(wildcard *.vert) $(wildcard *.frag)
MINIFY_EXPORT_OUTPUTS := $(addprefix $(OUT)/, $(addsuffix .exports.h, $(MINIFY_INPUTS)))
MINIFY_GLSL_OUTPUTS := $(addprefix $(OUT)/,\
                         $(patsubst %.glsl, %.minified.glsl,\
                         $(patsubst %.vert, %.minified.vert,\
                         $(patsubst %.frag, %.minified.frag,\
                           $(MINIFY_INPUTS)))))
MINIFY_HPP_OUTPUTS := $(addprefix $(OUT)/, $(addsuffix .hpp, $(MINIFY_INPUTS)))
MINIFY_OUTPUTS := $(MINIFY_EXPORT_OUTPUTS) $(MINIFY_GLSL_OUTPUTS) $(MINIFY_HPP_OUTPUTS)
MINIFY_STAMP := $(OUT)/glsl.stamp

minify: $(MINIFY_OUTPUTS)

## Using a stamp enables a build step with multiple inputs and multiple outputs.
## https://www.gnu.org/software/automake/manual/html_node/Multiple-Outputs.html
$(MINIFY_OUTPUTS): $(MINIFY_STAMP)
	@test -f $@ || rm -f $(MINIFY_STAMP)
	@test -f $@ || "$(MAKE)" $(AM_MAKEFLAGS) $(MINIFY_STAMP)

$(MINIFY_STAMP): $(MINIFY_INPUTS) minify.py
	python3 minify.py $(FLAGS) -o $(OUT) $(MINIFY_INPUTS)
	@touch $(MINIFY_STAMP)

$(OUT)/.:
	@mkdir -p $@

## Metal shader offline compiling.
$(OUT)/ios/.: | $(OUT)/.
	@mkdir -p $@

$(OUT)/macosx/.: | $(OUT)/.
	@mkdir -p $@

DRAW_COMBINATIONS_METAL := $(OUT)/draw_combinations.metal
METAL_INPUTS := $(wildcard metal/*.metal)
METAL_MACOSX_AIR_OUTPUTS := \
	$(addprefix $(OUT)/, $(patsubst metal/%.metal, macosx/%.air, $(METAL_INPUTS)))
METAL_IOS_AIR_OUTPUTS := $(addprefix $(OUT)/, $(patsubst metal/%.metal, ios/%.air, $(METAL_INPUTS)))

$(DRAW_COMBINATIONS_METAL): metal/generate_draw_combinations.py | $(OUT)/.
	python3 metal/generate_draw_combinations.py $(DRAW_COMBINATIONS_METAL)

rive_pls_macosx_metallib: $(OUT)/rive_pls_macosx.metallib.c
rive_pls_ios_metallib: $(OUT)/rive_pls_ios.metallib.c
rive_pls_ios_simulator_metallib: $(OUT)/rive_pls_ios_simulator.metallib.c
rive_renderer_xros_metallib: $(OUT)/rive_renderer_xros.metallib.c
rive_renderer_xros_simulator_metallib: $(OUT)/rive_renderer_xros_simulator.metallib.c
rive_renderer_appletvos_metallib: $(OUT)/rive_renderer_appletvos.metallib.c
rive_renderer_appletvsimulator_metallib: $(OUT)/rive_renderer_appletvsimulator.metallib.c

## The source files all get regenerated in a batch, so there's no need to separate out separate
## rules for each intermediate file.
$(OUT)/macosx/rive_pls_macosx.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL) | $(OUT)/macosx/.
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk macosx metal -std=macos-metal2.3 \
		-mmacosx-version-min=11.0 \
		-I$(OUT) -ffast-math -ffp-contract=fast -fpreserve-invariance -fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/macosx/%.air, $(FILE));)
	xcrun -sdk macosx metallib $(METAL_MACOSX_AIR_OUTPUTS) -o $(OUT)/macosx/rive_pls_macosx.metallib

$(OUT)/rive_pls_macosx.metallib.c: $(OUT)/macosx/rive_pls_macosx.metallib
	xxd -i -n rive_pls_macosx_metallib \
		$(OUT)/macosx/rive_pls_macosx.metallib \
		$(OUT)/rive_pls_macosx.metallib.c

$(OUT)/ios/rive_pls_ios.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL) | $(OUT)/ios/.
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk iphoneos metal -std=ios-metal2.2 \
		-I$(OUT) -mios-version-min=13 -ffast-math -ffp-contract=fast -fpreserve-invariance \
		-fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/ios/%.air, $(FILE));)
	xcrun -sdk iphoneos metallib $(METAL_IOS_AIR_OUTPUTS) -o $(OUT)/ios/rive_pls_ios.metallib

$(OUT)/rive_pls_ios.metallib.c: $(OUT)/ios/rive_pls_ios.metallib
	xxd -i -n rive_pls_ios_metallib $(OUT)/ios/rive_pls_ios.metallib $(OUT)/rive_pls_ios.metallib.c

$(OUT)/ios/rive_pls_ios_simulator.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL) | $(OUT)/ios/.
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk iphonesimulator metal -std=ios-metal2.2 \
		-I$(OUT) -miphonesimulator-version-min=13 -ffast-math -ffp-contract=fast -fpreserve-invariance \
		-fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/ios/%.air, $(FILE));)
	xcrun -sdk iphonesimulator metallib $(METAL_IOS_AIR_OUTPUTS) -o $(OUT)/ios/rive_pls_ios_simulator.metallib

$(OUT)/rive_pls_ios_simulator.metallib.c: $(OUT)/ios/rive_pls_ios_simulator.metallib
	xxd -i -n rive_pls_ios_simulator_metallib $(OUT)/ios/rive_pls_ios_simulator.metallib $(OUT)/rive_pls_ios_simulator.metallib.c

$(OUT)/ios/rive_renderer_xros.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL) | $(OUT)/ios/.
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk xros metal -std=metal3.1 \
		-I$(OUT) --target=air64-apple-xros1.0 -ffast-math -ffp-contract=fast -fpreserve-invariance \
		-fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/ios/%.air, $(FILE));)
	xcrun -sdk xros metallib $(METAL_IOS_AIR_OUTPUTS) -o $(OUT)/ios/rive_renderer_xros.metallib

$(OUT)/rive_renderer_xros.metallib.c: $(OUT)/ios/rive_renderer_xros.metallib
	xxd -i -n rive_renderer_xros_metallib $(OUT)/ios/rive_renderer_xros.metallib $(OUT)/rive_renderer_xros.metallib.c

$(OUT)/ios/rive_renderer_xros_simulator.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL) | $(OUT)/ios/.
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk xrsimulator metal -std=metal3.1 \
		-I$(OUT) --target=air64-apple-xros1.0-simulator -ffast-math -ffp-contract=fast -fpreserve-invariance \
		-fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/ios/%.air, $(FILE));)
	xcrun -sdk xrsimulator metallib $(METAL_IOS_AIR_OUTPUTS) -o $(OUT)/ios/rive_renderer_xros_simulator.metallib

$(OUT)/rive_renderer_xros_simulator.metallib.c: $(OUT)/ios/rive_renderer_xros_simulator.metallib
	xxd -i -n rive_renderer_xros_simulator_metallib $(OUT)/ios/rive_renderer_xros_simulator.metallib $(OUT)/rive_renderer_xros_simulator.metallib.c

$(OUT)/ios/rive_renderer_appletvos.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL) | $(OUT)/ios/.
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk appletvos metal -std=metal3.0 \
		-I$(OUT) -mappletvos-version-min=16.0 -ffast-math -ffp-contract=fast -fpreserve-invariance \
		-fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/ios/%.air, $(FILE));)
	xcrun -sdk appletvos metallib $(METAL_IOS_AIR_OUTPUTS) -o $(OUT)/ios/rive_renderer_appletvos.metallib

$(OUT)/rive_renderer_appletvos.metallib.c: $(OUT)/ios/rive_renderer_appletvos.metallib
	xxd -i -n rive_renderer_appletvos_metallib $(OUT)/ios/rive_renderer_appletvos.metallib $(OUT)/rive_renderer_appletvos.metallib.c

$(OUT)/ios/rive_renderer_appletvsimulator.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL) | $(OUT)/ios/.
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk appletvsimulator metal -std=metal3.0 \
		-I$(OUT) -mappletvsimulator-version-min=16.0 -ffast-math -ffp-contract=fast -fpreserve-invariance \
		-fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/ios/%.air, $(FILE));)
	xcrun -sdk appletvsimulator metallib $(METAL_IOS_AIR_OUTPUTS) -o $(OUT)/ios/rive_renderer_appletvsimulator.metallib

$(OUT)/rive_renderer_appletvsimulator.metallib.c: $(OUT)/ios/rive_renderer_appletvsimulator.metallib
	xxd -i -n rive_renderer_appletvsimulator_metallib $(OUT)/ios/rive_renderer_appletvsimulator.metallib $(OUT)/rive_renderer_appletvsimulator.metallib.c


## SPIRV compilation.

$(OUT)/spirv/.: | $(OUT)/.
	@mkdir -p $@

# All glsl source files that need to be built
SPIRV_STANDARD_INPUTS := $(wildcard spirv/*.main) \
				 $(wildcard spirv/*.vert) \
				 $(wildcard spirv/*.frag)

# WebGPU needs some additional shaders to be built specifically for it,
# to ensure the proper sampler binding set is used.
SPIRV_WEBGPU_INPUTS := \
    spirv/tessellate.main \
	spirv/render_atlas.vert \
	spirv/render_atlas_fill.frag \
	spirv/render_atlas_stroke.frag \
	spirv/blit_texture_as_draw_filtered.main \
    spirv/atomic_resolve_coalesced.main \

# Clockwise shaders need separate builds with FIXED_FUNCTION_COLOR_OUTPUT
# defined.
SPIRV_CLOCKWISE_INPUTS := \
    spirv/draw_clockwise_path.main \
    spirv/draw_clockwise_clip.main \
    spirv/draw_clockwise_interior_triangles.main \
    spirv/draw_clockwise_clip_interior_triangles.main \
    spirv/draw_clockwise_atlas_blit.main \
    spirv/draw_clockwise_image_mesh.main \
    spirv/draw_clockwise_atomic_path.main \
    spirv/draw_clockwise_atomic_interior_triangles.main \
    spirv/draw_clockwise_atomic_atlas_blit.main \
    spirv/draw_clockwise_atomic_image_mesh.main \
    spirv/init_clockwise_atomic_workaround.frag \
    spirv/draw_clockwise_atomic_clip.frag \
    spirv/draw_clockwise_atomic_clip_interior_triangles.frag \
    spirv/clear_clockwise_atomic_clip.main \

# Atomic shaders need separate builds with FIXED_FUNCTION_COLOR_OUTPUT, as well
# as builds for WebGPU.
SPIRV_DRAW_ATOMIC_INPUTS := \
    spirv/atomic_draw_image_mesh.main \
    spirv/atomic_draw_image_rect.main \
    spirv/atomic_draw_interior_triangles.main \
    spirv/atomic_draw_atlas_blit.main \
    spirv/atomic_draw_path.main \
    spirv/atomic_resolve.main \
    spirv/atomic_init.main \

# MSAA shaders need separate builds with FIXED_FUNCTION_COLOR_OUTPUT and/or
# DISABLE_CLIP_DISTANCE_FOR_UBERSHADERS, as well as builds for WebGPU.
SPIRV_DRAW_MSAA_INPUTS := \
    spirv/draw_msaa_atlas_blit.main \
    spirv/draw_msaa_image_mesh.main \
    spirv/draw_msaa_path.main \
    spirv/draw_msaa_stencil.main \


## Helpers for use in SPIRV_LIST_RULE (using lower_snake_case to distinguish things that use inputs like $1, $2, etc)
spirv_typed_filename = $(basename $1).$2
spirv_out_filename_no_ext = $(OUT)/$(call spirv_typed_filename,$1,$2)
spirv_type = $(lastword $(subst _, ,$2))
spirv_is_vert = $(findstring vert,$(spirv_type))
spirv_is_webgpu = $(findstring webgpu,$2)
spirv_is_atomic_or_clockwise_atomic = $(findstring atomic,$1)
spirv_is_not_clockwise_atomic = $(if $(findstring clockwise_atomic,$1),,1)
spirv_is_clockwise = $(and $(findstring clockwise,$1), $(call spirv_is_not_clockwise_atomic,$1))

## SPIR-V Optimizer settings

## To work around a driver bug in Android 9/10-era Adreno 5/6xx driver bugs, we need to run the
## fragment shaders through a preprocess optimization. The important bits for the workaround are:
##   --merge-return
##   --inline-entry-points-exhaustive
## without those, the pipelines on the affected devices will fail to link. However, we can do
## more optimizations while we're here, which will reduce the binary size.

## Vertex shaders can be optimized using the standard "optimize for performance" option
## with no known issues
SPIRV_STANDARD_VERT_OPT_PARAMS = -O

## Fragment shaders are a bit different: This is mostly a copy of the settings that spirv-opt
## reports for "-O" except with the --simplify-instructions option removed, which causes many
## issues on Adreno drivers. Additionally, the following three options cause problems with
## our atomic shaders:
##   --ssa-rewrite
##   --eliminate-local-single-block
##   --eliminate-local-single-store
## So for atomic shaders, we end up with a different, even-more-pared-down set of instructions
## that doesn't drastically grow their sizes while also dodging all of the driver issues.
##
## Also use "-O" for the WebGPU shaders (for dawn) because something about the big list of
## options causes an internal compiler error error in its driver, but -O works great.
##
## TODO: Figure out why these cause issues with the atomic shaders and see if we can fix it
## to get consistent fragment shader optimizations.
spirv_frag_opt_params = \
	$(if $(spirv_is_webgpu),-O, \
		$(if $(spirv_is_atomic_or_clockwise_atomic), \
			--preserve-bindings \
			--preserve-interface \
			--wrap-opkill \
			--simplify-instructions \
			--eliminate-dead-branches \
			--merge-return \
			--inline-entry-points-exhaustive \
			--eliminate-dead-inserts \
			--eliminate-dead-members \
			--merge-blocks \
			--redundancy-elimination \
			--cfg-cleanup \
			--eliminate-dead-const \
			--eliminate-dead-variables \
			--eliminate-dead-functions \
			--eliminate-dead-code-aggressive \
		, \
			--wrap-opkill \
			--eliminate-dead-branches \
			--merge-return \
			--inline-entry-points-exhaustive \
			--eliminate-dead-functions \
			--eliminate-dead-code-aggressive \
			--private-to-local \
			--eliminate-local-single-block \
			--eliminate-local-single-store \
			--eliminate-dead-code-aggressive \
			--scalar-replacement=100 \
			--convert-local-access-chains \
			--eliminate-local-single-block \
			--eliminate-local-single-store \
			--eliminate-dead-code-aggressive \
			--ssa-rewrite \
			--eliminate-dead-code-aggressive \
			--ccp \
			--eliminate-dead-code-aggressive \
			--loop-unroll \
			--eliminate-dead-branches \
			--redundancy-elimination \
			--combine-access-chains \
			--scalar-replacement=100 \
			--convert-local-access-chains \
			--eliminate-local-single-block \
			--eliminate-local-single-store \
			--eliminate-dead-code-aggressive \
			--ssa-rewrite \
			--eliminate-dead-code-aggressive \
			--vector-dce \
			--eliminate-dead-inserts \
			--eliminate-dead-branches \
			--if-conversion \
			--copy-propagate-arrays \
			--reduce-load-size \
			--eliminate-dead-code-aggressive \
			--merge-blocks \
			--redundancy-elimination \
			--eliminate-dead-branches \
			--merge-blocks \
		) \
	)

## WebGPU fragment shaders need a different PLS_IMPL value
spirv_frag_params = $(if $(spirv_is_webgpu), \
                        -DPLS_IMPL_STORAGE_BUFFER, \
					$(if $(spirv_is_clockwise), \
						-DPLS_IMPL_STORAGE_TEXTURE, \
						-DPLS_IMPL_SUBPASS_LOAD))

## Vertex shaders get the standard optimizations.
## Fragment shaders get the adreno workaround options if they're in the workaround list
## or they'll get the "atomic"
spirv_opt_params = \
	$(if $(spirv_is_vert), \
		$(SPIRV_STANDARD_VERT_OPT_PARAMS), \
		$(spirv_frag_opt_params) \
	) \


## The rules/outputs for a given input/output pair
## Usage: $(eval $(call spirv_list_rule, INPUT_FILENAME, OUTPUT_TYPE [, ADDITIONAL_COMPILE_OPTIONS]))
##   Where OUTPUT_TYPE is, say, "vert" or "frag" or "fixedcolor_frag", etc
define spirv_list_rule
  $(spirv_out_filename_no_ext).spv: $1 $(MINIFY_STAMP) | $(OUT)/spirv/.
	@glslangValidator -S $(spirv_type) -DTARGET_SPIRV \
		$(if $(spirv_is_vert), -DVERTEX, -DFRAGMENT $(spirv_frag_params)) \
		-I$(OUT)  -V $3 -o $(spirv_out_filename_no_ext).spv.unoptimized $1
	@spirv-opt --preserve-bindings --preserve-interface $(spirv_opt_params) \
		$(spirv_out_filename_no_ext).spv.unoptimized -o $(spirv_out_filename_no_ext).spv
	@rm $(spirv_out_filename_no_ext).spv.unoptimized

  $(spirv_out_filename_no_ext).h: $(spirv_out_filename_no_ext).spv
	@python3 spirv_binary_to_header.py $(spirv_out_filename_no_ext).spv $(spirv_out_filename_no_ext).h $(subst $(suffix $1),_$2,$(notdir $1))

  SPIRV_OUTPUTS_BINARY += $(spirv_out_filename_no_ext).spv
  SPIRV_OUTPUTS_HEADERS += $(spirv_out_filename_no_ext).h
endef

## Make a set of rules for a given set of files and output types
## Usage: $(eval $(call make_spirv_rules, LIST_OF_INPUT_FILES, LIST_OF_OUTPUT_TYPES [, ADDITIONAL_COMPILE_OPTIONS]))
##   Where LIST_OF_OUTPUT_TYPES can contain one or more of entries like "vert" or "frag" or "fixedcolor_frag"
define make_spirv_rules
    ## Note that the inner foreach will filter out ".frag" files from the list for any vert targets, and vice versa.
    $(foreach type,$2,\
        $(foreach file,$(filter-out %.$(if $(findstring vert, $(type)),frag,vert),$1),\
            $(eval $(call spirv_list_rule,$(file),$(type),$3))\
        )\
    )
endef

## All .main/vert/frag files should build
$(eval $(call make_spirv_rules, $(SPIRV_STANDARD_INPUTS), vert frag))

SPIRV_COMMON_WEBGPU_PARAMS = -DTARGET_WGSL -DUSE_WEBGPU_SAMPLERS

## Each of the specialized SPIRV lists have their own associated rules
$(eval $(call make_spirv_rules, $(SPIRV_DRAW_MSAA_INPUTS), noclipdistance_vert, -DDISABLE_CLIP_DISTANCE_FOR_UBERSHADERS))
$(eval $(call make_spirv_rules, \
    $(SPIRV_CLOCKWISE_INPUTS) \
    $(SPIRV_DRAW_ATOMIC_INPUTS) \
    $(SPIRV_DRAW_MSAA_INPUTS), \
    fixedcolor_frag, \
    -DFIXED_FUNCTION_COLOR_OUTPUT))
$(eval $(call make_spirv_rules, \
    $(SPIRV_WEBGPU_INPUTS) \
    $(SPIRV_DRAW_ATOMIC_INPUTS) \
    $(SPIRV_DRAW_MSAA_INPUTS), \
    webgpu_vert, \
    $(SPIRV_COMMON_WEBGPU_PARAMS)))
$(eval $(call make_spirv_rules, \
    $(SPIRV_DRAW_MSAA_INPUTS), \
    webgpu_noclipdistance_vert, \
    $(SPIRV_COMMON_WEBGPU_PARAMS) -DDISABLE_CLIP_DISTANCE_FOR_UBERSHADERS))
$(eval $(call make_spirv_rules, \
    $(SPIRV_WEBGPU_INPUTS) \
    $(SPIRV_DRAW_ATOMIC_INPUTS) \
    $(SPIRV_DRAW_MSAA_INPUTS), \
    webgpu_frag, \
    $(SPIRV_COMMON_WEBGPU_PARAMS)))
$(eval $(call make_spirv_rules, \
    $(SPIRV_DRAW_ATOMIC_INPUTS) \
    $(SPIRV_DRAW_MSAA_INPUTS), \
    webgpu_fixedcolor_frag, \
    $(SPIRV_COMMON_WEBGPU_PARAMS) -DFIXED_FUNCTION_COLOR_OUTPUT))


spirv: $(SPIRV_OUTPUTS_HEADERS)
spirv-binary: $(SPIRV_OUTPUTS_BINARY)

## WGSL compilation. Translates WebGPU-flavored SPIR-V binaries to WGSL using
## naga. Some Vulkan-flavored SPIR-V files aren't compatible with WebGPU so we
## filter to the webgpu_* variants only — plus a hand-curated list of standard
## variants that translate cleanly (e.g., shaders without combined samplers).
$(OUT)/wgsl/.: | $(OUT)/.
	@mkdir -p $@

WGSL_OUTPUTS := \
    $(patsubst $(OUT)/spirv/%.spv,$(OUT)/wgsl/%.wgsl, \
        $(filter %.webgpu_vert.spv \
                 %.webgpu_noclipdistance_vert.spv \
                 %.webgpu_frag.spv \
                 %.webgpu_fixedcolor_frag.spv, \
                 $(SPIRV_OUTPUTS_BINARY))) \
    $(OUT)/wgsl/draw_msaa_stencil.vert.wgsl \
    $(OUT)/wgsl/draw_msaa_stencil.frag.wgsl \
    $(OUT)/wgsl/color_ramp.vert.wgsl \
    $(OUT)/wgsl/color_ramp.frag.wgsl

## Filter naga's noisy "Unknown decoration RelaxedPrecision" warning — WGSL has
## no precision qualifiers, so naga always drops it. bash with pipefail ensures
## a real naga failure still fails the build (pipeline exit status reflects
## naga's, not grep's). The trailing "[ $$? = 1 ]" absorbs grep's "no matches"
## exit code on clean runs where naga emitted nothing.

## TERM=dumb forces a terminfo entry that always exists. CI sometimes sets
## TERM=unknown, which makes naga's terminal-detection code (ncurses-backed)
## abort with "Error opening terminal: unknown" before it can translate.

## --keep-coordinate-space disables naga's default Y-flip on gl_Position. naga
## assumes Vulkan-flavored SPIR-V (Y-down NDC) and flips to OpenGL/ WebGPU's Y-up
## NDC. Our GLSL is already authored for Y-up — gpu.cpp sets
## renderTargetInverseViewportY negative so the math produces WebGPU clip coords
## directly — and the extra flip would render upside down (Dawn's SPIR-V path
## didn't apply it either).
$(OUT)/wgsl/%.wgsl: $(OUT)/spirv/%.spv | $(OUT)/wgsl/.
	@echo "wgsl/$*.wgsl"
	@TERM=dumb bash -o pipefail -c 'naga --keep-coordinate-space "$<" "$@" 2>&1 | { grep -v "Unknown decoration RelaxedPrecision" || [ $$? = 1 ]; }'

## Embed each .wgsl as a C++ raw string literal in a .hpp header that downstream
## code can #include. The variable lives in the wgsl:: namespace and is named
## after the basename with dots replaced by underscores (e.g.
## wgsl::tessellate_webgpu_vert). wgsl_to_header.py minifies the source first
## unless WGSL_FLAGS includes --raw.
WGSL_HEADER_OUTPUTS := $(patsubst %.wgsl,%.hpp,$(WGSL_OUTPUTS))

$(OUT)/wgsl/%.hpp: $(OUT)/wgsl/%.wgsl wgsl_to_header.py | $(OUT)/wgsl/.
	@python3 wgsl_to_header.py $(WGSL_FLAGS) $< $@ $(subst .,_,$(basename $(notdir $<)))

wgsl: $(WGSL_HEADER_OUTPUTS)

## d3d compilation.
.PHONY: $(OUT)/d3d/render_atlas.frag.h

$(OUT)/d3d/.: | $(OUT)/.
	@mkdir -p $@


FXC_DEBUG_FLAG := $(if $(filter --human-readable,$(FLAGS)),/Zi,)

D3D_OUTPUTS := \
	 $(OUT)/d3d/root.sig.h \
	 $(addprefix $(OUT)/, $(patsubst %.hlsl, %.vert.h, $(wildcard d3d/*.hlsl))) \
	 $(addprefix $(OUT)/, $(patsubst %.hlsl, %.frag.h, $(wildcard d3d/*.hlsl))) \
	 $(OUT)/d3d/render_atlas_stroke.frag.h\
	 $(OUT)/d3d/render_atlas_fill.frag.h\

$(OUT)/d3d/%.vert.h: d3d/%.hlsl $(MINIFY_STAMP) | $(OUT)/d3d/.
	@fxc /D VERTEX /I $(OUT) $(FXC_DEBUG_FLAG) /T vs_5_0 /Fh $@ $<

$(OUT)/d3d/%.frag.h: d3d/%.hlsl $(MINIFY_STAMP) | $(OUT)/d3d/.
	@fxc /D FRAGMENT /I $(OUT) $(FXC_DEBUG_FLAG) /T ps_5_0 /Fh  $@ $<

$(OUT)/d3d/render_atlas_stroke.frag.h: d3d/render_atlas.hlsl $(MINIFY_STAMP) | $(OUT)/d3d/.
	@fxc /D FRAGMENT /D ATLAS_FEATHERED_STROKE $(FXC_DEBUG_FLAG)  /I $(OUT) /T ps_5_0 /Fh  $@ $<

$(OUT)/d3d/render_atlas_fill.frag.h: d3d/render_atlas.hlsl $(MINIFY_STAMP) | $(OUT)/d3d/.
	@fxc /D FRAGMENT /D ATLAS_FEATHERED_FILL $(FXC_DEBUG_FLAG)  /I $(OUT) /T ps_5_0 /Fh  $@ $<

$(OUT)/d3d/root.sig.h: d3d/root.sig | $(OUT)/d3d/.
	@fxc /I $(OUT) /T rootsig_1_1 /E ROOT_SIG /Fh   $@ $<

d3d: $(D3D_OUTPUTS)

## Cleaning.
clean:
	@rm -fr out
