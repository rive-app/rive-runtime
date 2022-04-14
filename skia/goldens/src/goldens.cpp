#ifdef TESTING
#else

#include "goldens_arguments.hpp"
#include "goldens_grid.hpp"

#include "rive_mgr.hpp"
#include "rive/animation/animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"

#include "SkData.h"
#include "skia_renderer.hpp"
#include "SkImage.h"
#include "SkPixmap.h"
#include "SkStream.h"
#include "SkSurface.h"

#define DISTURB false

int main(int argc, const char* argv[]) {
    try {
        GoldensArguments args(argc, argv);

        const auto info = SkImageInfo::Make(SW, SH, kRGBA_8888_SkColorType, kOpaque_SkAlphaType);
        auto surf = SkSurface::MakeRaster(info);
        auto canvas = surf->getCanvas();

        canvas->clear(SK_ColorWHITE);

        rive::SkiaRenderer renderer(canvas);

        RenderGoldensGrid(&renderer, args.source().c_str(), "", "");

        if (DISTURB) {
            SkPaint p;
            p.setColor(0x80FF6622);
            canvas->drawCircle(500, 500, 300, p);
        }

        auto img = surf->makeImageSnapshot();
        auto data = img->encodeToData();
        SkFILEWStream(args.destination().c_str()).write(data->data(), data->size());
    } catch (const args::Completion& e) {
        return 0;
    } catch (const args::Help&) {
        return 0;
    } catch (const args::ParseError& e) {
        return 1;
    } catch (args::ValidationError e) {
        return 1;
    }

    return 0;
}

#endif
