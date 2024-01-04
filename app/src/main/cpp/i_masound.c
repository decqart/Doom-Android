//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2008 David Flater
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//  System interface for sound.
//

#include <stdio.h>
#include <stdlib.h>

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_RESOURCE_MANAGER
#include "miniaudio.h"

#include "i_sound.h"
#include "i_system.h"
#include "i_swap.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#include "doomtype.h"

#define NUM_CHANNELS 16

typedef struct allocated_sound_s allocated_sound_t;

struct allocated_sound_s {
    ma_sound *sound;
    ma_audio_buffer *audio_buffer; //needed for lifetime
    allocated_sound_t *prev, *next;
};

static boolean sound_initialized = False;

static sfxinfo_t *channels_playing[NUM_CHANNELS];

static boolean use_sfx_prefix = True;

static ma_engine engine;

// Doubly-linked list of allocated sounds.
// When a sound is played, it is moved to the head, so that the oldest
// sounds not used recently are at the tail.
static allocated_sound_t *allocated_sounds_head = NULL;
static allocated_sound_t *allocated_sounds_tail = NULL;
static size_t allocated_sounds_size = 0;

// Hook a sound into the linked list at the head.
static void AllocatedSoundLink(allocated_sound_t *snd)
{
    snd->prev = NULL;

    snd->next = allocated_sounds_head;
    allocated_sounds_head = snd;

    if (allocated_sounds_tail == NULL)
        allocated_sounds_tail = snd;
    else
        snd->next->prev = snd;
}

// Allocate a block for a new sound effect.
static void AllocateSound(sfxinfo_t *sfxinfo, byte *data, size_t len, int sample_rate)
{
    // Allocate the sound structure and data.  The data will immediately
    // follow the structure, which acts as a header.
    allocated_sound_t *snd = malloc(sizeof(allocated_sound_t));
    if (snd == NULL)
        return;

    ma_audio_buffer_config buffer_config = ma_audio_buffer_config_init(
            ma_format_u8,
            1,
            len/2,
            data,
            NULL);
    buffer_config.sampleRate = sample_rate;

    ma_audio_buffer *audio_buffer = malloc(sizeof(ma_audio_buffer));
    if (ma_audio_buffer_init(&buffer_config, audio_buffer) != MA_SUCCESS)
    {
        free(snd);
        return;
    }

    ma_sound *sound = malloc(sizeof(ma_sound));
    ma_result result = ma_sound_init_from_data_source(&engine, audio_buffer, MA_SOUND_FLAG_DECODE, NULL, sound);
    if (result != MA_SUCCESS)
    {
        free(snd);
        return;
    }

    snd->sound = sound;
    snd->audio_buffer = audio_buffer;

    // driver_data pointer points to the allocated_sound structure.
    sfxinfo->driver_data = snd;

    // Keep track of how much memory all these cached sounds are using...
    allocated_sounds_size += len;

    AllocatedSoundLink(snd);
}

// When a sound stops, check if it is still playing.  If it is not,
// we can mark the sound data as CACHE to be freed back for other
// means.
static void ReleaseSoundOnChannel(int channel)
{
    sfxinfo_t *sfxinfo = channels_playing[channel];
    if (sfxinfo == NULL)
        return;

    channels_playing[channel] = NULL;
}

static void GetSfxLumpName(sfxinfo_t *sfx, char *buf, size_t buf_len)
{
    // Linked sfx lumps? Get the lump number for the sound linked to.
    if (sfx->link != NULL)
        sfx = sfx->link;

    // Doom adds a DS* prefix to sound lumps; Heretic and Hexen don't do this.
    if (use_sfx_prefix)
        M_snprintf(buf, buf_len, "ds%s", sfx->name);
    else
        M_StringCopy(buf, sfx->name, buf_len);
}

static void I_MA_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{
    for (int i = 0; i < num_sounds; ++i)
    {
        sfxinfo_t *sfxinfo = &sounds[i];

        // need to load the sound
        char name[9];
        GetSfxLumpName(sfxinfo, name, sizeof(name));
        sfxinfo->lumpnum = W_CheckNumForName(name);
        //printf("sfx->name = %s", sfxinfo->name);
        //printf("sfx->lumpnum = %d", sfxinfo->lumpnum);
        int lumpnum = sfxinfo->lumpnum;
        if (lumpnum == -1)
            continue;

        byte *data = W_CacheLumpNum(lumpnum, PU_STATIC);
        unsigned int lumplen = W_LumpLength(lumpnum);

        // Check the header, and ensure this is a valid sound
        if (lumplen < 8 || data[0] != 0x03 || data[1] != 0x00)
            continue;

        // 16 bit sample rate field, 32 bit length field
        int samplerate = (data[3] << 8) | data[2];
        unsigned int length = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

        // If the header specifies that the length of the sound is greater than
        // the length of the lump itself, this is an invalid sound lump

        // We also discard sound lumps that are less than 49 samples long,
        // as this is how DMX behaves - although the actual cut-off length
        // seems to vary slightly depending on the sample rate.  This needs
        // further investigation to better understand the correct behavior.
        if (length > lumplen - 8 || length <= 48)
            continue;

        // The DMX sound library seems to skip the first 16 and last 16
        // bytes of the lump - reason unknown.
        data += 16;
        length -= 32;

        // Sample rate conversion
        AllocateSound(sfxinfo, data, length, samplerate);

        // don't need the original lump any more
        //W_ReleaseLumpNum(lumpnum);
    }
}

static void I_MA_UpdateSoundParams(int handle, int vol, int sep)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
        return;

    int left = ((254 - sep) * vol) / 127;
    int right = ((sep) * vol) / 127;

    if (left < 0)
        left = 0;
    else if (left > 255)
        left = 255;

    if (right < 0)
        right = 0;
    else if (right > 255)
        right = 255;

    sfxinfo_t *sfxinfo = channels_playing[handle];
    if (sfxinfo == NULL)
        return;

    allocated_sound_t *snd = sfxinfo->driver_data;
    if (snd == NULL)
        return;

    ma_sound_set_volume(snd->sound, (float)vol/64.0f);
    ma_sound_set_pan(snd->sound, (float)(right-left));
}

// Retrieve the raw data lump index for a given SFX name.
static int I_MA_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char name[9];
    GetSfxLumpName(sfx, name, sizeof(name));

    return W_GetNumForName(name);
}

// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
static int I_MA_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    if (!sound_initialized || channel < 0 || channel >= NUM_CHANNELS)
        return -1;

    // Release a sound effect if there is already one playing
    // on this channel
    ReleaseSoundOnChannel(channel);

    allocated_sound_t *snd = sfxinfo->driver_data;
    if (snd == NULL)
        return -1;

    // set separation, etc.
    I_MA_UpdateSoundParams(channel, vol, sep);

    // play sound
    ma_sound_start(snd->sound);

    channels_playing[channel] = sfxinfo;

    return channel;
}

static void I_MA_StopSound(int handle)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
        return;

    sfxinfo_t *sfxinfo = channels_playing[handle];
    if (sfxinfo == NULL)
        return;

    allocated_sound_t *snd = sfxinfo->driver_data;
    if (snd == NULL)
        return;

    ma_sound_stop(snd->sound);

    // Sound data is no longer needed; release the
    // sound data being used for this channel
    ReleaseSoundOnChannel(handle);
}

static boolean I_MA_SoundIsPlaying(int handle)
{
    if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
        return False;

    sfxinfo_t *sfxinfo = channels_playing[handle];
    if (sfxinfo == NULL)
        return False;

    allocated_sound_t *snd = sfxinfo->driver_data;
    if (snd == NULL)
        return False;

    return ma_sound_is_playing(snd->sound);
}

// Periodically called to update the sound system
static void I_MA_UpdateSound(void)
{
    // Check all channels to see if a sound has finished
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
        if (channels_playing[i] && !I_MA_SoundIsPlaying(i))
        {
            // Sound has finished playing on this channel,
            // but sound data has not been released to cache
            ReleaseSoundOnChannel(i);
        }
    }
}

static void I_MA_ShutdownSound(void)
{
    if (!sound_initialized)
        return;

    ma_engine_uninit(&engine);

    sound_initialized = False;
}

static boolean I_MA_InitSound(boolean _use_sfx_prefix)
{
    use_sfx_prefix = _use_sfx_prefix;

    // No sounds yet
    for (int i = 0; i < NUM_CHANNELS; ++i)
        channels_playing[i] = NULL;

    if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
        printf("Failed init audio engine.\n");
        return False;
    }

    sound_initialized = True;

    return True;
}

static snddevice_t sound_ma_devices[] = {
        SNDDEVICE_SB,
        SNDDEVICE_PAS,
        SNDDEVICE_GUS,
        SNDDEVICE_WAVEBLASTER,
        SNDDEVICE_SOUNDCANVAS,
        SNDDEVICE_AWE32
};

sound_module_t sound_ma_module = {
        sound_ma_devices,
        ARRLEN(sound_ma_devices),
        I_MA_InitSound,
        I_MA_ShutdownSound,
        I_MA_GetSfxLumpNum,
        I_MA_UpdateSound,
        I_MA_UpdateSoundParams,
        I_MA_StartSound,
        I_MA_StopSound,
        I_MA_SoundIsPlaying,
        I_MA_PrecacheSounds
};
