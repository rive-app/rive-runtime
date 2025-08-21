#ifndef _RIVE_SYMBOL_TYPE_HPP_
#define _RIVE_SYMBOL_TYPE_HPP_
namespace rive
{
enum class SymbolType : unsigned char
{
    none = 0,
    vertexX = 1,
    vertexY = 2,
    cubicVertexInPointX = 3,
    cubicVertexInPointY = 4,
    cubicVertexOutPointX = 5,
    cubicVertexOutPointY = 6,
    rotation = 7,
    inRotation = 8,
    outRotation = 9,
    distance = 10,
    inDistance = 11,
    outDistance = 12,
};
}
#endif
