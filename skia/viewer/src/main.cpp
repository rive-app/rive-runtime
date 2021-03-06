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

#include <cmath>
#include <stdio.h>

rive::File* currentFile = nullptr;
rive::Artboard* artboard = nullptr;
rive::LinearAnimationInstance* animationInstance = nullptr;

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
	auto length = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	uint8_t* bytes = new uint8_t[length];
	if (fread(bytes, 1, length, fp) != length)
	{
		delete[] bytes;
		fprintf(stderr, "failed to read all of %s\n", filename);
		return;
	}
	auto reader = rive::BinaryReader(bytes, length);
	rive::File* file = nullptr;
	auto result = rive::File::import(reader, &file);
	if (result != rive::ImportResult::success)
	{
		delete[] bytes;
		fprintf(stderr, "failed to import %s\n", filename);
		return;
	}
	artboard = file->artboard();
	artboard->advance(0.0f);

	delete animationInstance;
	delete currentFile;

	auto animation = artboard->firstAnimation();
	if (animation != nullptr)
	{
		animationInstance = new rive::LinearAnimationInstance(animation);
	}
	else
	{
		animationInstance = nullptr;
	}

	currentFile = file;
	delete[] bytes;
}

int main()
{
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize glfw.\n");
		return 1;
	}
	glfwSetErrorCallback(glfwErrorCallback);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Rive Viewer", NULL, NULL);
	if (window == nullptr)
	{
		fprintf(stderr, "Failed to make window or GL.\n");
		glfwTerminate();
		return 1;
	}

	glfwSetDropCallback(window, glfwDropCallback);
	glfwMakeContextCurrent(window);
	// Enable VSYNC.
	glfwSwapInterval(1);

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

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	delete currentFile;

	// Cleanup Skia.
	delete surface;
	context = nullptr;

	// Cleanup GLFW.
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}