#include "audio.h"

#include "utox_av.h"
#include "filter_audio.h"

#include "../native/audio.h"
#include "../native/keyboard.h"
#include "../native/thread.h"
#include "../native/time.h"

#include "../debug.h"
#include "../friend.h"
#include "../groups.h"
#include "../macros.h"
#include "../main.h" // USER_STATUS_*
#include "../self.h"
#include "../settings.h"
#include "../tox.h"
#include "../utox.h"

#include "../../langs/i18n_decls.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <tox/toxav.h>

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>

#ifdef AUDIO_FILTERING
#include <AL/alext.h>
#endif
/* include for compatibility with older versions of OpenAL */
#ifndef ALC_ALL_DEVICES_SPECIFIER
#include <AL/alext.h>
#endif
#endif

#ifdef AUDIO_FILTERING
#include <filter_audio.h>
#endif

static void utox_filter_audio_kill(Filter_Audio *filter_audio_handle) {
#ifdef AUDIO_FILTERING
    kill_filter_audio(filter_audio_handle);
#else
    (void)filter_audio_handle;
#endif
}

bool utox_audio_thread_init = false;

void draw_audio_bars(int x, int y, int UNUSED(width), int UNUSED(height), int level, int level_med, int level_red, int level_max, int channels);

#define AUDIO_VU_MIN_VALUE -20
#define AUDIO_VU_MED_VALUE 90 // 110
#define AUDIO_VU_RED_VALUE 100 // 120
static float global_audio_in_vu = AUDIO_VU_MIN_VALUE;
static float global_audio_out_vu = AUDIO_VU_MIN_VALUE;


static ALCdevice *audio_out_handle, *audio_in_handle;
static void *     audio_out_device, *audio_in_device;
static bool       speakers_on, microphone_on;
static int16_t    speakers_count, microphone_count;
/* TODO hacky fix. This source list should be a VLA with a way to link sources to friends.
 * NO SRSLY don't leave this like this! */
static ALuint ringtone, preview, notifytone;

static ALuint RingBuffer, ToneBuffer;

static bool audio_in_device_open(void) {
    if (!audio_in_device) {
        return false;
    }
    if (audio_in_device == (void *)1) {
        audio_in_handle = (void *)1;
        return true;
    }

    alGetError();
    int _format_ = AL_FORMAT_STEREO16; // AL_FORMAT_MONO16
    audio_in_handle = alcCaptureOpenDevice(audio_in_device, UTOX_DEFAULT_SAMPLE_RATE_A, _format_,
                                           (UTOX_DEFAULT_FRAME_A * UTOX_DEFAULT_SAMPLE_RATE_A * 4) / 1000);
    if (alGetError() == AL_NO_ERROR) {
        return true;
    }
    return false;
}

static bool audio_in_device_close(void) {
    if (audio_in_handle) {
        if (audio_in_handle == (void *)1) {
            audio_in_handle = NULL;
            microphone_on = false;
            return false;
        }
        if (microphone_on) {
            alcCaptureStop(audio_in_handle);
        }
        alcCaptureCloseDevice(audio_in_handle);
    }
    audio_in_handle = NULL;
    microphone_on = false;
    return false;
}

static bool audio_in_listen(void) {
    if (microphone_on) {
        microphone_count++;
        return true;
    }

    if (audio_in_handle) {
        if (audio_in_device == (void *)1) {
            audio_init(audio_in_handle);
            return true;
        }
        alcCaptureStart(audio_in_handle);
    } else if (audio_in_device) {
        /* Unable to get handle, try to open it again. */
        audio_in_device_open();
        if (audio_in_handle) {
            alcCaptureStart(audio_in_handle);
        } else {
            LOG_TRACE("uTox Audio", "Unable to listen to device!" );
        }
    }

    if (audio_in_handle) {
        microphone_on    = true;
        microphone_count = 1;
        return true;
    }

    microphone_on    = false;
    microphone_count = 0;
    return false;

}

static bool audio_in_ignore(void) {
    if (!microphone_on) {
        return false;
    }

    if (--microphone_count > 0) {
        return true;
    }

    if (audio_in_handle) {
        if (audio_in_handle == (void *)1) {
            audio_close(audio_in_handle);
            microphone_on    = false;
            microphone_count = 0;
            return false;
        }
        alcCaptureStop(audio_in_handle);
    }

    microphone_on = false;
    microphone_count = 0;
    return false;
}

bool utox_audio_in_device_set(ALCdevice *new_device) {
    if (microphone_on || microphone_count) {
        return false;
    }

    if (new_device) {
        audio_in_device = new_device;
        LOG_ERR("uTox Audio", "Audio in device changed." );
        return true;
    }

    audio_in_device = NULL;
    audio_in_handle = NULL;
    LOG_ERR("uTox Audio", "Audio out device set to null." );
    return false;
}

ALCdevice *utox_audio_in_device_get(void) {
    if (audio_in_handle) {
        return audio_in_device;
    }
    return NULL;
}

static ALCcontext *context;

static bool audio_out_device_open(void) {
    if (speakers_on) {
        speakers_count++;
        LOG_ERR("uTox Audio", "speakers_count = %d", (int)speakers_count);
        return true;
    }

    LOG_ERR("uTox Audio", "alcOpenDevice():001:%p", audio_out_device);
    audio_out_handle = alcOpenDevice(audio_out_device);
    if (!audio_out_handle) {
        LOG_ERR("uTox Audio", "alcOpenDevice() failed" );
        speakers_on = false;
        return false;
    }

    LOG_ERR("uTox Audio", "alcCreateContext():001");
    context = alcCreateContext(audio_out_handle, NULL);
    if (!alcMakeContextCurrent(context)) {
        LOG_ERR("uTox Audio", "alcMakeContextCurrent() failed" );
        LOG_ERR("uTox Audio", "alcCloseDevice:001");
        alcCloseDevice(audio_out_handle);
        audio_out_handle = NULL;
        speakers_on = false;
        return false;
    }

    ALint error;
    alGetError(); /* clear errors */
    /* Create the buffers for the ringtone */
    LOG_ERR("uTox Audio", "alGenSources:001:preview");
    alGenSources((ALuint)1, &preview);
    if ((error = alGetError()) != AL_NO_ERROR) {
        LOG_ERR("uTox Audio", "Error generating source with err %x" , error);
        speakers_on = false;
        speakers_count = 0;
        return false;
    }
    /* Create the buffers for incoming audio */
    LOG_ERR("uTox Audio", "alGenSources:002:ringtone");
    alGenSources((ALuint)1, &ringtone);
    if ((error = alGetError()) != AL_NO_ERROR) {
        LOG_ERR("uTox Audio", "Error generating source with err %x" , error);
        speakers_on = false;
        speakers_count = 0;
        return false;
    }
    LOG_ERR("uTox Audio", "alGenSources:003:notifytone");
    alGenSources((ALuint)1, &notifytone);
    if ((error = alGetError()) != AL_NO_ERROR) {
        LOG_ERR("uTox Audio", "Error generating source with err %x" , error);
        speakers_on = false;
        speakers_count = 0;
        return false;
    }

    speakers_on = true;
    speakers_count = 1;
    return true;
}

static bool audio_out_device_close(void) {
    if (!audio_out_handle) {
        return false;
    }

    if (!speakers_on) {
        return false;
    }

    if (--speakers_count > 0) {
        return true;
    }

    LOG_ERR("uTox Audio", "alDeleteSources:001:preview");
    alDeleteSources((ALuint)1, &preview);
    preview = 0;
    LOG_ERR("uTox Audio", "alDeleteSources:002:ringtone");
    alDeleteSources((ALuint)1, &ringtone);
    ringtone = 0;
    LOG_ERR("uTox Audio", "alDeleteSources:003:notifytone");
    alDeleteSources((ALuint)1, &notifytone);
    notifytone = 0;
    alcMakeContextCurrent(NULL);
    LOG_ERR("uTox Audio", "alcDestroyContext:001");
    alcDestroyContext(context);
    LOG_ERR("uTox Audio", "alcCloseDevice:002");
    alcCloseDevice(audio_out_handle);
    audio_out_handle = NULL;
    speakers_on = false;
    speakers_count = 0;
    return false;
}

bool utox_audio_out_device_set(ALCdevice *new_device) {
    if (new_device) {
        audio_out_device = new_device;
        LOG_ERR("uTox Audio", "Audio out device changed." );
        return true;
    }

    audio_out_device = NULL;
    audio_out_handle = NULL;
    LOG_ERR("uTox Audio", "Audio in device set to null." );
    return false;
}

ALCdevice *utox_audio_out_device_get(void) {
    if (audio_out_handle) {
        return audio_out_device;
    }
    return NULL;
}

static float audio_vu(const int16_t *pcm_data, uint32_t sample_count)
{
    float sum = 0.0;

    for (uint32_t i = 0; i < sample_count; i++)
    {
        sum += abs(pcm_data[i]) / 32767.0;
    }

    float vu = 20.0 * logf(sum);
    return vu;
}

void sourceplaybuffer(unsigned int f, const int16_t *data, int samples, uint8_t channels, unsigned int sample_rate) {
    if (!channels || channels > 2) {
        return;
    }

    // calculate audio in level -----------------
    size_t sample_count = (size_t)(samples);
    global_audio_in_vu = AUDIO_VU_MIN_VALUE;

    if (sample_count > 0)
    {
        float vu_value = audio_vu(data, sample_count);

        if (isfinite(vu_value))
        {
            if (vu_value > AUDIO_VU_MIN_VALUE)
            {
                global_audio_in_vu = vu_value;
            }
        }
    }
    // calculate audio in level -----------------

    // draw audio in level -----------------
    draw_audio_bars(1, -8, 10, 10, (int)global_audio_in_vu, AUDIO_VU_MED_VALUE, AUDIO_VU_RED_VALUE, 200, channels);
    // draw audio in level -----------------

    ALuint source;
    if (f >= self.friend_list_size) {
        source = preview;
    } else {
        source = get_friend(f)->audio_dest;
    }

    if (source == 0)
    {
        if (f >= self.friend_list_size) {
            LOG_ERR("uTox Audio", "preview: preview=%d", (int)preview);
            LOG_ERR("uTox Audio", "alGenSources:004:preview");
            alGenSources(1, &preview);
            source = preview;
        } else {
            LOG_ERR("uTox Audio", "normal: source=%d", (int)get_friend(f)->audio_dest);
            LOG_ERR("uTox Audio", "alGenSources:005:audio_dest");
            alGenSources(1, &get_friend(f)->audio_dest);
            source = get_friend(f)->audio_dest;
        }        
    }

    ALuint bufid;
    ALint processed = 0;
    ALint queued = 16;
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
    alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
    // LOG_ERR("uTox Audio", "alSourcei:001");
    alSourcei(source, AL_LOOPING, AL_FALSE);

    if (processed) {
        ALuint bufids[processed];
        alSourceUnqueueBuffers(source, processed, bufids);
        // LOG_ERR("uTox Audio", "alDeleteBuffers:001");
        alDeleteBuffers(processed - 1, bufids + 1);
        bufid = bufids[0];
    } else if (queued < 16) {
        // LOG_ERR("uTox Audio", "alGenBuffers:001");
        alGenBuffers(1, &bufid);
    } else {
        // LOG_ERR("uTox Audio", "dropped audio frame: source=%d", (int)source);
        return;
    }

    // LOG_ERR("uTox Audio", "alBufferData:001");
    alBufferData(bufid, (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, data, samples * 2 * channels,
                 sample_rate);
    alSourceQueueBuffers(source, 1, &bufid);

    // LOG_ERR("uTox Audio", "audio frame || samples == %i channels == %u rate == %u " , samples, channels, sample_rate);

    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) {
        alSourcePlay(source);
        // LOG_ERR("uTox Audio", "Starting source : %d", (int)source);
    }
}

static void audio_in_init(void) {
    const char *audio_in_device_list;
    audio_in_device_list = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
    if (audio_in_device_list) {
        audio_in_device = (void *)audio_in_device_list;
        LOG_ERR("uTox Audio", "input device list:" );
        while (*audio_in_device_list) {
            LOG_ERR("uTox Audio", "\t%s" , audio_in_device_list);
            postmessage_utox(AUDIO_IN_DEVICE, UI_STRING_ID_INVALID, 0, (void *)audio_in_device_list);
            audio_in_device_list += strlen(audio_in_device_list) + 1;
        }
    }
    postmessage_utox(AUDIO_IN_DEVICE, STR_AUDIO_IN_NONE, 0, NULL);
    audio_detect(); /* Get audio devices for windows */
}

static void audio_out_init(void) {
    const char *audio_out_device_list;
    if (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT")) {
        audio_out_device_list = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
    } else {
        audio_out_device_list = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    }

    if (audio_out_device_list) {
        audio_out_device = (void *)audio_out_device_list;
        LOG_ERR("uTox Audio", "output device list:" );
        while (*audio_out_device_list) {
            LOG_ERR("uTox Audio", "\t%s" , audio_out_device_list);
            postmessage_utox(AUDIO_OUT_DEVICE, 0, 0, (void *)audio_out_device_list);
            audio_out_device_list += strlen(audio_out_device_list) + 1;
        }
    }

    LOG_ERR("uTox Audio", "alcOpenDevice():002:%p", audio_out_device);
    audio_out_handle = alcOpenDevice(audio_out_device);
    if (!audio_out_handle) {
        LOG_TRACE("uTox Audio", "alcOpenDevice() failed" );
        return;
    }

    int attrlist[] = { ALC_FREQUENCY, UTOX_DEFAULT_SAMPLE_RATE_A, ALC_INVALID };

    LOG_ERR("uTox Audio", "alcCreateContext():002");
    context = alcCreateContext(audio_out_handle, attrlist);
    if (!alcMakeContextCurrent(context)) {
        LOG_TRACE("uTox Audio", "alcMakeContextCurrent() failed" );
        LOG_ERR("uTox Audio", "alcCloseDevice:003");
        alcCloseDevice(audio_out_handle);
        return;
    }

    // ALint error;
    // alGetError(); /* clear errors */

    // alGenSources((ALuint)1, &ringtone);
    // if ((error = alGetError()) != AL_NO_ERROR) {
    //     LOG_TRACE("uTox Audio", "Error generating source with err %x" , error);
    //     return;
    // }

    // alGenSources((ALuint)1, &preview);
    // if ((error = alGetError()) != AL_NO_ERROR) {
    //     LOG_TRACE("uTox Audio", "Error generating source with err %x" , error);
    //     return;
    // }

    LOG_ERR("uTox Audio", "alcCloseDevice:004");
    alcCloseDevice(audio_out_handle);
}

static bool audio_source_init(ALuint *source) {
    ALint error;
    alGetError();
    LOG_ERR("uTox Audio", "alGenSources:006:audio_source_init");
    alGenSources((ALuint)1, source);
    if ((error = alGetError()) != AL_NO_ERROR) {
        LOG_ERR("uTox Audio", "Error generating source with err %x" , error);
        return false;
    }
    return true;
}

static void audio_source_raze(ALuint *source) {
    LOG_INFO("Audio", "Deleting source");
    LOG_ERR("uTox Audio", "alDeleteSources:004:source");
    alDeleteSources((ALuint)1, source);
}

// clang-format off
enum {
    NOTE_none,
    NOTE_c3_sharp,
    NOTE_g3,
    NOTE_b3,
    NOTE_c4,
    NOTE_a4,
    NOTE_b4,
    NOTE_e4,
    NOTE_f4,
    NOTE_c5,
    NOTE_d5,
    NOTE_e5,
    NOTE_f5,
    NOTE_g5,
    NOTE_a5,
    NOTE_c6_sharp,
    NOTE_e6,
};

static struct {
    uint8_t note;
    double  freq;
} notes[] = {
    {NOTE_none,         1           }, /* Can't be 0 or openal will skip this note/time */
    {NOTE_c3_sharp,     138.59      },
    {NOTE_g3,           196.00      },
    {NOTE_b3,           246.94      },
    {NOTE_c4,           261.63      },
    {NOTE_a4,           440.f       },
    {NOTE_b4,           493.88      },
    {NOTE_e4,           329.63      },
    {NOTE_f4,           349.23      },
    {NOTE_c5,           523.25      },
    {NOTE_d5,           587.33      },
    {NOTE_e5,           659.25      },
    {NOTE_f5,           698.46      },
    {NOTE_g5,           783.99      },
    {NOTE_a5,           880.f       },
    {NOTE_c6_sharp,     1108.73     },
    {NOTE_e6,           1318.51     },
};

static struct melodies { /* C99 6.7.8/10 uninitialized arithmetic types are 0 this is what we want. */
    uint8_t count;
    uint8_t volume;
    uint8_t fade;
    uint8_t notes[8];
} normal_ring[16] = {
    {1, 14, 1, {NOTE_f5,        }},
    {1, 14, 1, {NOTE_f5,        }},
    {1, 14, 1, {NOTE_f5,        }},
    {1, 14, 1, {NOTE_c6_sharp,  }},
    {1, 14, 0, {NOTE_c5,        }},
    {1, 14, 1, {NOTE_c5,        }},
    {0, 0, 0,  {0,  }},
}, friend_offline[4] = {
    {1, 14, 1, {NOTE_c4, }},
    {1, 14, 1, {NOTE_g3, }},
    {1, 14, 1, {NOTE_g3, }},
    {0, 0, 0,  {0, }},
}, friend_online[4] = {
    {1, 14, 0, {NOTE_g3, }},
    {1, 14, 1, {NOTE_g3, }},
    {1, 14, 1, {NOTE_a4, }},
    {1, 14, 1, {NOTE_b4, }},
}, friend_new_msg[8] = {
    {1, 0, 0,  {0, }}, /* 3/8 sec of silence for spammy friends */
    {1, 0, 0,  {0, }},
    {1, 0, 0,  {0, }},
    {1, 9,  0, {NOTE_g5, }},
    {1, 9,  1, {NOTE_g5, }},
    {1, 12, 1, {NOTE_a4, }},
    {1, 10, 1, {NOTE_a4, }},
    {1, 0, 0,  {0, }},
}, friend_request[8] = {
    {1, 9,  0, {NOTE_g5, }},
    {1, 9,  1, {NOTE_g5, }},
    {1, 12, 1, {NOTE_b3, }},
    {1, 10, 1, {NOTE_b3, }},
    {1, 9,  0, {NOTE_g5, }},
    {1, 9,  1, {NOTE_g5, }},
    {1, 12, 1, {NOTE_b3, }},
    {1, 10, 0, {NOTE_b3, }},
};
// clang-format on

typedef struct melodies MELODY;

// TODO: These should be functions rather than macros that only work in a specific context.
#define FADE_STEP_OUT() (1 - ((double)(index % (sample_rate / notes_per_sec)) / (sample_rate / notes_per_sec)))
#define FADE_STEP_IN() (((double)(index % (sample_rate / notes_per_sec)) / (sample_rate / notes_per_sec)))

// GEN_NOTE_RAW is unused. Delete?
#define GEN_NOTE_RAW(x, a) ((a * base_amplitude) * (sin((tau * x) * index / sample_rate)))
#define GEN_NOTE_NUM(x, a) ((a * base_amplitude) * (sin((tau * notes[x].freq) * index / sample_rate)))

#define GEN_NOTE_NUM_FADE(x, a) \
    ((a * base_amplitude * FADE_STEP_OUT()) * (sin((tau * notes[x].freq) * index / sample_rate)))
// GEN_NOTE_NUM_FADE_IN is unused. Delete?
#define GEN_NOTE_NUM_FADE_IN(x, a) \
    ((a * base_amplitude * FADE_STEP_IN()) * (sin((tau * notes[x].freq) * index / sample_rate)))

static void generate_melody(MELODY melody[], uint32_t seconds, uint32_t notes_per_sec, ALuint *target) {
    ALint error;
    alGetError(); /* clear errors */

    LOG_ERR("uTox Audio", "alGenBuffers:002");
    alGenBuffers((ALuint)1, target);

    if ((error = alGetError()) != AL_NO_ERROR) {
        LOG_TRACE("uTox Audio", "Error generating buffer with err %i" , error);
        return;
    }

    const uint32_t sample_rate    = 22000;
    const uint32_t base_amplitude = 1000;
    const double tau = 6.283185307179586476925286766559;

    const size_t buf_size = seconds * sample_rate * 2; // 16 bit (2 bytes per sample)
    int16_t *samples  = calloc(buf_size, sizeof(int16_t));

    if (!samples) {
        LOG_TRACE("uTox Audio", "Unable to generate ringtone buffer!" );
        return;
    }

    for (uint64_t index = 0; index < buf_size; ++index) {
        /* index / sample rate `mod` seconds. will give you full second long notes
         * you can change the length each tone is played by changing notes_per_sec
         * but you'll need to add additional case to cover the entire span of time */
        const int position = ((index / (sample_rate / notes_per_sec)) % (seconds * notes_per_sec));

        for (int i = 0; i < melody[position].count; ++i) {
            if (melody[position].fade) {
                samples[index] += GEN_NOTE_NUM_FADE(melody[position].notes[i], melody[position].volume);
            } else {
                samples[index] += GEN_NOTE_NUM(melody[position].notes[i], melody[position].volume);
            }
        }
    }

    LOG_ERR("uTox Audio", "alBufferData:002");
    alBufferData(*target, AL_FORMAT_MONO16, samples, buf_size, sample_rate);
    free(samples);
}

static void generate_tone_call_ringtone() { generate_melody(normal_ring, 4, 4, &RingBuffer); }

static void generate_tone_friend_offline() { generate_melody(friend_offline, 1, 4, &ToneBuffer); }

static void generate_tone_friend_online() { generate_melody(friend_online, 1, 4, &ToneBuffer); }

static void generate_tone_friend_new_msg() { generate_melody(friend_new_msg, 1, 8, &ToneBuffer); }

static void generate_tone_friend_request() { generate_melody(friend_request, 1, 8, &ToneBuffer); }

void postmessage_audio(uint8_t msg, uint32_t param1, uint32_t param2, void *data) {
    while (audio_thread_msg && utox_audio_thread_init) {
        yieldcpu(1);
    }

    audio_msg.msg    = msg;
    audio_msg.param1 = param1;
    audio_msg.param2 = param2;
    audio_msg.data   = data;

    audio_thread_msg = 1;
}

// TODO: This function is 300 lines long. Cut it up.
void utox_audio_thread(void *args) {
    time_t close_device_time = 0;
    ToxAV *av = args;

    #ifdef AUDIO_FILTERING
    LOG_INFO("uTox Audio", "Audio Filtering"
    #ifdef ALC_LOOPBACK_CAPTURE_SAMPLES
        " and Echo cancellation"
    #endif
        " enabled in this build" );
    #endif
    // bool call[MAX_CALLS] = {0}, preview = 0;

    const int perframe = (UTOX_DEFAULT_FRAME_A * UTOX_DEFAULT_SAMPLE_RATE_A) / 1000;
    uint8_t *buf = calloc(1, (perframe * 2 * UTOX_DEFAULT_AUDIO_CHANNELS) * 10);

    LOG_ERR("uTox Audio", "frame size: %d" , (int)perframe);
    LOG_ERR("uTox Audio", "buf size: %d" , (int)(perframe * 2 * UTOX_DEFAULT_AUDIO_CHANNELS));

    /* init Microphone */
    audio_in_init();
    // audio_in_device_open();
    // audio_in_listen();

    /* init Speakers */
    audio_out_init();
    // audio_out_device_open();
    // audio_out_device_close();

    #define PREVIEW_BUFFER_SIZE (UTOX_DEFAULT_SAMPLE_RATE_A / 2)
    int16_t *preview_buffer = calloc(PREVIEW_BUFFER_SIZE, 2);
    if (!preview_buffer) {
        LOG_ERR("uTox Audio", "Unable to allocate memory for preview buffer.");
        return;
    }
    unsigned int preview_buffer_index = 0;
    bool preview_on = false;

    utox_audio_thread_init = true;
    while (1) {
        if (audio_thread_msg) {
            const TOX_MSG *m = &audio_msg;
            if (m->msg == UTOXAUDIO_KILL) {
                break;
            }

            int call_ringing = 0;
            switch (m->msg) {
                case UTOXAUDIO_CHANGE_MIC: {
                    while (audio_in_ignore()) { continue; }
                    LOG_ERR("uTox Audio", "audio_in_device_close:001:while");
                    while (audio_in_device_close())
                    {
                        LOG_ERR("uTox Audio", "audio_in_device_close:001");
                        continue;
                    }

                    break;
                }
                case UTOXAUDIO_CHANGE_SPEAKER: {
                    LOG_ERR("uTox Audio", "audio_out_device_close:001:while");
                    while (audio_out_device_close())
                    {
                        LOG_ERR("uTox Audio", "audio_out_device_close:001");
                        continue;
                    }

                    break;
                }
                case UTOXAUDIO_START_FRIEND: {
                    LOG_ERR("Audio", "Starting Friend Audio %u", m->param1);

                    audio_out_device_open();

                    FRIEND *f = get_friend(m->param1);
                    if (f && !f->audio_dest) {
                        LOG_ERR("Audio", "Starting Friend Audio %u audio_source_init %d", m->param1, (int)f->audio_dest);
                        audio_source_init(&f->audio_dest);
                        LOG_ERR("Audio", "Starting Friend Audio %u audio_source_init %d -> done", m->param1, (int)f->audio_dest);
                    }
                    audio_in_listen();
                    break;
                }
                case UTOXAUDIO_STOP_FRIEND: {
                    LOG_ERR("Audio", "Stoping Friend Audio %u", m->param1);
                    FRIEND *f = get_friend(m->param1);
                    if (f && f->audio_dest) {
                        LOG_ERR("Audio", "Stoping Friend Audio %u audio_source_init %d", m->param1, (int)f->audio_dest);
                        audio_source_raze(&f->audio_dest);
                        f->audio_dest = 0;
                        LOG_ERR("Audio", "Stoping Friend Audio %u audio_source_init %d -> done", m->param1, (int)f->audio_dest);
                    }
                    audio_in_ignore();
                    LOG_ERR("uTox Audio", "audio_out_device_close:002");
                    audio_out_device_close();
                    break;
                }
                case UTOXAUDIO_GROUPCHAT_START: {
                    LOG_ERR("Audio", "Starting Groupchat Audio %u", m->param1);
                    GROUPCHAT *g = get_group(m->param1);
                    if (!g) {
                        LOG_ERR("uTox Audio", "Could not get group %u", m->param1);
                        break;
                    }

                    if (!g->audio_dest) {
                        audio_source_init(&g->audio_dest);
                    }

                    audio_out_device_open();
                    audio_in_listen();
                    int32_t res = toxav_groupchat_enable_av(toxav_get_tox(av), (uint32_t)g->number, callback_av_group_audio, (void *)NULL);
                    LOG_ERR("Audio", "Starting Groupchat Audio:res=%d", res);
                    break;
                }
                case UTOXAUDIO_GROUPCHAT_STOP: {
                    LOG_ERR("Audio", "Stopping Groupchat Audio %u", m->param1);
                    GROUPCHAT *g = get_group(m->param1);
                    if (!g) {
                        LOG_ERR("uTox Audio", "Could not get group %u", m->param1);
                        break;
                    }

                    TOX_ERR_CONFERENCE_PEER_QUERY error;
                    uint32_t res3 = tox_conference_peer_count(toxav_get_tox(av), (uint32_t)g->number, &error);
                    int32_t res = toxav_groupchat_disable_av(toxav_get_tox(av), (uint32_t)g->number);

                    LOG_ERR("Audio", "Stopping Groupchat Audio:res=%d tox_conference_peer_count:res3=%d", res, res3);

                    if (error == TOX_ERR_CONFERENCE_PEER_QUERY_OK)
                    {
                        if ((res3 > 0) && (res3 < 1000))
                        {
                            for (int jj=1;jj<(int)res3;jj++)
                            {
                                // 0 --> thats us
                                LOG_ERR("Audio", "Deleting source for peer %u in group %u", jj, (uint32_t)g->number);
                                LOG_ERR("uTox Audio", "alDeleteSources:005:g->source[jj]");
                                alDeleteSources(1, &g->source[jj]);
                                // set to zero that its detected as deleted !
                                g->source[jj] = 0;                        
                            }
                        }
                    }

                    if (g->audio_dest) {
                        audio_source_raze(&g->audio_dest);
                        g->audio_dest = 0;
                    }

                    audio_in_ignore();
                    LOG_ERR("uTox Audio", "audio_out_device_close:003");
                    // ** // audio_out_device_close();
                    break;
                }
                case UTOXAUDIO_START_PREVIEW: {
                    preview_on = true;
                    audio_out_device_open();
                    audio_in_listen();
                    break;
                }
                case UTOXAUDIO_STOP_PREVIEW: {
                    preview_on = false;
                    audio_in_ignore();
                    LOG_ERR("uTox Audio", "audio_out_device_close:004");
                    // ** // audio_out_device_close();
                    break;
                }
                case UTOXAUDIO_PLAY_RINGTONE: {
                    if (settings.audible_notifications_enabled && self.status != USER_STATUS_DO_NOT_DISTURB) {
                        LOG_INFO("uTox Audio", "Going to start ringtone!" );

                        audio_out_device_open();

                        generate_tone_call_ringtone();

                        LOG_ERR("uTox Audio", "alSourcei:002");
                        alSourcei(ringtone, AL_LOOPING, AL_TRUE);
                        LOG_ERR("uTox Audio", "alSourcei:003");
                        alSourcei(ringtone, AL_BUFFER, RingBuffer);

                        alSourcePlay(ringtone);
                        call_ringing++;
                    }
                    break;
                }
                case UTOXAUDIO_STOP_RINGTONE: {
                    call_ringing--;
                    LOG_INFO("uTox Audio", "Going to stop ringtone!" );
                    alSourceStop(ringtone);
                    yieldcpu(5);
                    LOG_ERR("uTox Audio", "audio_out_device_close:005");
                    //**// audio_out_device_close();
                    break;
                }
                case UTOXAUDIO_PLAY_NOTIFICATION: {
                    if (settings.audible_notifications_enabled && self.status == USER_STATUS_AVAILABLE) {
                        LOG_INFO("uTox Audio", "Going to start notification tone!" );

                        if (close_device_time <= time(NULL)) {
                            audio_out_device_open();
                        }

                        switch (m->param1) {
                            case NOTIFY_TONE_FRIEND_ONLINE: {
                                generate_tone_friend_online();
                                break;
                            }
                            case NOTIFY_TONE_FRIEND_OFFLINE: {
                                generate_tone_friend_offline();
                                break;
                            }
                            case NOTIFY_TONE_FRIEND_NEW_MSG: {
                                generate_tone_friend_new_msg();
                                break;
                            }
                            case NOTIFY_TONE_FRIEND_REQUEST: {
                                generate_tone_friend_request();
                                break;
                            }
                        }

                        LOG_ERR("uTox Audio", "alSourcei:004");
                        alSourcei(notifytone, AL_LOOPING, AL_FALSE);
                        LOG_ERR("uTox Audio", "alSourcei:005");
                        alSourcei(notifytone, AL_BUFFER, ToneBuffer);

                        alSourcePlay(notifytone);

                        time(&close_device_time);
                        close_device_time += 10;
                        LOG_INFO("uTox Audio", "close device set!" );
                    }
                    break;
                }
                case UTOXAUDIO_STOP_NOTIFICATION: {
                    break;
                }

                case UTOXAUDIO_NEW_AV_INSTANCE: {
                    av = m->data;
                    audio_in_init();
                    audio_out_init();
                }
            }
            audio_thread_msg = 0;

            if (close_device_time && time(NULL) >= close_device_time) {
                LOG_INFO("uTox Audio", "close device triggered!" );
                LOG_ERR("uTox Audio", "audio_out_device_close:006");
                audio_out_device_close();
                close_device_time = 0;
            }
        }

        settings.audio_filtering_enabled = filter_audio_check();

        bool sleep = true;

        if (microphone_on) {
            ALint samples;
            bool frame = 0;
            /* If we have a device_in we're on linux so we can just call OpenAL, otherwise we're on something else so
             * we'll need to call audio_frame() to add to the buffer for us. */
            if (audio_in_handle == (void *)1) {
                frame = audio_frame((void *)buf);
                if (frame) {
                    /* We have an audio frame to use, continue without sleeping. */
                    sleep = false;
                }
            } else {
                alcGetIntegerv(audio_in_handle, ALC_CAPTURE_SAMPLES, sizeof(samples), &samples);
                if (samples >= perframe) {
                    alcCaptureSamples(audio_in_handle, buf, perframe);
                    frame = true;
                    if (samples >= perframe * 2) {
                        sleep = false;
                    }
                }
            }

#if 0
            #ifdef AUDIO_FILTERING
            #ifdef ALC_LOOPBACK_CAPTURE_SAMPLES
            if (f_a && settings.audio_filtering_enabled) {
                alcGetIntegerv(audio_out_device, ALC_LOOPBACK_CAPTURE_SAMPLES, sizeof(samples), &samples);
                if (samples >= perframe) {
                    int16_t buffer[perframe];
                    alcCaptureSamplesLoopback(audio_out_handle, buffer, perframe);
                    pass_audio_output(f_a, buffer, perframe);
                    set_echo_delay_ms(f_a, UTOX_DEFAULT_FRAME_A);
                    if (samples >= perframe * 2) {
                        sleep = false;
                    }
                }
            }
            #endif
            #endif
#endif

            if (frame) {
                bool voice = true;
                #ifdef AUDIO_FILTERING__XXX
                if (f_a) {
                    const int ret = filter_audio(f_a, (int16_t *)buf, perframe);

                    if (ret == -1) {
                        LOG_TRACE("uTox Audio", "filter audio error" );
                    }

                    if (ret == 0) {
                        voice = false;
                    }
                }
                #endif

                /* If push to talk, we don't have to do anything */
                if (!check_ptt_key()) {
                    voice = false; // PTT is up, send nothing.
                }

                if (preview_on) {
                    if (preview_buffer_index + perframe > PREVIEW_BUFFER_SIZE) {
                        preview_buffer_index = 0;
                    }
                    sourceplaybuffer(self.friend_list_size, preview_buffer + preview_buffer_index, perframe,
                                     UTOX_DEFAULT_AUDIO_CHANNELS, UTOX_DEFAULT_SAMPLE_RATE_A);
                    if (voice) {
                        memcpy(preview_buffer + preview_buffer_index, buf, perframe * sizeof(int16_t));
                    } else {
                        memset(preview_buffer + preview_buffer_index, 0, perframe * sizeof(int16_t));
                    }
                    preview_buffer_index += perframe;
                }

                if (voice)
                {


                    size_t active_call_count = 0;
                    for (size_t i = 0; i < self.friend_list_count; i++)
                    {
                        if (UTOX_SEND_AUDIO(i))
                        {
                            active_call_count++;
                            TOXAV_ERR_SEND_FRAME error = 0;
                            // LOG_TRACE("uTox Audio", "Sending audio frame!" );

#if 0                            
                            LOG_ERR("uTox Audio", "toxav_audio_send_frame:samples=%d", (int) perframe);
                            if (perframe > 20)
                            {
                                for (int i = 0; i < 20; i++)
                                {
                                    printf("%02X", buf[i]);
                                }
                            }
#endif
                            toxav_audio_send_frame(av, get_friend(i)->number, (const int16_t *)buf, perframe,
                                                   UTOX_DEFAULT_AUDIO_CHANNELS, UTOX_DEFAULT_SAMPLE_RATE_A, &error);

                            if (error == TOXAV_ERR_SEND_FRAME_SYNC)
                            {
                                int tries = 0;
                                for (tries=0;tries<10;tries++)
                                {
                                    toxav_audio_send_frame(av, get_friend(i)->number, (const int16_t *)buf, perframe,
                                                           UTOX_DEFAULT_AUDIO_CHANNELS, UTOX_DEFAULT_SAMPLE_RATE_A, &error);
                                    if (error == TOXAV_ERR_SEND_FRAME_OK)
                                    {
                                        LOG_ERR("uTox Audio", "toxav_audio_send_frame:+OK+:tries=%d", (int) tries);
                                        break;
                                    }
                                }

                                if (error != TOXAV_ERR_SEND_FRAME_OK)
                                {
                                    LOG_ERR("uTox Audio", "toxav_audio_send_frame:ERR:tries=%d", (int) tries);
                                }
                            }

                            if (error != TOXAV_ERR_SEND_FRAME_OK) {
                                if (error == TOXAV_ERR_SEND_FRAME_SYNC) {
                                    LOG_ERR("uTox Audio", "Audio Frame sync error: %d %d" , (int)i, (int)error);
                                }
                                else
                                {
                                    LOG_ERR("uTox Audio", "toxav_send_audio error friend == %lu, error ==  %i" , i, error);
                                }
                            } else {

                                // calculate audio out level -----------------
                                size_t sample_count = (size_t)(perframe);
                                global_audio_out_vu = AUDIO_VU_MIN_VALUE;

                                if (sample_count > 0)
                                {
                                    float vu_value = audio_vu((const int16_t *)buf, sample_count);

                                    if (isfinite(vu_value))
                                    {
                                        if (vu_value > AUDIO_VU_MIN_VALUE)
                                        {
                                            global_audio_out_vu = vu_value;
                                        }
                                    }
                                }
                                // calculate audio out level -----------------

                                // draw audio out level -----------------
                                draw_audio_bars(1, 1, 10, 10, (int)global_audio_out_vu, AUDIO_VU_MED_VALUE, AUDIO_VU_RED_VALUE, 200, UTOX_DEFAULT_AUDIO_CHANNELS);
                                // draw audio out level -----------------

                                // LOG_TRACE("uTox Audio", "Send a frame to friend %i" ,i);
                                if (active_call_count >= UTOX_MAX_CALLS) {
                                    LOG_ERR("uTox Audio", "We're calling more peers than allowed by UTOX_MAX_CALLS, This is a bug" );
                                    break;
                                }
                            }
                        }
                    }

                    Tox *tox = toxav_get_tox(av);
                    uint32_t num_chats = tox_conference_get_chatlist_size(tox);

                    int res_group_audio_send = -1;

                    if (num_chats)
                    {
                        for (size_t i = 0 ; i < num_chats; ++i)
                        {
                            if (get_group(i) && get_group(i)->active_call)
                            {
                                LOG_TRACE("uTox Audio", "Sending audio in groupchat %u", i);
                                res_group_audio_send = toxav_group_send_audio(tox, i, (int16_t *)buf, perframe,
                                                            UTOX_DEFAULT_AUDIO_CHANNELS, UTOX_DEFAULT_SAMPLE_RATE_A);
                            }
                        }

                        if (res_group_audio_send == 0)
                        {
                            // calculate audio out level -----------------
                            size_t sample_count = (size_t)(perframe);
                            global_audio_out_vu = AUDIO_VU_MIN_VALUE;

                            if (sample_count > 0)
                            {
                                float vu_value = audio_vu((const int16_t *)buf, sample_count);

                                if (isfinite(vu_value))
                                {
                                    if (vu_value > AUDIO_VU_MIN_VALUE)
                                    {
                                        global_audio_out_vu = vu_value;
                                    }
                                }
                            }
                            // calculate audio out level -----------------

                            // draw audio out level -----------------
                            draw_audio_bars(1, 1, 10, 10, (int)global_audio_out_vu, AUDIO_VU_MED_VALUE, AUDIO_VU_RED_VALUE, 200, UTOX_DEFAULT_AUDIO_CHANNELS);
                            // draw audio out level -----------------
                        }
                    }


                }
            }
        }

        if (sleep) {
            yieldcpu(8);
        }
    }

    utox_filter_audio_kill(f_a);
    f_a = NULL;

    // missing some cleanup ?
    LOG_ERR("uTox Audio", "alDeleteSources:006:ringtone");
    alDeleteSources(1, &ringtone);
    ringtone = 0;
    LOG_ERR("uTox Audio", "alDeleteSources:007:preview");
    alDeleteSources(1, &preview);
    preview = 0;
    LOG_ERR("uTox Audio", "alDeleteBuffers:002");
    alDeleteBuffers(1, &RingBuffer);

    LOG_ERR("uTox Audio", "audio_in_device_close:002:while");
    while (audio_in_device_close())
    {
        LOG_ERR("uTox Audio", "audio_in_device_close:002");
        continue;
    }

    LOG_ERR("uTox Audio", "audio_out_device_close:007:while");
    while (audio_out_device_close())
    {
        LOG_ERR("uTox Audio", "audio_out_device_close:007");
        continue;
    }

    if (buf)
    {
        free(buf);
    }

    audio_thread_msg       = 0;
    utox_audio_thread_init = false;
    free(preview_buffer);
    LOG_TRACE("uTox Audio", "Clean thread exit!");
}

void callback_av_group_audio(void *UNUSED(tox), uint32_t groupnumber, uint32_t peernumber, const int16_t *pcm, unsigned int samples,
                             uint8_t channels, unsigned int sample_rate, void *UNUSED(userdata))
{
    // LOG_ERR("uTox Audio", "Received audio in groupchat %i from peer %i samples %d", groupnumber, peernumber, (int)samples);

    GROUPCHAT *g = get_group(groupnumber);
    if (!g) {
        LOG_ERR("uTox Audio", "Could not get group with number: %i", groupnumber);
        return;
    }

    if (!g->active_call) {
        // LOG_ERR("uTox Audio", "Packets for inactive call %u", groupnumber);
        return;
    }

    uint64_t time = get_time();

    if (time - g->last_recv_audio[peernumber] > (uint64_t)1 * 1000 * 1000 * 1000) {
        postmessage_utox(GROUP_UPDATE, groupnumber, peernumber, NULL);
    }

    g->last_recv_audio[peernumber] = time;

    if (channels < 1 || channels > 2) {
        LOG_ERR("uTox Audio", "Can't continue, with channel > 2 or < 1.");
        return;
    }

    if (g->muted) {
        LOG_ERR("uTox Audio", "Group %u audio muted.", groupnumber);
        return;
    }

    if (g->source[peernumber] == 0)
    {
        LOG_ERR("uTox Audio", "alGenSources:007:g->source[peernumber]");
        alGenSources(1, &g->source[peernumber]);
    }

    ALuint bufid;
    ALint processed = 0;
    ALint queued = 16;
    alGetSourcei(g->source[peernumber], AL_BUFFERS_PROCESSED, &processed);
    alGetSourcei(g->source[peernumber], AL_BUFFERS_QUEUED, &queued);
    // LOG_ERR("uTox Audio", "alSourcei:006");
    alSourcei(g->source[peernumber], AL_LOOPING, AL_FALSE);

    if (processed)
    {
        ALuint bufids[processed];
        alSourceUnqueueBuffers(g->source[peernumber], processed, bufids);
        // LOG_ERR("uTox Audio", "alDeleteBuffers:003");
        alDeleteBuffers(processed - 1, bufids + 1);
        bufid = bufids[0];
    }
    else if(queued < 16)
    {
        // LOG_ERR("uTox Audio", "alGenBuffers:003");
        alGenBuffers(1, &bufid);
    }
    else
    {
        // LOG_ERR("uTox Audio", "dropped audio frame gid=%i peernum=%i audiosource=%d" , groupnumber, peernumber, (int)g->source[peernumber]);
        return;
    }

    // calculate audio in level (group chat) -----------------
    size_t sample_count = (size_t)(samples);
    global_audio_in_vu = AUDIO_VU_MIN_VALUE;

    if (sample_count > 0)
    {
        float vu_value = audio_vu(pcm, sample_count);

        if (isfinite(vu_value))
        {
            if (vu_value > AUDIO_VU_MIN_VALUE)
            {
                global_audio_in_vu = vu_value;
            }
        }
    }
    // calculate audio in level (group chat) -----------------

    // draw audio in level (group chat) -----------------
    // LOG_ERR("uTox Audio", "icoming audio frame %i %i" , groupnumber, peernumber);
    draw_audio_bars(1, -1, 10, 10, (int)global_audio_in_vu, AUDIO_VU_MED_VALUE, AUDIO_VU_RED_VALUE, 200, channels);
    // draw audio in level (group chat) -----------------


    // LOG_ERR("uTox Audio", "alBufferData:003");
    alBufferData(bufid, (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, pcm, samples * 2 * channels,
                    sample_rate);
    alSourceQueueBuffers(g->source[peernumber], 1, &bufid);

    ALint state;
    alGetSourcei(g->source[peernumber], AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) {
        alSourcePlay(g->source[peernumber]);
        // LOG_ERR("uTox Audio", "Starting source %i %i" , groupnumber, peernumber);
    }
}

void group_av_peer_add(GROUPCHAT *g, int peernumber) {
    if (!g || peernumber < 0) {
        LOG_ERR("uTox Audio", "Invalid groupchat or peer number");
        return;
    }

    LOG_ERR("uTox Audio", "Adding source for peer %u in group %u", peernumber, g->number);
    LOG_ERR("uTox Audio", "alGenSources:008:g->source[peernumber]");
    alGenSources(1, &g->source[peernumber]);
}

void group_av_peer_remove(GROUPCHAT *g, int peernumber) {
    if (!g || peernumber < 0) {
        LOG_ERR("uTox Audio", "Invalid groupchat or peer number");
        return;
    }

    LOG_ERR("uTox Audio", "Deleting source for peer %u in group %u", peernumber, g->number);
    LOG_ERR("uTox Audio", "alDeleteSources:008:g->source[peernumber]");
    alDeleteSources(1, &g->source[peernumber]);
    // set to zero that its detected as deleted !
    g->source[peernumber] = 0;
}
