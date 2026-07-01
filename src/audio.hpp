#pragma once
#include <stdint.h>
#include "audio.hpp"
#include <audioclient.h>


struct AudioSource
{
    uint32_t channel_count;
    uint32_t bits_per_sample;
    uint32_t sample_rate;
    uint32_t sample_count;
    void* samples;

    
};

struct AudioHandler
{
    IAudioClient2* audio_client;
    IAudioRenderClient* audio_render_client;
    UINT32 buffer_size_in_frames;
    uint32_t wav_playback_sample = 0;
    AudioSource clip = {};
    HRESULT hr;

    AudioHandler();
    void update();

    AudioSource parse_wav_file(uint8_t* file_bytes, uint32_t file_size);

};