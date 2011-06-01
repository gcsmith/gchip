// gchip - a simple recompiling chip-8 emulator
// Copyright (C) 2011  Garrett Smith.
// 
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2 of the License, or (at your
// option) any later version.
// 
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
// 
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "SDL_audio.h"
#include "audio.h"
#include "common.h"

const int num_samples = 512;
const int freq = 8;

// -----------------------------------------------------------------------------
void audio_callback(void *userdata, uint8_t *stream, int len)
{
    memcpy(stream, ((audio_data_t *)userdata)->buffer, len);
}

// -----------------------------------------------------------------------------
void audio_init(audio_data_t *audio)
{
    int i;
    float wave;

    audio->buffer = (uint8_t *)malloc(2 * num_samples);
    audio->pos = 0;
    audio->len = num_samples;

    for (i = 0; i < num_samples; i++) {
        wave = sin(i * 2.0 * freq * M_PI / num_samples);
        audio->buffer[i] = (uint8_t)(127 * wave);
    }

    SDL_AudioSpec fmt;
    fmt.freq = 22050;
    fmt.format = AUDIO_S16;
    fmt.channels = 1;
    fmt.samples = num_samples;
    fmt.callback = audio_callback;
    fmt.userdata = audio;

    if (SDL_OpenAudio(&fmt, NULL) < 0) {
        log_err("failed to open audio (%s)", SDL_GetError());
        return;
    }
}

// -----------------------------------------------------------------------------
void audio_shutdown(audio_data_t *audio)
{
    SDL_PauseAudio(1);
    SDL_CloseAudio();
    SAFE_FREE(audio->buffer);
}

