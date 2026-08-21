#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

// https://github.com/kevinmoran/BeginnerWASAPI/tree/master/02.%20Playing%20a%20Wav%20File
struct tWAVEFORMATEX;
struct IAudioClient;

struct Win32Audio
{
    IAudioClient*   client;
    HANDLE          event;
    HANDLE          thread;
    LONG            stop;

    BYTE*           buffer_1;
    BYTE*           buffer_2;
    tWAVEFORMATEX*  buffer_format;
    UINT32          out_buffer_bytes_count;
    UINT32          ring_buffer_bytes_count;    // always power of 2

    SRWLOCK         lock;
    UINT32          prev_sample_count;
    volatile LONG   ring_buffer_read_offset;    // offset for audio thread to read from buffer
    volatile LONG   ring_buffer_lock_offset;    // offset to end of region audio thread is reading
    volatile LONG   ring_buffer_write_offset;   // offset to point main loop has written to

    float volume = 1.0f;

    // Win32Audio() {};
    // Win32Audio(size_t sample_rate, size_t channel_count, DWORD channel_mask);
    void cleanup();

};

void Win32AudioStart(Win32Audio* audio, size_t sample_rate, size_t channel_count, DWORD channel_mask);
// void Win32AudioStop(Win32Audio* audio);

struct Win32AudioWriteContext
{
    float* samples;
    size_t sample_count;        // number of samples to write.
    size_t prev_sample_count;   // samples since last tick.

    Win32AudioWriteContext(Win32Audio* audio, float dt);
    void release(Win32Audio* audio);
};

struct Sound
{
    short* samples      = NULL; // contents of sound stored as samples.
    size_t sample_count = 0;    // total number of samples in the sound.
    size_t position     = 0;    // the current sample.
    bool is_loop        = false;
    float volume        = 1.0f;
    std::string name;

    Sound() {};
    Sound(std::string filename, size_t sample_rate, bool loop, float volume);
    void play(Win32AudioWriteContext write_context);
};
