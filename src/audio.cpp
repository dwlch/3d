#include "audio.hpp"
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <mmsystem.h>
#include <aviriff.h>

#include <assert.h>
#include <stdint.h>

#define SOUNDS_PATH     "./assets/sounds/"
#include <string>

bool win32_load_file(const char* filename, void** data, uint32_t* byte_read_count)
{
    HANDLE file = CreateFileA(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if((file == INVALID_HANDLE_VALUE)) return false;

    DWORD file_size = GetFileSize(file, 0);
    if(!file_size) return false;

    *data = HeapAlloc(GetProcessHeap(), 0, file_size + 1);
    if(!*data) return false;

    if(!ReadFile(file, *data, file_size, (LPDWORD)byte_read_count, 0)) return false;

    CloseHandle(file);
    ((uint8_t*)*data)[file_size] = 0;

    return true;
}

void win32_free_file(void* data)
{
    HeapFree(GetProcessHeap(), 0, data);
}

void AudioHandler::update()
{
    // Padding is how much valid data is queued up in the sound buffer
    // if there's enough padding then we could skip writing more data
    UINT32 buffer_padding;
    hr = audio_client->GetCurrentPadding(&buffer_padding);
    assert(hr == S_OK);

    // How much padding we want our sound buffer to have after writing to it.
    // Needs to be enough so that the playback doesn't reach garbage data
    // but we get less latency the lower it is (i.e. how long does it take
    // between pressing jump and hearing the sound effect)
    // Try setting this to e.g. 1/250.f to hear what happens when
    // we're not writing enough data to stay ahead of playback!
    const float TARGET_BUFFER_PADDING_IN_SECONDS = 1.0f / 60.0f;
    UINT32 target_buffer_padding = UINT32(buffer_size_in_frames * TARGET_BUFFER_PADDING_IN_SECONDS);
    UINT32 frames_to_write_count = target_buffer_padding - buffer_padding;

    int16_t* buffer;
    hr = audio_render_client->GetBuffer(frames_to_write_count, (BYTE**)(&buffer));
    assert(hr == S_OK);

    for(UINT32 index = 0; index < frames_to_write_count; ++index)
    {
        uint32_t leftSampleIndex    = wav_playback_sample;
        uint32_t rightSampleIndex   = wav_playback_sample + clip.channel_count - 1;
        *buffer++ = ((uint16_t*)clip.samples)[leftSampleIndex];
        *buffer++ = ((uint16_t*)clip.samples)[rightSampleIndex];
        wav_playback_sample += clip.channel_count;
        wav_playback_sample %= clip.sample_count; // Loop if we reach end of wav file
    }
    hr = audio_render_client->ReleaseBuffer(frames_to_write_count, 0);
    assert(hr == S_OK);
}

AudioHandler::AudioHandler()
{
    const char* filename = "./assets/sounds/30-b.wav";
    void* file_bytes;
    uint32_t file_size;
    bool result = win32_load_file(filename, &file_bytes, &file_size);
    assert(result);

    clip = parse_wav_file((uint8_t*)file_bytes, file_size);
    assert(clip.channel_count == 2 || clip.channel_count == 1);
    assert(clip.sample_rate == 44100);
    assert(clip.bits_per_sample == 16);

    hr = CoInitializeEx(nullptr, COINIT_SPEED_OVER_MEMORY);
    assert(hr == S_OK);

    IMMDeviceEnumerator* deviceEnumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (LPVOID*)(&deviceEnumerator));
    assert(hr == S_OK);

    IMMDevice* audioDevice;
    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &audioDevice);
    assert(hr == S_OK);

    deviceEnumerator->Release();

    // IAudioClient2* audioClient;
    hr = audioDevice->Activate(__uuidof(IAudioClient2), CLSCTX_ALL, nullptr, (LPVOID*)(&audio_client));
    assert(hr == S_OK);

    audioDevice->Release();

    const int32_t OUTPUT_SAMPLE_RATE    = 44100;
    WAVEFORMATEX mixFormat              = {};
    mixFormat.wFormatTag                = WAVE_FORMAT_PCM;
    mixFormat.nChannels                 = 2;
    mixFormat.nSamplesPerSec            = OUTPUT_SAMPLE_RATE;
    mixFormat.wBitsPerSample            = 16;
    mixFormat.nBlockAlign               = (mixFormat.nChannels * mixFormat.wBitsPerSample) / 8;
    mixFormat.nAvgBytesPerSec           = mixFormat.nSamplesPerSec * mixFormat.nBlockAlign;

    const float BUFFER_SIZE_IN_SECONDS          = 2.0f;
    const int64_t REFTIMES_PER_SEC              = 10000000; // hundred nanoseconds
    REFERENCE_TIME requestedSoundBufferDuration = (REFERENCE_TIME)(REFTIMES_PER_SEC * BUFFER_SIZE_IN_SECONDS);
    DWORD initStreamFlags                       = (AUDCLNT_STREAMFLAGS_RATEADJUST | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY);
    
    
    hr = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, initStreamFlags, requestedSoundBufferDuration, 0, &mixFormat, nullptr);
    assert(hr == S_OK);

    
    hr = audio_client->GetService(__uuidof(IAudioRenderClient), (LPVOID*)(&audio_render_client));
    assert(hr == S_OK);

    hr = audio_client->GetBufferSize(&buffer_size_in_frames);
    assert(hr == S_OK);

    hr = audio_client->Start();
    assert(hr == S_OK);
}


AudioSource AudioHandler::parse_wav_file(uint8_t* file_bytes, uint32_t file_size)
{
    AudioSource result  = {};
    RIFFLIST* header    = (RIFFLIST*)file_bytes;


    if(header->fcc != FCC('RIFF') || header->fccListType != FCC('WAVE')) //*(DWORD*)"RIFF"
    {
        assert(!"Invalid wav header");
        return result;
    }

    void* sub_chunks    = file_bytes + sizeof(RIFFLIST);
    void* file_end      = file_bytes + file_size;


    for(RIFFCHUNK* chunk = (RIFFCHUNK*)(sub_chunks); chunk < file_end; chunk = RIFFNEXT(chunk))
    {
        if(chunk->fcc == FCC('fmt '))
        {
            WAVEFORMATEX* fmt = (WAVEFORMATEX*)(chunk + 1);

            if(fmt->wFormatTag != WAVE_FORMAT_PCM)
            {
                assert(!"Unsupported format - PCM only");
                return result;
            }

            assert(chunk->cb == 16 || chunk->cb == 18);
            assert(fmt->nBlockAlign == fmt->nChannels * fmt->wBitsPerSample / 8);
            assert(fmt->nAvgBytesPerSec == fmt->nSamplesPerSec * fmt->nBlockAlign);

            result.channel_count    = fmt->nChannels;
            result.sample_rate      = fmt->nSamplesPerSec;
            result.bits_per_sample  = fmt->wBitsPerSample;
        }
        else if(chunk->fcc == FCC('data'))
        {
            result.sample_count = chunk->cb / sizeof(uint16_t);
            result.samples      = ((uint8_t*)chunk + sizeof(RIFFCHUNK));
            assert((uint8_t*)result.samples + chunk->cb - 1 < file_end);
        }
    }
    return result;
}
