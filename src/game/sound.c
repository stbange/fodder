#include "sound.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    float *samples;
    int count, offset;
    float pan;
} Clip;

struct FodderSound {
    SDL_AudioStream *stream;
    Clip clips[FODDER_PACK_FILES];
    FodderScreen previous;
    unsigned feedback_tick;
    int attacker;
    bool flank_called, horn_started;
};

static const SDL_AudioSpec mono = {SDL_AUDIO_F32, 1, 22050};
static const SDL_AudioSpec stereo = {SDL_AUDIO_F32, 2, 22050};

static bool load_clip(Clip *clip, const FodderPack *pack, FodderPackFile file)
{
    char path[FODDER_PATH];
    SDL_AudioSpec format;
    Uint8 *source = NULL, *decoded = NULL;
    Uint32 bytes = 0;
    int size = 0;
    if (!fodder_pack_file(pack, file, path) ||
        !SDL_LoadWAV(path, &format, &source, &bytes))
        return false;
    bool valid = bytes > 0 && bytes < 5 * 1024 * 1024 && format.freq >= 8000 &&
                 format.freq <= 96000 && format.channels >= 1 && format.channels <= 2 &&
                 SDL_ConvertAudioSamples(&format, source, (int)bytes, &mono, &decoded, &size);
    SDL_free(source);
    if (!valid)
        return false;
    *clip = (Clip){(float *)decoded, size / (int)sizeof(float), size / (int)sizeof(float), 0.0f};
    return clip->count > 0;
}

FodderSound *fodder_sound_load(const FodderPack *pack, char error[FODDER_ERROR])
{
    FodderSound *sound = calloc(1, sizeof(*sound));
    bool valid = sound != NULL;
    for (int i = FODDER_STRIKE_AUDIO; valid && i < FODDER_PACK_FILES; ++i) {
        if (i < FODDER_SWING_AUDIO || pack->files[i][0])
            valid = load_clip(&sound->clips[i], pack, (FodderPackFile)i);
    }
    if (valid) {
        sound->clips[FODDER_CROWD_AUDIO].offset = 0;
        sound->attacker = -1;
        return sound;
    }
    fodder_sound_free(sound);
    snprintf(error, FODDER_ERROR, "Could not decode the pack's WAV audio.");
    return NULL;
}

void fodder_sound_free(FodderSound *sound)
{
    if (!sound)
        return;
    if (sound->stream)
        SDL_DestroyAudioStream(sound->stream);
    for (int i = FODDER_STRIKE_AUDIO; i < FODDER_PACK_FILES; ++i)
        SDL_free(sound->clips[i].samples);
    free(sound);
}

void fodder_sound_enable(FodderSound *sound)
{
    if (!sound || sound->stream)
        return;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        return;
    sound->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &stereo, NULL, NULL);
    if (sound->stream)
        SDL_ResumeAudioStreamDevice(sound->stream);
}

static float sample(Clip *clip)
{
    return clip->offset < clip->count ? clip->samples[clip->offset++] : 0.0f;
}

static void play(FodderSound *sound, FodderPackFile file, float pan)
{
    sound->clips[file].offset = 0;
    sound->clips[file].pan = pan;
}

static void play_contact(FodderSound *sound, FodderCombatFeedback feedback)
{
    static const FodderPackFile files[] = {
        [FODDER_COMBAT_HIT] = FODDER_HIT_AUDIO,
        [FODDER_COMBAT_MISS] = FODDER_SWING_AUDIO,
        [FODDER_COMBAT_BLOCKED] = FODDER_STRIKE_AUDIO,
        [FODDER_COMBAT_OBSTRUCTED] = FODDER_OBSTRUCTION_AUDIO,
        [FODDER_COMBAT_COUNTER] = FODDER_HIT_AUDIO,
        [FODDER_COMBAT_PARRY] = FODDER_STRIKE_AUDIO,
        [FODDER_COMBAT_GUARDED] = FODDER_STRIKE_AUDIO,
        [FODDER_COMBAT_GUARD_BROKEN] = FODDER_OBSTRUCTION_AUDIO
    };
    FodderPackFile file = files[feedback];
    play(sound, sound->clips[file].count ? file : FODDER_STRIKE_AUDIO, 0.0f);
}

void fodder_sound_frame(FodderSound *sound, const FodderSession *session)
{
    if (!sound->stream)
        return;
    const FodderBattle *battle = &session->battle;
    if (session->screen == FODDER_FIGHT) {
        if (sound->previous == FODDER_CAMP || sound->previous == FODDER_RESULT) {
            play(sound, FODDER_HORN_AUDIO, 0.0f);
            sound->flank_called = false;
        }
        if (battle->attacker >= 0 && battle->attacker != sound->attacker) {
            const FodderSoldier *attacker = &battle->soldiers[battle->attacker];
            FodderPackFile cue = attacker->strike == FODDER_STRIKE_OVERHEAD ? FODDER_OVERHEAD_AUDIO : FODDER_SLASH_AUDIO;
            if (attacker->health > 0.0f)
                play(sound, cue, 0.86f * sinf(fodder_bearing(battle->soldiers, attacker)));
        }
        if (battle->flanker >= 0 && !sound->flank_called) {
            play(sound, FODDER_FLANK_AUDIO, 0.0f);
            sound->flank_called = true;
        }
        if (battle->feedback_tick && battle->feedback_tick != sound->feedback_tick)
            play_contact(sound, battle->feedback);
    }
    if (!sound->horn_started && battle->outcome != FODDER_ACTIVE) {
        play(sound, FODDER_HORN_AUDIO, 0.0f);
        sound->horn_started = true;
    } else if (battle->outcome == FODDER_ACTIVE) {
        sound->horn_started = false;
    }
    sound->attacker = battle->attacker;
    sound->feedback_tick = battle->feedback_tick;
    sound->previous = session->screen;
    for (int fill = 0; fill < 3 && SDL_GetAudioStreamQueued(sound->stream) < 3072; ++fill) {
        float frame[512];
        const float level = session->screen == FODDER_FIGHT ? 0.57f : 0.24f;
        Clip *crowd = &sound->clips[FODDER_CROWD_AUDIO];
        for (unsigned i = 0; i < sizeof(frame) / sizeof(frame[0]); i += 2) {
            if (crowd->offset == crowd->count)
                crowd->offset = 0;
            float ambient = sample(crowd) * level + sample(&sound->clips[FODDER_HORN_AUDIO]) * 0.46f;
            float left = ambient, right = ambient;
            for (int file = FODDER_SWING_AUDIO; file < FODDER_PACK_FILES; ++file) {
                Clip *clip = &sound->clips[file];
                float value = sample(clip) * 0.54f;
                left += value * (1.0f - clip->pan);
                right += value * (1.0f + clip->pan);
            }
            float clash = sample(&sound->clips[FODDER_STRIKE_AUDIO]) * 0.43f;
            frame[i] = fodder_clamp(left + clash, -1.0f, 1.0f);
            frame[i + 1] = fodder_clamp(right + clash, -1.0f, 1.0f);
        }
        SDL_PutAudioStreamData(sound->stream, frame, sizeof(frame));
    }
}
