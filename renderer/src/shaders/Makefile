## Runs minify.py on the whole batch of .glsl files in this folder.
##
## Premake can't do this build step because it has multiple inputs AND multiple outputs.
##
## The files have to be processed in batch in to ensure consistent renaming.
OUT := out/generated
FLAGS :=

## Shader minification.
MINIFY_INPUTS := $(wildcard *.glsl) $(wildcard *.vert) $(wildcard *.frag)
MINIFY_EXPORT_OUTPUTS := $(addprefix $(OUT)/,\
                           $(patsubst %.glsl, %.glsl.exports.h,\
                           $(patsubst %.vert, %.vert.exports.h,\
                           $(patsubst %.frag, %.frag.exports.h,\
                             $(MINIFY_INPUTS)))))
MINIFY_GLSL_OUTPUTS := $(addprefix $(OUT)/,\
                         $(patsubst %.glsl, %.minified.glsl,\
                         $(patsubst %.vert, %.minified.vert,\
                         $(patsubst %.frag, %.minified.frag,\
                           $(MINIFY_INPUTS)))))
MINIFY_HPP_OUTPUTS := $(addprefix $(OUT)/,\
                        $(patsubst %.glsl, %.glsl.hpp,\
                        $(patsubst %.vert, %.vert.hpp,\
                        $(patsubst %.frag, %.frag.hpp,\
                          $(MINIFY_INPUTS)))))
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


## Metal shader offline compiling.
DRAW_COMBINATIONS_METAL := $(OUT)/draw_combinations.metal
METAL_INPUTS := $(wildcard metal/*.metal)
METAL_MACOSX_AIR_OUTPUTS := \
	$(addprefix $(OUT)/, $(patsubst metal/%.metal, macosx/%.air, $(METAL_INPUTS)))
METAL_IOS_AIR_OUTPUTS := $(addprefix $(OUT)/, $(patsubst metal/%.metal, ios/%.air, $(METAL_INPUTS)))

$(DRAW_COMBINATIONS_METAL): metal/generate_draw_combinations.py
	@mkdir -p $(OUT)
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
$(OUT)/macosx/rive_pls_macosx.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL)
	@mkdir -p $(OUT)/macosx
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk macosx metal -std=macos-metal2.3 \
		-mmacosx-version-min=10.0 \
		-I$(OUT) -ffast-math -ffp-contract=fast -fpreserve-invariance -fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/macosx/%.air, $(FILE));)
	xcrun -sdk macosx metallib $(METAL_MACOSX_AIR_OUTPUTS) -o $(OUT)/macosx/rive_pls_macosx.metallib

$(OUT)/rive_pls_macosx.metallib.c: $(OUT)/macosx/rive_pls_macosx.metallib
	xxd -i -n rive_pls_macosx_metallib \
		$(OUT)/macosx/rive_pls_macosx.metallib \
		$(OUT)/rive_pls_macosx.metallib.c

$(OUT)/ios/rive_pls_ios.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL)
	@mkdir -p $(OUT)/ios
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk iphoneos metal -std=ios-metal2.2 \
		-I$(OUT) -mios-version-min=13 -ffast-math -ffp-contract=fast -fpreserve-invariance \
		-fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/ios/%.air, $(FILE));)
	xcrun -sdk iphoneos metallib $(METAL_IOS_AIR_OUTPUTS) -o $(OUT)/ios/rive_pls_ios.metallib

$(OUT)/rive_pls_ios.metallib.c: $(OUT)/ios/rive_pls_ios.metallib
	xxd -i -n rive_pls_ios_metallib $(OUT)/ios/rive_pls_ios.metallib $(OUT)/rive_pls_ios.metallib.c

$(OUT)/ios/rive_pls_ios_simulator.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL)
	@mkdir -p $(OUT)/ios
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk iphonesimulator metal -std=ios-metal2.2 \
		-I$(OUT) -miphonesimulator-version-min=13 -ffast-math -ffp-contract=fast -fpreserve-invariance \
		-fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/ios/%.air, $(FILE));)
	xcrun -sdk iphonesimulator metallib $(METAL_IOS_AIR_OUTPUTS) -o $(OUT)/ios/rive_pls_ios_simulator.metallib

$(OUT)/rive_pls_ios_simulator.metallib.c: $(OUT)/ios/rive_pls_ios_simulator.metallib
	xxd -i -n rive_pls_ios_simulator_metallib $(OUT)/ios/rive_pls_ios_simulator.metallib $(OUT)/rive_pls_ios_simulator.metallib.c

$(OUT)/ios/rive_renderer_xros.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL)
	@mkdir -p $(OUT)/ios
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk xros metal -std=metal3.1 \
		-I$(OUT) --target=air64-apple-xros1.0 -ffast-math -ffp-contract=fast -fpreserve-invariance \
		-fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/ios/%.air, $(FILE));)
	xcrun -sdk xros metallib $(METAL_IOS_AIR_OUTPUTS) -o $(OUT)/ios/rive_renderer_xros.metallib

$(OUT)/rive_renderer_xros.metallib.c: $(OUT)/ios/rive_renderer_xros.metallib
	xxd -i -n rive_renderer_xros_metallib $(OUT)/ios/rive_renderer_xros.metallib $(OUT)/rive_renderer_xros.metallib.c

$(OUT)/ios/rive_renderer_xros_simulator.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL)
	@mkdir -p $(OUT)/ios
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk xrsimulator metal -std=metal3.1 \
		-I$(OUT) --target=air64-apple-xros1.0-simulator -ffast-math -ffp-contract=fast -fpreserve-invariance \
		-fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/ios/%.air, $(FILE));)
	xcrun -sdk xrsimulator metallib $(METAL_IOS_AIR_OUTPUTS) -o $(OUT)/ios/rive_renderer_xros_simulator.metallib

$(OUT)/rive_renderer_xros_simulator.metallib.c: $(OUT)/ios/rive_renderer_xros_simulator.metallib
	xxd -i -n rive_renderer_xros_simulator_metallib $(OUT)/ios/rive_renderer_xros_simulator.metallib $(OUT)/rive_renderer_xros_simulator.metallib.c

$(OUT)/ios/rive_renderer_appletvos.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL)
	@mkdir -p $(OUT)/ios
	$(foreach FILE, $(METAL_INPUTS), \
		xcrun -sdk appletvos metal -std=metal3.0 \
		-I$(OUT) -mappletvos-version-min=16.0 -ffast-math -ffp-contract=fast -fpreserve-invariance \
		-fvisibility=hidden \
		-c $(FILE) \
		-o $(patsubst metal/%.metal, $(OUT)/ios/%.air, $(FILE));)
	xcrun -sdk appletvos metallib $(METAL_IOS_AIR_OUTPUTS) -o $(OUT)/ios/rive_renderer_appletvos.metallib

$(OUT)/rive_renderer_appletvos.metallib.c: $(OUT)/ios/rive_renderer_appletvos.metallib
	xxd -i -n rive_renderer_appletvos_metallib $(OUT)/ios/rive_renderer_appletvos.metallib $(OUT)/rive_renderer_appletvos.metallib.c

$(OUT)/ios/rive_renderer_appletvsimulator.metallib: $(MINIFY_GLSL_OUTPUTS) $(METAL_INPUTS) $(DRAW_COMBINATIONS_METAL)
	@mkdir -p $(OUT)/ios
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
SPIRV_INPUTS := $(wildcard spirv/*.main) \
				$(wildcard spirv/*.vert) \
				$(wildcard spirv/*.frag)

SPIRV_OUTPUTS_HEADERS := \
    $(addprefix $(OUT)/, $(patsubst %.main, %.vert.h, $(wildcard spirv/*.main))) \
    $(addprefix $(OUT)/, $(patsubst %.main, %.frag.h, $(wildcard spirv/*.main))) \
    $(addprefix $(OUT)/, $(patsubst %.vert, %.vert.h, $(wildcard spirv/*.vert))) \
    $(addprefix $(OUT)/, $(patsubst %.frag, %.frag.h, $(wildcard spirv/*.frag))) \
    $(OUT)/spirv/atomic_draw_image_mesh.fixedcolor_frag.h \
    $(OUT)/spirv/atomic_draw_image_rect.fixedcolor_frag.h \
    $(OUT)/spirv/atomic_draw_interior_triangles.fixedcolor_frag.h \
    $(OUT)/spirv/atomic_draw_atlas_blit.fixedcolor_frag.h \
    $(OUT)/spirv/atomic_draw_path.fixedcolor_frag.h \
    $(OUT)/spirv/atomic_resolve.fixedcolor_frag.h \
    $(OUT)/spirv/draw_msaa_atlas_blit.fixedcolor_frag.h \
    $(OUT)/spirv/draw_msaa_image_mesh.fixedcolor_frag.h \
    $(OUT)/spirv/draw_msaa_path.fixedcolor_frag.h \
    $(OUT)/spirv/draw_msaa_stencil.fixedcolor_frag.h \
    $(OUT)/spirv/draw_path.webgpu_vert.h \
    $(OUT)/spirv/draw_path.webgpu_frag.h \
    $(OUT)/spirv/draw_interior_triangles.webgpu_vert.h \
    $(OUT)/spirv/draw_interior_triangles.webgpu_frag.h \
    $(OUT)/spirv/draw_atlas_blit.webgpu_vert.h \
    $(OUT)/spirv/draw_atlas_blit.webgpu_frag.h \
    $(OUT)/spirv/draw_image_mesh.webgpu_frag.h \
	$(OUT)/spirv/draw_msaa_path.noclipdistance_vert.h \
	$(OUT)/spirv/draw_msaa_image_mesh.noclipdistance_vert.h \
	$(OUT)/spirv/draw_msaa_atlas_blit.noclipdistance_vert.h \

SPIRV_OUTPUTS_BINARY := \
    $(addprefix $(OUT)/, $(patsubst %.main, %.vert.spirv, $(wildcard spirv/*.main))) \
    $(addprefix $(OUT)/, $(patsubst %.main, %.frag.spirv, $(wildcard spirv/*.main))) \
    $(addprefix $(OUT)/, $(patsubst %.vert, %.vert.spirv, $(wildcard spirv/*.vert))) \
    $(addprefix $(OUT)/, $(patsubst %.frag, %.frag.spirv, $(wildcard spirv/*.frag))) \
    $(OUT)/spirv/atomic_draw_image_mesh.fixedcolor_frag.spirv \
    $(OUT)/spirv/atomic_draw_image_rect.fixedcolor_frag.spirv \
    $(OUT)/spirv/atomic_draw_interior_triangles.fixedcolor_frag.spirv \
    $(OUT)/spirv/atomic_draw_atlas_blit.fixedcolor_frag.spirv \
    $(OUT)/spirv/atomic_draw_path.fixedcolor_frag.spirv \
    $(OUT)/spirv/atomic_resolve.fixedcolor_frag.spirv \
    $(OUT)/spirv/draw_msaa_atlas_blit.fixedcolor_frag.spirv \
    $(OUT)/spirv/draw_msaa_image_mesh.fixedcolor_frag.spirv \
    $(OUT)/spirv/draw_msaa_path.fixedcolor_frag.spirv \
    $(OUT)/spirv/draw_msaa_stencil.fixedcolor_frag.spirv \
	$(OUT)/spirv/draw_msaa_path.noclipdistance_vert.spirv \
	$(OUT)/spirv/draw_msaa_atlas_blit.noclipdistance_vert.spirv \
    $(OUT)/spirv/draw_msaa_image_mesh.noclipdistance_vert.spirv \
    
## Compile *.main into vertex shaders. First rule generates the binary spirv, then the .h file after.
$(OUT)/spirv/%.vert.spirv: spirv/%.main $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S vert -DTARGET_VULKAN -DVERTEX -I$(OUT) -V -o $@ $<

$(OUT)/spirv/%.vert.h: spirv/%.main $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S vert -DTARGET_VULKAN -DVERTEX -I$(OUT) -V --vn $(subst .main,_vert,$(notdir $<)) -o $@ $<

## Compile *.main again into fragment shaders.
$(OUT)/spirv/%.frag.spirv: spirv/%.main $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S frag -DTARGET_VULKAN -DFRAGMENT -DPLS_IMPL_SUBPASS_LOAD -I$(OUT) -V -o $@ $<

$(OUT)/spirv/%.frag.h: spirv/%.main $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S frag -DTARGET_VULKAN -DFRAGMENT -DPLS_IMPL_SUBPASS_LOAD -I$(OUT) -V --vn $(subst .main,_frag,$(notdir $<)) -o $@ $<

## Compile atomic fragment shaders again with FIXED_FUNCTION_COLOR_OUTPUT defined.
$(OUT)/spirv/%.fixedcolor_frag.spirv: spirv/%.main $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S frag -DTARGET_VULKAN -DFRAGMENT -DPLS_IMPL_SUBPASS_LOAD -DFIXED_FUNCTION_COLOR_OUTPUT -I$(OUT) -V -o $@ $<

$(OUT)/spirv/%.fixedcolor_frag.h: spirv/%.main $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S frag -DTARGET_VULKAN -DFRAGMENT -DPLS_IMPL_SUBPASS_LOAD -DFIXED_FUNCTION_COLOR_OUTPUT -I$(OUT) -V --vn $(subst .main,_fixedcolor_frag,$(notdir $<)) -o $@ $<

## Compile msaa vertex shaders again with DISABLE_CLIP_RECT_FOR_VULKAN_MSAA defined.
$(OUT)/spirv/%.noclipdistance_vert.spirv: spirv/%.main $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S vert -DTARGET_VULKAN -DVERTEX -DDISABLE_CLIP_RECT_FOR_VULKAN_MSAA -I$(OUT) -V -o $@ $<

$(OUT)/spirv/%.noclipdistance_vert.h: spirv/%.main $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S vert -DTARGET_VULKAN -DVERTEX -DDISABLE_CLIP_RECT_FOR_VULKAN_MSAA -I$(OUT) -V --vn $(subst .main,_noclipdistance_vert,$(notdir $<)) -o $@ $<

## Compile *.vert into vertex shaders.
$(OUT)/spirv/%.vert.spirv: spirv/%.vert $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S vert -DTARGET_VULKAN -DVERTEX -I$(OUT) -V -o $@ $<

$(OUT)/spirv/%.vert.h: spirv/%.vert $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S vert -DTARGET_VULKAN -DVERTEX -I$(OUT) -V --vn $(subst .vert,_vert,$(notdir $<)) -o $@ $<

## Compile *.frag into fragment shaders.
$(OUT)/spirv/%.frag.spirv: spirv/%.frag $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S frag -DTARGET_VULKAN -DFRAGMENT -DPLS_IMPL_SUBPASS_LOAD -I$(OUT) -V -o $@ $<

$(OUT)/spirv/%.frag.h: spirv/%.frag $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S frag -DTARGET_VULKAN -DFRAGMENT -DPLS_IMPL_SUBPASS_LOAD -I$(OUT) -V --vn $(subst .frag,_frag,$(notdir $<)) -o $@ $<

## Compile fragment shaders in a way that is compatible with webgpu.
$(OUT)/spirv/%.webgpu_vert.h: spirv/%.main $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S vert -DTARGET_VULKAN -DVERTEX -DSPEC_CONST_NONE -I$(OUT) -V --vn $(subst .main,_webgpu_vert,$(notdir $<)) -o $@ $<

$(OUT)/spirv/%.webgpu_frag.h: spirv/%.main $(MINIFY_STAMP)
	@mkdir -p $(OUT)/spirv
	@glslangValidator -S frag -DTARGET_VULKAN -DFRAGMENT -DPLS_IMPL_NONE -DSPEC_CONST_NONE -I$(OUT) -V --vn $(subst .main,_webgpu_frag,$(notdir $<)) -o $@ $<

spirv: $(SPIRV_OUTPUTS_HEADERS)
spirv-binary: $(SPIRV_OUTPUTS_BINARY)

## d3d compilation.
.PHONY: $(OUT)/d3d/render_atlas.frag.h


FXC_DEBUG_FLAG := $(if $(filter --human-readable,$(FLAGS)),/Zi,)

D3D_OUTPUTS := \
	 $(OUT)/d3d/root.sig.h \
	 $(addprefix $(OUT)/, $(patsubst %.hlsl, %.vert.h, $(wildcard d3d/*.hlsl))) \
	 $(addprefix $(OUT)/, $(patsubst %.hlsl, %.frag.h, $(wildcard d3d/*.hlsl))) \
	 $(OUT)/d3d/render_atlas_stroke.frag.h\
	 $(OUT)/d3d/render_atlas_fill.frag.h\

$(OUT)/d3d/%.vert.h: d3d/%.hlsl $(MINIFY_STAMP)
	@mkdir -p $(OUT)/d3d
	@fxc /D VERTEX /I $(OUT) $(FXC_DEBUG_FLAG) /T vs_5_0 /Fh $@ $<

$(OUT)/d3d/%.frag.h: d3d/%.hlsl $(MINIFY_STAMP)
	@mkdir -p $(OUT)/d3d
	@fxc /D FRAGMENT /I $(OUT) $(FXC_DEBUG_FLAG) /T ps_5_0 /Fh  $@ $<

$(OUT)/d3d/render_atlas_stroke.frag.h: d3d/render_atlas.hlsl $(MINIFY_STAMP)
	@mkdir -p $(OUT)/d3d
	@fxc /D FRAGMENT /D ATLAS_FEATHERED_STROKE $(FXC_DEBUG_FLAG)  /I $(OUT) /T ps_5_0 /Fh  $@ $<

$(OUT)/d3d/render_atlas_fill.frag.h: d3d/render_atlas.hlsl $(MINIFY_STAMP)
	@mkdir -p $(OUT)/d3d
	@fxc /D FRAGMENT /D ATLAS_FEATHERED_FILL $(FXC_DEBUG_FLAG)  /I $(OUT) /T ps_5_0 /Fh  $@ $<

$(OUT)/d3d/root.sig.h: d3d/root.sig
	@mkdir -p $(OUT)/d3d
	@fxc /I $(OUT) /T rootsig_1_1 /E ROOT_SIG /Fh   $@ $<

d3d: $(D3D_OUTPUTS)

## Cleaning.
clean:
	@rm -fr out
