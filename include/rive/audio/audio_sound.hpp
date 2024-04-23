#ifdef WITH_RIVE_AUDIO
#ifndef _RIVE_AUDIO_SOUND_HPP_
#define _RIVE_AUDIO_SOUND_HPP_

#include "miniaudio.h"
#include "rive/refcnt.hpp"
#include "rive/audio/audio_source.hpp"

namespace rive
{
class AudioEngine;
class Artboard;

struct ma_end_clipped_decoder
{
    ma_data_source_base base;
    ma_decoder decoder;
    ma_uint64 frameCursor;
    ma_uint64 endFrame;
};

static ma_result ma_end_clipped_decoder_read(ma_data_source* pDataSource,
                                             void* pFramesOut,
                                             ma_uint64 frameCount,
                                             ma_uint64* pFramesRead)
{
    ma_end_clipped_decoder* clipped = (ma_end_clipped_decoder*)pDataSource;

    ma_result result =
        ma_decoder_read_pcm_frames(&clipped->decoder, pFramesOut, frameCount, pFramesRead);

    clipped->frameCursor += *pFramesRead;
    if (clipped->frameCursor >= clipped->endFrame)
    {
        ma_uint64 overflow = clipped->frameCursor - clipped->endFrame;
        if (*pFramesRead > overflow)
        {
            *pFramesRead -= overflow;
        }
        else
        {
            *pFramesRead = 0;
            return MA_AT_END;
        }
    }

    return result;
}

static ma_result ma_end_clipped_decoder_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    ma_end_clipped_decoder* clipped = (ma_end_clipped_decoder*)pDataSource;
    ma_result result = ma_decoder_seek_to_pcm_frame(&clipped->decoder, frameIndex);
    if (result != MA_SUCCESS)
    {
        return result;
    }

    clipped->frameCursor = frameIndex;
    return result;
}

static ma_result ma_end_clipped_decoder_get_data_format(ma_data_source* pDataSource,
                                                        ma_format* pFormat,
                                                        ma_uint32* pChannels,
                                                        ma_uint32* pSampleRate,
                                                        ma_channel* pChannelMap,
                                                        size_t channelMapCap)
{
    ma_end_clipped_decoder* clipped = (ma_end_clipped_decoder*)pDataSource;
    return ma_decoder_get_data_format(&clipped->decoder,
                                      pFormat,
                                      pChannels,
                                      pSampleRate,
                                      pChannelMap,
                                      channelMapCap);
}

static ma_result ma_end_clipped_decoder_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    ma_end_clipped_decoder* clipped = (ma_end_clipped_decoder*)pDataSource;
    *pCursor = clipped->frameCursor;
    return MA_SUCCESS;
}

static ma_result ma_end_clipped_decoder_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
    ma_end_clipped_decoder* clipped = (ma_end_clipped_decoder*)pDataSource;
    return ma_decoder_get_length_in_pcm_frames(&clipped->decoder, pLength);
}

static ma_data_source_vtable g_ma_end_clipped_decoder_vtable = {
    ma_end_clipped_decoder_read,
    ma_end_clipped_decoder_seek,
    ma_end_clipped_decoder_get_data_format,
    ma_end_clipped_decoder_get_cursor,
    ma_end_clipped_decoder_get_length};

class AudioSound : public RefCnt<AudioSound>
{
    friend class AudioEngine;

public:
    bool seek(uint64_t timeInFrames);
    ~AudioSound();
    void stop(uint64_t fadeTimeInFrames = 0);
    float volume();
    void volume(float value);
    bool completed() const;

private:
    AudioSound(AudioEngine* engine, rcp<AudioSource> source, Artboard* artboard);
    ma_end_clipped_decoder* clippedDecoder() { return &m_decoder; }
    ma_audio_buffer* buffer() { return &m_buffer; }
    ma_sound* sound() { return &m_sound; }
    void dispose();

    ma_end_clipped_decoder m_decoder;
    ma_audio_buffer m_buffer;
    ma_sound m_sound;
    rcp<AudioSource> m_source;

    // This is storage used by the AudioEngine.
    bool m_isDisposed;
    rcp<AudioSound> m_nextPlaying;
    rcp<AudioSound> m_prevPlaying;
    AudioEngine* m_engine;
    Artboard* m_artboard;
};
} // namespace rive

#endif
#endif