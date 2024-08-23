#ifndef _RIVE_CLIP_RESULT_HPP_
#define _RIVE_CLIP_RESULT_HPP_
namespace rive
{
enum class ClipResult : unsigned char
{
    noClip = 0,
    clip = 1,
    emptyClip = 2
};
}
#endif