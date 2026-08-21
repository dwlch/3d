#include "audio.hpp"

#include <iostream>
#include <string>
#include <cstdint>
#include <assert.h>

// loading?
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>

// internal.
#include "utility.hpp"
#include "defines.hpp"
#define SOUNDS_PATH "./assets/sounds/"

// Forward declare internal functions.
static DWORD CALLBACK audio_thread_proc(LPVOID arg);

DWORD round_up_pow2(DWORD value)
{
    unsigned long index;
    _BitScanReverse(&index, value - 1);
    assert(index < 31);
    return 1U << (index + 1);
}

// load a sound from a given .wav file.
Sound::Sound(std::string filename, size_t sample_rate, bool loop, float volume)
{
    // convert filename as string into wide string.
    std::string filepath    = SOUNDS_PATH + filename + ".wav";
    int count               = MultiByteToWideChar(CP_ACP, 0, filepath.c_str(), filepath.length(), NULL, 0);

    std::wstring filepath_wide(count, 0);
    MultiByteToWideChar(CP_ACP, 0, filepath.c_str(), filepath.length(), &filepath_wide[0], count);

    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    // assert(SUCCEEDED(hr));

    // load the .wav file from the filepath.
    IMFSourceReader* reader;
    hr = MFCreateSourceReaderFromURL(filepath_wide.c_str(), NULL, &reader);
    // assert(SUCCEEDED(hr));

    // read only first audio stream.
    // -- i think this is just for weird edge cases, i don't think .wav files 'normally' have more than 1 stream.
    hr = reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    // assert(SUCCEEDED(hr));

    hr = reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    // assert(SUCCEEDED(hr));

    const size_t channel_count          = 2;
    WAVEFORMATEXTENSIBLE format         = {};
    format.Format.wFormatTag            = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels             = (WORD)channel_count;
    format.Format.wBitsPerSample        = (WORD)(8 * sizeof(short)); // this sets it to 16.
    format.Format.nSamplesPerSec        = (WORD)sample_rate;
    format.Format.nBlockAlign           = (WORD)((format.Format.nChannels * format.Format.wBitsPerSample) / 8);
    format.Format.nAvgBytesPerSec       = (DWORD)(format.Format.nSamplesPerSec * format.Format.nBlockAlign);
    format.Format.cbSize                = sizeof(format) - sizeof(format.Format);
    format.Samples.wValidBitsPerSample  = 8 * sizeof(short);
    format.dwChannelMask                = (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
    format.SubFormat                    = KSDATAFORMAT_SUBTYPE_PCM;

    // Media Foundation in Windows 8+ allows reader to convert output to different format than native
    IMFMediaType* type;

    hr = MFCreateMediaType(&type);
    // assert(SUCCEEDED(hr));

    hr = MFInitMediaTypeFromWaveFormatEx(type, &format.Format, sizeof(format));
    // assert(SUCCEEDED(hr));

    hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, type);
    // assert(SUCCEEDED(hr));

    type->Release();

    size_t used     = 0;
    size_t capacity = 0;
    for(;;)
    {
        IMFSample* sample;
        DWORD flags = 0;
        hr = reader->ReadSample((DWORD)MF_SOURCE_READER_ALL_STREAMS, 0, NULL, &flags, NULL, &sample);
        if (FAILED(hr))
        {
            break;
        }

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
        {
            // finished reading wav file, so break.
            break;
        }
        assert(flags == 0);

        IMFMediaBuffer* buffer;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        // assert(SUCCEEDED(hr));

        BYTE* data;
        DWORD size;
        hr = buffer->Lock(&data, NULL, &size);
        // assert(SUCCEEDED(hr));
        {
            size_t avail = capacity - used;
            if (avail < size)
            {
                samples = (short*)realloc(samples, capacity += 64 * 1024);
            }
            memcpy((char*)samples + used, data, size);
            used += size;
        }
        hr = buffer->Unlock();
        // assert(SUCCEEDED(hr));

        buffer->Release();
        sample->Release();
    }

    reader->Release();

    hr = MFShutdown();
    // assert(SUCCEEDED(hr));

    sample_count    = (used / format.Format.nBlockAlign) * format.Format.nChannels;
    position        = sample_count;
    is_loop         = loop;
    name            = filename;
    Sound::volume   = volume;
    
    std::cout << "Loaded sound: " << name << "\n";
}

void Sound::play(Win32AudioWriteContext write_context)
{
    // increment sound position by incoming samples.
    position += write_context.prev_sample_count;

    if (is_loop)
    {
        
        position %= sample_count;
    }
    else
    {
        position = glm::min(position, sample_count);
    }

    // copy position to a temp one, as this needs to be incremented within the next loop.
    // which would make the sound speed up considerably if we used position directly.
    size_t current_sample = position;

    for (size_t i = 0; i < write_context.sample_count; ++i)
    {
        if (is_loop)
        {
            if (current_sample == sample_count)
            {
                // reset looping sound back to start
                current_sample = 0;
            }
        }
        else
        {
            if (current_sample >= sample_count)
            {
                // non-looping sounds stops playback when done
                break;
            }
        }

        write_context.samples[0]    += volume * (samples[current_sample++] * (1.0f / 32768.0f));
        write_context.samples[1]    += volume * (samples[current_sample++] * (1.0f / 32768.0f));
        write_context.samples       += 2; // unsure if this just always has to = 2.
    }
}

void Win32AudioStart(Win32Audio* audio, size_t sample_rate, size_t channel_count, DWORD channel_mask)
{
    // *audio = {};

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    // assert(SUCCEEDED(hr));

    // Create enumerator to get audio device
    IMMDeviceEnumerator* enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (LPVOID*)(&enumerator));
    // assert(SUCCEEDED(hr));

    // Get default playback device
    IMMDevice* device;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    // assert(SUCCEEDED(hr));
    enumerator->Release();

    // Create audio client for device
    hr = device->Activate(__uuidof(IAudioClient2), CLSCTX_ALL, NULL, (LPVOID*)&audio->client);
    // assert(SUCCEEDED(hr));
    device->Release();

    // i think this bit is fine actually.
    WAVEFORMATEXTENSIBLE format_ext         = {};
    format_ext.Format.wFormatTag            = WAVE_FORMAT_EXTENSIBLE;
    format_ext.Format.nChannels             = (WORD)channel_count;
    format_ext.Format.nSamplesPerSec        = (WORD)sample_rate;
    format_ext.Format.nBlockAlign           = (WORD)(channel_count * sizeof(float));
    format_ext.Format.nAvgBytesPerSec       = (DWORD)(format_ext.Format.nSamplesPerSec * format_ext.Format.nBlockAlign);
    format_ext.Format.wBitsPerSample        = (WORD)(8 * sizeof(float));
    format_ext.Format.cbSize                = sizeof(format_ext) - sizeof(format_ext.Format);
    format_ext.Samples.wValidBitsPerSample  = 8 * sizeof(float);
    format_ext.dwChannelMask                = channel_mask;
    format_ext.SubFormat                    = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;


    WAVEFORMATEX* wfx;
    if (sample_rate == 0 || channel_count == 0 || channel_mask == 0)
    {
        // Use native mixing format
        hr = audio->client->GetMixFormat(&wfx);
        // assert(SUCCEEDED(hr));
        audio->buffer_format = wfx;
    }
    else
    {
        // Use requested format
        wfx = &format_ext.Format;
        audio->buffer_format = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(format_ext));
        CopyMemory(audio->buffer_format, &format_ext, sizeof(format_ext));
    }

    bool init_success = FALSE;

    // Try to initialize client with newer functionality in Windows 10, no AUTOCONVERTPCM allowed
    IAudioClient3* client_3;
    if (SUCCEEDED(audio->client->QueryInterface(__uuidof(IAudioClient3), (LPVOID*)&client_3)))
    {
        // Minimum buffer size will typically be 480 samples (10msec @ 48khz)
        // but it can be 128 samples (2.66 msec @ 48khz) if driver is properly installed
        // See bullet-point instructions here: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/low-latency-audio#measurement-tools
        UINT32 defaultPeriodSamples, fundamentalPeriodSamples, minPeriodSamples, maxPeriodSamples;
        hr = client_3->GetSharedModeEnginePeriod(wfx, &defaultPeriodSamples, &fundamentalPeriodSamples, &minPeriodSamples, &maxPeriodSamples);

        if (SUCCEEDED(hr))
        {
            const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
            if (SUCCEEDED(client_3->InitializeSharedAudioStream(flags, minPeriodSamples, wfx, NULL)))
            {
                init_success = TRUE;
            }
        }
        client_3->Release();
    }

    // If we couldn't initialize with IAudioClient3, fall back on older API
    if (!init_success)
    {
        std::cout << "IAudioClient3 init failure, falling back on older API." << "\n";
        // Get device period for shared-mode streams, this will typically be 480 samples (10msec @ 48khz)
        REFERENCE_TIME device_period;
        hr = audio->client->GetDevicePeriod(&device_period, NULL);
        // assert(SUCCEEDED(hr));

        const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        hr = audio->client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, device_period, 0, wfx, NULL);
        // assert(SUCCEEDED(hr));
    }

    UINT32 buffer_samples_count;
    hr = audio->client->GetBufferSize(&buffer_samples_count);
    // assert(SUCCEEDED(hr));
    audio->out_buffer_bytes_count = buffer_samples_count * audio->buffer_format->nBlockAlign;

    // Create event handle to wait on - WASAPI will signal it to request we submit samples
    audio->event = CreateEventW(NULL, FALSE, FALSE, NULL);
    hr = audio->client->SetEventHandle(audio->event);
    // assert(SUCCEEDED(hr));

    // Use at least 64KB or 1 second (whichever is larger), and round upwards to pow2 for ringbuffer
    DWORD ring_buffer_bytes_count = round_up_pow2(std::max((DWORD)(64 * 1024), audio->buffer_format->nAvgBytesPerSec));

    // Explanation of Magic Ring Buffer: https://fgiesen.wordpress.com/2012/07/21/the-magic-ring-buffer/
    // MSDN Example code: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2#examples

    // Reserve virtual address placeholder for 2x size for magic ringbuffer
    char* placeholder_1 = (char*)VirtualAlloc2(NULL, NULL, 2 * ring_buffer_bytes_count, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
    assert(placeholder_1);
    char* placeholder_2 = placeholder_1 + ring_buffer_bytes_count;

    // Split allocated address space in half
    BOOL ok = VirtualFree(placeholder_1, ring_buffer_bytes_count, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
    assert(ok);

    // Create page-file backed section for buffer
    HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, ring_buffer_bytes_count, NULL);
    assert(section);

    // Map same section into both addresses
    void* view_1 = MapViewOfFile3(section, NULL, placeholder_1, 0, ring_buffer_bytes_count, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    void* view_2 = MapViewOfFile3(section, NULL, placeholder_2, 0, ring_buffer_bytes_count, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    assert(view_1 && view_2);

    // Free placeholders, actual memory will be freed only when it is unmapped
    VirtualFree(placeholder_1, 0, MEM_RELEASE);
    VirtualFree(placeholder_2, 0, MEM_RELEASE);
    CloseHandle(section);

    audio->buffer_1                 = (BYTE*)view_1;
    audio->buffer_2                 = (BYTE*)view_2;
    audio->ring_buffer_bytes_count  = ring_buffer_bytes_count;

    InitializeSRWLock(&audio->lock);
    audio->thread = CreateThread(NULL, 0, &audio_thread_proc, audio, 0, NULL);
}

void Win32Audio::cleanup()
{
    // Notify thread to stop
    InterlockedExchange(&stop, TRUE);
    SetEvent(event);

    // Wait for thread to finish
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    CloseHandle(event);

    // Release ringbuffer
    UnmapViewOfFileEx(buffer_1, 0);
    UnmapViewOfFileEx(buffer_2, 0);

    // Release audio client
    CoTaskMemFree(buffer_format);
    client->Release();

    CoUninitialize();
}

Win32AudioWriteContext::Win32AudioWriteContext(Win32Audio* audio, float dt)
{
    UINT32 bytes_per_sample         = audio->buffer_format->nBlockAlign;
    UINT32 ring_buffer_bytes_count  = audio->ring_buffer_bytes_count;
    UINT32 out_buffer_bytes_count   = audio->out_buffer_bytes_count;

    AcquireSRWLockExclusive(&audio->lock);

    // How many bytes are in use by audio thread = [read, lock) range
    UINT32 active_bytes_count = audio->ring_buffer_lock_offset - audio->ring_buffer_read_offset;

    // Make sure audio thread has locked enough samples to fill output buffer,
    // in case it gets woken before UnlockBuffer is called
    if (active_bytes_count < out_buffer_bytes_count)
    {
        // Num bytes we've written to ringbuffer = [read, write) range
        // i.e. upper bound on what audio thread can submit to wasapi
        UINT32 bytes_written_count      = audio->ring_buffer_write_offset - audio->ring_buffer_read_offset;
        active_bytes_count              = std::min(out_buffer_bytes_count, bytes_written_count);
        audio->ring_buffer_lock_offset  = audio->ring_buffer_read_offset + active_bytes_count;
    }

    // Set write marker to end of locked region of ringbuffer
    audio->ring_buffer_write_offset = audio->ring_buffer_lock_offset;

    // How many bytes can be written to buffer
    UINT32 bytes_avail_count    = ring_buffer_bytes_count - active_bytes_count;
    prev_sample_count           = audio->prev_sample_count * audio->buffer_format->nChannels;
    audio->prev_sample_count    = 0;

    ReleaseSRWLockExclusive(&audio->lock);

    // UINT32 write_offset = audio->ring_buffer_write_offset % ring_buffer_bytes_count;
    // Fast modulus because ring_buffer_bytes_count is power of 2
    UINT32 write_offset         = audio->ring_buffer_write_offset & (ring_buffer_bytes_count - 1);
    samples                     = (float*)(audio->buffer_1 + write_offset); // Return pointer to ringbuffer at write offset
    UINT32 samples_avail_count  = bytes_avail_count / bytes_per_sample;

    // Set minNumSamplesToWritePerTick to the max amount of time you expect main
    // loop will take until the next tick. If a tick exceeds this time audio
    // will stutter as audio thread will fill the gap with silence
    UINT32 min_samples_per_tick = audio->buffer_format->nSamplesPerSec / dt;
    sample_count                = std::min(min_samples_per_tick, samples_avail_count);

    // Initialise output buffer to 0 for mixing
    memset(samples, 0, sample_count * bytes_per_sample);
}

void Win32AudioWriteContext::release(Win32Audio* audio)
{
    UINT32 bytes_per_sample     = audio->buffer_format->nBlockAlign;
    size_t bytes_written_count  = sample_count * bytes_per_sample;

    // Advance write offset to allow audio thread to read new samples.
    InterlockedAdd(&audio->ring_buffer_write_offset, (LONG)bytes_written_count);
}

// Entry point for audio thread
static DWORD CALLBACK audio_thread_proc(LPVOID arg)
{
    Win32Audio* audio   = (Win32Audio*)arg;
    DWORD task          = 0;
    HANDLE handle       = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task);
    assert(handle);

    IAudioClient* client = audio->client;

    IAudioRenderClient* render_client;
    HRESULT hr = client->GetService(__uuidof(IAudioRenderClient), (LPVOID*)&render_client);
    assert(SUCCEEDED(hr));

    UINT32 buffer_samples_count;
    hr = client->GetBufferSize(&buffer_samples_count);

    std::cout << "buffer_samples_count: " << buffer_samples_count << "\n";
    // assert(SUCCEEDED(hr));

    hr = client->Start();
    // assert(SUCCEEDED(hr));

    UINT32 bytes_per_sample = audio->buffer_format->nBlockAlign;
    UINT32 ring_buffer_mask = audio->ring_buffer_bytes_count - 1;
    BYTE* ring_buffer       = audio->buffer_1;


    // main audio loop.
    while (WaitForSingleObject(audio->event, INFINITE) == WAIT_OBJECT_0)
    {
        if (InterlockedExchange(&audio->stop, FALSE))
        {
            break;
        }

        // How many submitted samples wasapi has left to use
        UINT32 padding_samples_count;
        hr = client->GetCurrentPadding(&padding_samples_count);
        // assert(SUCCEEDED(hr));

        UINT32 sample_count_max = buffer_samples_count - padding_samples_count;

        // Get output buffer from WASAPI
        BYTE* output_buffer;
        hr = render_client->GetBuffer(sample_count_max, &output_buffer);
        // assert(SUCCEEDED(hr));

        AcquireSRWLockExclusive(&audio->lock);

        // Num bytes available to read from ringbuffer
        UINT32 bytes_avail_count        = audio->ring_buffer_write_offset - audio->ring_buffer_read_offset;
        UINT32 samples_avail_count      = bytes_avail_count / bytes_per_sample;

        // Clamp to not exceed available space in wasapi buffer
        UINT32 samples_to_submit_count  = std::min((int)samples_avail_count, (int)sample_count_max);
        UINT32 bytes_to_read_count      = samples_to_submit_count * bytes_per_sample;

        // Lock the range of ringbuffer we will be reading - [read, lock)
        // so the main thread can't overwrite it
        audio->ring_buffer_lock_offset  = audio->ring_buffer_read_offset + bytes_to_read_count;
        DWORD flags                     = 0;

        // If we have no samples to submit, fill buffer with silence
        if (samples_to_submit_count == 0)
        {
            samples_to_submit_count = sample_count_max;
            flags                   = AUDCLNT_BUFFERFLAGS_SILENT;
        }

        // this '2' feels a  bit like it could go many places -- not exactly sure where it should go.
        // need to double the samples count bcos otherwise it stops halfway through the track bcos im doing it in stereo.
        // this seems to work fine though so...?
        // std::cout << "max: " << sample_count_max << "\n";
        // std::cout << "to submit: " << samples_to_submit_count * 2 << "\n";
        audio->prev_sample_count += samples_to_submit_count;

        // std::cout << "audio->prev_sample_count: " << audio->prev_sample_count << "\n";

        // Can now unlock buffer for main thread, it won't write in
        // [read, lock) interval while we're copying to output buffer
        ReleaseSRWLockExclusive(&audio->lock);

        memcpy(output_buffer, ring_buffer + (audio->ring_buffer_read_offset & ring_buffer_mask), bytes_to_read_count);

        // Unlock bytes in [read, lock) interval of ringbuffer
        InterlockedAdd(&audio->ring_buffer_read_offset, bytes_to_read_count);

        // Submit output buffer to WASAPI
        hr = render_client->ReleaseBuffer(samples_to_submit_count, flags);
        // assert(SUCCEEDED(hr));
    }

    // Stop playback
    hr = client->Stop();
    assert(SUCCEEDED(hr));
    render_client->Release();

    AvRevertMmThreadCharacteristics(handle);
    return 0;
}
