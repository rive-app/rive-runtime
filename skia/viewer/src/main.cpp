
// Makes ure gl3w is included before glfw3
#include "GL/gl3w.h"

#define SK_GL
#include "GLFW/glfw3.h"

#include "GrBackendSurface.h"
#include "GrContext.h"
#include "SkCanvas.h"
#include "SkColorSpace.h"
#include "SkSurface.h"
#include "SkTypes.h"
#include "gl/GrGLInterface.h"

#include "animation/linear_animation_instance.hpp"
#include "artboard.hpp"
#include "file.hpp"
#include "layout.hpp"
#include "math/aabb.hpp"
#include "skia_renderer.hpp"

#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include <cmath>
#include <stdio.h>

rive::File* currentFile = nullptr;
rive::Artboard* artboard = nullptr;
rive::LinearAnimationInstance* animationInstance = nullptr;
uint8_t* fileBytes = nullptr;
unsigned int fileBytesLength = 0;

void initAnimation(int index)
{
	assert(fileBytes != nullptr);
	auto reader = rive::BinaryReader(fileBytes, fileBytesLength);
	rive::File* file = nullptr;
	auto result = rive::File::import(reader, &file);
	if (result != rive::ImportResult::success)
	{
		delete[] fileBytes;
		fprintf(stderr, "failed to import file\n");
		return;
	}
	artboard = file->artboard();
	artboard->advance(0.0f);

	delete animationInstance;
	delete currentFile;

	auto animation = index >= 0 && index < artboard->animationCount()
	                     ? artboard->animation(index)
	                     : nullptr;
	if (animation != nullptr)
	{
		animationInstance = new rive::LinearAnimationInstance(animation);
	}
	else
	{
		animationInstance = nullptr;
	}

	currentFile = file;
}

void glfwErrorCallback(int error, const char* description)
{
	puts(description);
}

void glfwDropCallback(GLFWwindow* window, int count, const char** paths)
{
	// Just get the last dropped file for now...
	auto filename = paths[count - 1];
	FILE* fp = fopen(filename, "r");
	fseek(fp, 0, SEEK_END);
	fileBytesLength = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	delete[] fileBytes;
	fileBytes = new uint8_t[fileBytesLength];
	if (fread(fileBytes, 1, fileBytesLength, fp) != fileBytesLength)
	{
		delete[] fileBytes;
		fprintf(stderr, "failed to read all of %s\n", filename);
		return;
	}
	initAnimation(0);
}

int main()
{
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize glfw.\n");
		return 1;
	}
	glfwSetErrorCallback(glfwErrorCallback);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Rive Viewer", NULL, NULL);
	if (window == nullptr)
	{
		fprintf(stderr, "Failed to make window or GL.\n");
		glfwTerminate();
		return 1;
	}

	glfwSetDropCallback(window, glfwDropCallback);
	glfwMakeContextCurrent(window);
	if (gl3wInit() != 0)
	{
		fprintf(stderr, "Failed to make initialize gl3w.\n");
		glfwTerminate();
		return 1;
	}
	// Enable VSYNC.
	glfwSwapInterval(1);

	// Setup ImGui
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 150");
	io.Fonts->AddFontDefault();

	// Setup Skia
	GrContextOptions options;
	sk_sp<GrContext> context = GrContext::MakeGL(nullptr, options);
	GrGLFramebufferInfo framebufferInfo;
	framebufferInfo.fFBOID = 0;
	framebufferInfo.fFormat = GL_RGBA8;

	SkSurface* surface = nullptr;
	SkCanvas* canvas = nullptr;

	// Render loop.
	int width = 0, height = 0;
	int lastScreenWidth = 0, lastScreenHeight = 0;
	double lastTime = glfwGetTime();
	while (!glfwWindowShouldClose(window))
	{
		glfwGetFramebufferSize(window, &width, &height);

		// Update surface.
		if (surface == nullptr || width != lastScreenWidth ||
		    height != lastScreenHeight)
		{
			lastScreenWidth = width;
			lastScreenHeight = height;

			SkColorType colorType =
			    kRGBA_8888_SkColorType; // GrColorTypeToSkColorType(GrPixelConfigToColorType(kRGBA_8888_GrPixelConfig));
			//
			// if (kRGBA_8888_GrPixelConfig == kSkia8888_GrPixelConfig)
			// {
			// 	colorType = kRGBA_8888_SkColorType;
			// }
			// else
			// {
			// 	colorType = kBGRA_8888_SkColorType;
			// }

			GrBackendRenderTarget backendRenderTarget(width,
			                                          height,
			                                          0, // sample count
			                                          0, // stencil bits
			                                          framebufferInfo);

			delete surface;
			surface = SkSurface::MakeFromBackendRenderTarget(
			              context.get(),
			              backendRenderTarget,
			              kBottomLeft_GrSurfaceOrigin,
			              colorType,
			              nullptr,
			              nullptr)
			              .release();
			if (surface == nullptr)
			{
				fprintf(stderr, "Failed to create Skia surface\n");
				return 1;
			}
			canvas = surface->getCanvas();
		}

		double time = glfwGetTime();
		float elapsed = (float)(time - lastTime);
		lastTime = time;

		// Clear screen.
		SkPaint paint;
		paint.setColor(SK_ColorDKGRAY);
		canvas->drawPaint(paint);

		if (artboard != nullptr)
		{
			if (animationInstance != nullptr)
			{
				animationInstance->advance(elapsed);
				animationInstance->apply(artboard);
			}
			artboard->advance(elapsed);

			rive::SkiaRenderer renderer(canvas);
			renderer.save();
			renderer.align(rive::Fit::contain,
			               rive::Alignment::center,
			               rive::AABB(0, 0, width, height),
			               artboard->bounds());
			artboard->draw(&renderer);
			renderer.restore();
		}

		context->flush();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		static int animationIndex = 0;

		if (artboard != nullptr)
		{
			if (ImGui::ListBox(
			        "Animations",
			        &animationIndex,
			        [](void* data, int index, const char** name) {
				        const char* animationName =
				            artboard->animation(index)->name().c_str();
				        *name = animationName;
				        return true;
			        },
			        artboard,
			        artboard->animationCount(),
			        4))
			{
				initAnimation(animationIndex);
			}
		}
		else
		{
			ImGui::Text("Drop a .riv file to preview.");
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	delete currentFile;
	delete[] fileBytes;

	// Cleanup Skia.
	delete surface;
	context = nullptr;

	ImGui_ImplGlfw_Shutdown();

	// Cleanup GLFW.
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}