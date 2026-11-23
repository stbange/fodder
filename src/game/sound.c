#include "sound.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    float *samples;
    int count, offset;
} Clip;

struct FodderSound {
    SDL_AudioStream *stream;
    Clip strike, horn, crowd;
    FodderScreen previous;
    bool striking, horn_started;
};

static const SDL_AudioSpec mono = {SDL_AUDIO_F32, 1, 22050};

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
    *clip = (Clip){(float *)decoded, size / (int)sizeof(float), size / (int)sizeof(float)};
    return clip->count > 0;
}

FodderSound *fodder_sound_load(const FodderPack *pack, char error[FODDER_ERROR])
{
    FodderSound *sound = calloc(1, sizeof(*sound));
    if (sound && load_clip(&sound->strike, pack, FODDER_STRIKE_AUDIO) &&
        load_clip(&sound->horn, pack, FODDER_HORN_AUDIO) &&
        load_clip(&sound->crowd, pack, FODDER_CROWD_AUDIO)) {
        sound->crowd.offset = 0;
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
    SDL_free(sound->strike.samples);
    SDL_free(sound->horn.samples);
    SDL_free(sound->crowd.samples);
    free(sound);
}

void fodder_sound_enable(FodderSound *sound)
{
    if (!sound || sound->stream)
        return;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        return;
    sound->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &mono, NULL, NULL);
    if (sound->stream)
        SDL_ResumeAudioStreamDevice(sound->stream);
}

static float sample(Clip *clip)
{
    return clip->offset < clip->count ? clip->samples[clip->offset++] : 0.0f;
}

void fodder_sound_frame(FodderSound *sound, const FodderSession *session)
{
    if (!sound->stream)
        return;
    const FodderBattle *battle = &session->battle;
    if (session->screen == FODDER_FIGHT && sound->previous == FODDER_CAMP)
        sound->horn.offset = 0;
    bool striking = battle->soldiers[0].recovery > 0.0f;
    if (striking && !sound->striking)
        sound->strike.offset = 0;
    if (!sound->horn_started && battle->outcome != FODDER_ACTIVE) {
        sound->horn.offset = 0;
        sound->horn_started = true;
    } else if (battle->outcome == FODDER_ACTIVE) {
        sound->horn_started = false;
    }
    sound->striking = striking;
    sound->previous = session->screen;
    for (int fill = 0; fill < 3 && SDL_GetAudioStreamQueued(sound->stream) < 3072; ++fill) {
        float frame[512];
        const float level = session->screen == FODDER_FIGHT ? 0.57f : 0.24f;
        for (unsigned i = 0; i < sizeof(frame) / sizeof(frame[0]); ++i) {
            if (sound->crowd.offset == sound->crowd.count)
                sound->crowd.offset = 0;
            frame[i] = sample(&sound->crowd) * level + sample(&sound->strike) * 0.33f +
                       sample(&sound->horn) * 0.46f;
        }
        SDL_PutAudioStreamData(sound->stream, frame, sizeof(frame));
    }
}
