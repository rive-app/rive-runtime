#ifndef GOLDENS_GRID_HPP
#define GOLDENS_GRID_HPP

namespace rive {
    class Renderer;
}

constexpr static int CELL = 256;
constexpr static int W = 5;
constexpr static int H = 5;
constexpr static int GAP = 2;
constexpr static int SW = W * CELL + (W + 1) * GAP;
constexpr static int SH = H * CELL + (H + 1) * GAP;

void RenderGoldensGrid(rive::Renderer* renderer,
                       const char* source,
                       const char* artboardName,
                       const char* animationName);

#endif
