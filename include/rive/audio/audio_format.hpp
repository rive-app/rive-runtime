#ifndef _RIVE_AUDIO_FORMAT_HPP_
#define _RIVE_AUDIO_FORMAT_HPP_
namespace rive
{
enum class AudioFormat : unsigned int
{
    unknown = 0,
    wav,
    flac,
    mp3,
    vorbis,
    buffered
};
}
#endif