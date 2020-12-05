#ifndef DD__GNU_SOURCE
#define DD__GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "video.h"

#include "utox_av.h"

#include "../friend.h"
#include "../debug.h"
#include "../macros.h"
#include "../self.h"
#include "../settings.h"
#include "../tox.h"
#include "../utox.h"

#include "../native/thread.h"
#include "../native/video.h"

#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <semaphore.h>
#include <string.h>
#include <tox/toxav.h>
#include <vpx/vpx_codec.h>
#include <vpx/vpx_image.h>

bool utox_video_thread_init = false;
uint16_t video_width, video_height, max_video_width, max_video_height;
static uint8_t video_send_t_num = 0;

static void *   video_device[16]     = { NULL }; /* TODO; magic number */
static int16_t  video_device_count   = 0;
static uint32_t video_device_current = 0;
static bool     video_active         = false;

static utox_av_video_frame utox_video_frame;

static bool video_device_status = true;

static vpx_image_t input;

static pthread_mutex_t video_thread_lock;

// --- FPS ---
#include <sys/time.h>
static struct timeval tm_outgoing_video_frames;
uint32_t global_video_out_fps;
#define TIMESPAN_IN_MS_ELEMENTS 10
#define SHOW_FPS_EVERY_X_FRAMES 200
unsigned long long timspan_in_ms[TIMESPAN_IN_MS_ELEMENTS];
uint16_t timspan_in_ms_cur_index = 0;
int32_t showfps_cur_index = 0;
uint32_t sleep_between_frames;

#include <X11/X.h>
#include <X11/Xlib.h>
extern XImage *screen_image;

extern int global_utox_max_desktop_capture_width;
extern int global_utox_max_desktop_capture_height;

sem_t count_video_play_threads;
int count_video_play_threads_int;
#define MAX_VIDEO_PLAY_THREADS 5
sem_t video_play_lock_;

struct video_play_thread_args {
    int w;
    int h;
    uint64_t timspan_in_ms2;
    ToxAV *av;
    struct timeval tt1;
    size_t i;
    uint8_t *y;
    uint8_t *u;
    uint8_t *v;
    int type; // -99 = screen capture, 1 = camera
    int thread_number;
};



void inc_video_t_counter()
{
    sem_wait(&count_video_play_threads);
    count_video_play_threads_int++;
    sem_post(&count_video_play_threads);
}

void dec_video_t_counter()
{
    sem_wait(&count_video_play_threads);
    count_video_play_threads_int--;

    if (count_video_play_threads_int < 0)
    {
        count_video_play_threads_int = 0;
    }

    sem_post(&count_video_play_threads);
}

int get_video_t_counter()
{
    sem_wait(&count_video_play_threads);
    int ret = count_video_play_threads_int;
    sem_post(&count_video_play_threads);
    return ret;
}

static inline void __utimer_start(struct timeval* tm1)
{
    gettimeofday(tm1, NULL);
}

static inline unsigned long long __utimer_stop(struct timeval* tm1)
{
    struct timeval tm2;
    gettimeofday(&tm2, NULL);

    unsigned long long t = 1000 * (tm2.tv_sec - tm1->tv_sec) + (tm2.tv_usec - tm1->tv_usec) / 1000;
	return t;
}
// --- FPS ---

// gives a counter value that increaes every millisecond
static uint64_t current_time_monotonic_default()
{
    uint64_t time = 0;
    struct timespec clock_mono;
    clock_gettime(CLOCK_MONOTONIC, &clock_mono);
    time = 1000ULL * clock_mono.tv_sec + (clock_mono.tv_nsec / 1000000ULL);
    return time;
}

static int get_policy(char p, int *policy)
{
    switch (p)
    {
        case 'f':
            *policy = SCHED_FIFO;
            return 1;

        case 'r':
            *policy = SCHED_RR;
            return 1;

        case 'b':
            *policy = SCHED_BATCH;
            return 1;

        case 'o':
            *policy = SCHED_OTHER;
            return 1;

        default:
            return 0;
    }
}

static void display_sched_attr(char *msg, int policy, struct sched_param *param)
{
    LOG_ERR("uToxVideo", "%s:policy=%s, priority=%d", msg,
        (policy == SCHED_FIFO)  ? "SCHED_FIFO" :
        (policy == SCHED_RR)    ? "SCHED_RR" :
        (policy == SCHED_BATCH) ? "SCHED_BATCH" :
        (policy == SCHED_OTHER) ? "SCHED_OTHER" :
        "???",
        param->sched_priority);
}

static void display_thread_sched_attr(char *msg)
{
    int policy, s;
    struct sched_param param;
    s = pthread_getschedparam(pthread_self(), &policy, &param);

    if (s != 0)
    {
        LOG_ERR("uToxVideo", "error in display_thread_sched_attr");
    }

    display_sched_attr(msg, policy, &param);
}

static bool video_device_init(void *handle) {

    // initialize video (will populate video_width and video_height)
    if (handle == (void *)1) {
        if (!native_video_init((void *)1)) {
            LOG_ERR("uToxVideo", "native_video_init() failed for desktop" );
            return false;
        }
    } else {
        if (!handle || !native_video_init(*(void **)handle)) {
            LOG_ERR("uToxVideo", "native_video_init() failed webcam" );
            return false;
        }
    }

    if (sem_init(&video_play_lock_, 0, 1))
    {
        LOG_ERR("uToxVideo", "Error in sem_init for video_play_lock_");
    }

    count_video_play_threads_int = 0;
    if (sem_init(&count_video_play_threads, 0, 1))
    {
        LOG_ERR("uToxVideo", "Error in sem_init for count_video_play_threads");
    }

    vpx_img_alloc(&input, VPX_IMG_FMT_I420, video_width, video_height, 1);
    LOG_ERR("uToxVideo", "vpx_img_alloc: %p", (void *)(&input));
    utox_video_frame.y = input.planes[0];
    utox_video_frame.u = input.planes[1];
    utox_video_frame.v = input.planes[2];
    utox_video_frame.w = input.d_w;
    utox_video_frame.h = input.d_h;

    LOG_ERR("uToxVideo", "video init done!" );
    video_device_status = true;

    return true;
}

static void close_video_device(void *handle) {
    if (handle >= (void *)2) {
        native_video_close(*(void **)handle);
        LOG_ERR("close_video_device", "vpx_img_free: %p", (void *)(&input));
        vpx_img_free(&input);
    }
    video_device_status = false;
    sem_destroy(&video_play_lock_);
    sem_destroy(&count_video_play_threads);
}

static bool video_device_start(void) {
    if (video_device_status) {
        native_video_startread();
        video_active = true;
        return true;
    }
    video_active = false;
    return false;
}

static bool video_device_stop(void) {
    if (video_device_status) {
        native_video_endread();
        video_active = false;
        return true;
    }
    video_active = false;
    return false;
}

#include "../ui/dropdown.h"
#include "../layout/settings.h" // TODO move?
void utox_video_append_device(void *device, bool localized, void *name, bool default_) {
    video_device[video_device_count++] = device;

#if 0
    if ((name)&&(!localized))
    {
        LOG_ERR("v4l", "utox_video_append_device:cur=%d name=%s count=%d", (int)video_device_current, name, (int)video_device_count);
    }
    else
    {
        LOG_ERR("v4l", "utox_video_append_device:cur=%d count=%d", (int)video_device_current, (int)video_device_count);
    }
#endif

    if (localized) {
        // Device name is localized with name containing UTOX_I18N_STR.
        // device is device handle pointer.
        dropdown_list_add_localized(&dropdown_video, (UTOX_I18N_STR)name, device);
    } else {
        // Device name is a hardcoded string.
        // device is a pointer to a buffer, that contains device handle pointer,
        // followed by device name string.
        dropdown_list_add_hardcoded(&dropdown_video, name, *(void **)device);
    }

    /* TODO remove all default settings */
    // default == true, if this device will be chosen by video detecting code.
    if (default_) {
        dropdown_video.selected = dropdown_video.over = (dropdown_video.dropcount - 1);
    }
}

bool utox_video_change_device(uint16_t device_number) {
    pthread_mutex_lock(&video_thread_lock);

    static bool _was_active = false;

    if (!device_number) {
        video_device_current = 0;
        if (video_active) {
            video_device_stop();
            LOG_ERR("uToxVideo", "utox_video_change_device:close_video_device:001");
            close_video_device(video_device[video_device_current]);
            if (settings.video_preview) {
                settings.video_preview = false;
                postmessage_utox(AV_CLOSE_WINDOW, 0, 0, NULL);
            }
        }
        LOG_TRACE("uToxVideo", "Disabled Video device (none)" );
        goto mutex_unlock;
    }

    if (video_active) {
        _was_active = true;
        video_device_stop();
        LOG_ERR("uToxVideo", "utox_video_change_device:close_video_device:002");
        close_video_device(video_device[video_device_current]);
    } else {
        _was_active = false;
    }

    video_device_current = device_number;

    if (!video_device_init(video_device[device_number])) {
        goto mutex_unlock;
    }

    if (!_was_active) {
        /* Just grab the new frame size */
        if (video_device_status) {
            close_video_device(video_device[video_device_current]);
        }
        goto mutex_unlock;
    }

    LOG_TRACE("uToxVideo", "Trying to restart video with new device..." );
    if (!video_device_start()) {
        LOG_ERR("uToxVideo", "Error, unable to start new device...");
        if (settings.video_preview) {
            settings.video_preview = false;
            postmessage_utox(AV_CLOSE_WINDOW, 0, 0, NULL);
        }
        goto mutex_unlock;
    }

    pthread_mutex_unlock(&video_thread_lock);
    return true;

    mutex_unlock:
    pthread_mutex_unlock(&video_thread_lock);
    return false;
}

bool utox_video_start(bool preview) {
    if (video_active) {
        LOG_NOTE("uToxVideo", "video already running" );
        return true;
    }

    if (!video_device_current) {
        LOG_NOTE("uToxVideo", "Not starting device None" );
        return false;
    }

    if (preview) {
        settings.video_preview = true;
    }

    if (video_device_init(video_device[video_device_current]) && video_device_start()) {
        video_active = true;
        LOG_NOTE("uToxVideo", "started video" );
        return true;
    }

    LOG_ERR("uToxVideo", "Unable to start video.");
    return false;
}

bool utox_video_stop(bool UNUSED(preview)) {
    if (!video_active) {
        LOG_TRACE("uToxVideo", "video already stopped!" );
        return false;
    }

    video_active           = false;
    settings.video_preview = false;
    postmessage_utox(AV_CLOSE_WINDOW, 0, 0, NULL);

    video_device_stop();
    LOG_ERR("uToxVideo", "utox_video_stop:close_video_device:004");
    close_video_device(video_device[video_device_current]);
    LOG_TRACE("uToxVideo", "stopped video" );
    return true;
}

static TOX_MSG video_msg;
void postmessage_video(uint8_t msg, uint32_t param1, uint32_t param2, void *data) {

    int max_counter = 500;
    int counter = 0;
    while (video_thread_msg) {
        yieldcpu(1);
        counter++;
        if (counter > max_counter)
        {
            LOG_ERR("postmessage_video", "endless loos, caught!!");
            break;
        }
    }

    video_msg.msg    = msg;
    video_msg.param1 = param1;
    video_msg.param2 = param2;
    video_msg.data   = data;

    video_thread_msg = true;
}

// Populates the video device dropdown.
static void init_video_devices(void) {
    // Add always-present null video input device.
#ifdef UTOX_USAGE__HQAV_APPLICATION
    utox_video_append_device(NULL, 1, (void *)STR_VIDEO_IN_NONE, 0);
    LOG_ERR("v4l", "**UTOX_USAGE__HQAV_APPLICATION**");
#else
    utox_video_append_device(NULL, 1, (void *)STR_VIDEO_IN_NONE, 1);
    LOG_ERR("v4l", "UTOX_USAGE normal");
#endif

    // select a video device (autodectect)
    video_device_current = native_video_detect();

    LOG_ERR("v4l", "init_video_devices:cur=%d", (int)video_device_current);

    if (video_device_current) {
        // open the video device to get some info e.g. frame size
        // close it afterwards to not block the device while it is not used
        if (video_device_init(video_device[video_device_current])) {
            LOG_ERR("uToxVideo", "init_video_devices:close_video_device:005");
            close_video_device(video_device[video_device_current]);
        }
    }
}

// that's acutally the thread to send our video
static void *video_play(void *video_frame_data)
{
    // LOG_ERR("uToxVideo:thread", "video SEND thread:start");
    // display_thread_sched_attr("Scheduler attributes of [1]: video_play");

#ifdef VIDEO_SEND_DEBUG_LOGGING
    uint64_t timspan_in_ms444 = current_time_monotonic_default();
#endif

    struct video_play_thread_args* d = (struct video_play_thread_args*)video_frame_data;
    uint64_t timspan_in_ms2 = d->timspan_in_ms2;
    ToxAV *av = d->av;
    struct timeval tt1 = d->tt1;
    size_t i = d->i;
    utox_av_video_frame utox_video_frame;
    utox_video_frame.w = d->w;
    utox_video_frame.h = d->h;
    int type = d->type;

    pthread_setname_np(pthread_self(), "t_v_send");

    if (type == -99)
    {
        if (!screen_image)
        {
            dec_video_t_counter();
            sem_post(&video_play_lock_);
            // LOG_ERR("uToxVideo:thread" , "ret:00");
            pthread_exit(0);
        }

        if (!screen_image->data)
        {
            dec_video_t_counter();
            sem_post(&video_play_lock_);
            // LOG_ERR("uToxVideo:thread" , "ret:01");
            pthread_exit(0);
        }
    }

    // ------ thread priority ------
    struct sched_param param;
    int policy;
    int s;
    get_policy('o', &policy);
    param.sched_priority = strtol("0", NULL, 0);
    s = pthread_setschedparam(pthread_self(), policy, &param);
    // ------ thread priority ------

    if ((utox_video_frame.w < 10) || (utox_video_frame.w > 20000))
    {
        dec_video_t_counter();
        sem_post(&video_play_lock_);
        // LOG_ERR("uToxVideo:thread" , "ret:02");
        pthread_exit(0);
    }

    if ((utox_video_frame.h < 10) || (utox_video_frame.h > 20000))
    {
        dec_video_t_counter();
        sem_post(&video_play_lock_);
        // LOG_ERR("uToxVideo:thread" , "ret:03");
        pthread_exit(0);
    }

    TOXAV_ERR_SEND_FRAME error;
    size_t y_size = (utox_video_frame.w * utox_video_frame.h);
    size_t u_size = ((utox_video_frame.w * utox_video_frame.h) / 4 );
    size_t v_size = ((utox_video_frame.w * utox_video_frame.h) / 4 );
    utox_video_frame.y = malloc(y_size);
    utox_video_frame.u = malloc(u_size);
    utox_video_frame.v = malloc(v_size);

    if (type == 1)
    {
        // LOG_ERR("uToxVideo:thread" , "cpy:-99");
        memcpy(utox_video_frame.y, d->y, y_size);
        memcpy(utox_video_frame.u, d->u, u_size);
        memcpy(utox_video_frame.v, d->v, v_size);
    }
    else
    {
        bgrxtoyuv420(utox_video_frame.y, utox_video_frame.u, utox_video_frame.v,
                    (uint8_t *)screen_image->data, screen_image->width, screen_image->height);
    }

    sem_post(&video_play_lock_);

#ifdef VIDEO_SEND_DEBUG_LOGGING
    LOG_ERR("uToxVideo", "video_send:1:time_spent=%d", (int)(current_time_monotonic_default() - timspan_in_ms444));
#endif

    if (type == 1)
    {
        toxav_video_send_frame_age(av, get_friend(i)->number, utox_video_frame.w, utox_video_frame.h,
                               utox_video_frame.y, utox_video_frame.u, utox_video_frame.v, &error,
                               (int32_t)(current_time_monotonic_default() - timspan_in_ms2));
        // LOG_ERR("uToxVideo:thread", "toxav_video_send_frame_age [-99]: res = %d", error);

        if (error) {
            if (error == TOXAV_ERR_SEND_FRAME_SYNC) {
                yieldcpu(1);
                toxav_video_send_frame_age(av, get_friend(i)->number, utox_video_frame.w, utox_video_frame.h,
                                       utox_video_frame.y, utox_video_frame.u, utox_video_frame.v, &error,
                                       (int32_t)(current_time_monotonic_default() - timspan_in_ms2));
                // LOG_ERR("uToxVideo:thread", "toxav_video_send_frame_age 2 [-99]: res = %d", error);
            }
        }

        free(utox_video_frame.y);
        free(utox_video_frame.u);
        free(utox_video_frame.v);
    }
    else
    {
        // resize into bounding box "w" x "h" if video is larger than that box (e.g. 4K video frame)
        UTOX_FRAME_PKG *frame = malloc(sizeof(UTOX_FRAME_PKG));

        float scale = 1.0f;

        if ((utox_video_frame.w <= global_utox_max_desktop_capture_width) && (utox_video_frame.h <= global_utox_max_desktop_capture_height))
        {
            // scale = 1.0f; // do not scale up!
        }
        else
        {
            float scale_w = (float)(utox_video_frame.w) / (float)(global_utox_max_desktop_capture_width);
            float scale_h = (float)(utox_video_frame.h) / (float)(global_utox_max_desktop_capture_height);

            scale = scale_w;
            if (scale_h > scale_w)
            {
                scale = scale_h;
            }
        }

        uint32_t new_width = (uint32_t)((float)(utox_video_frame.w) / scale);
        uint32_t new_height = (uint32_t)((float)(utox_video_frame.h) / scale);

        if (new_width > global_utox_max_desktop_capture_width)
        {
            new_width = global_utox_max_desktop_capture_width;
        }

        if (new_height > global_utox_max_desktop_capture_height)
        {
            new_height = global_utox_max_desktop_capture_height;
        }

        if (new_width & 1)
        {
            // x264 can not handle odd width
            if (new_width > 2)
            {
                new_width--;
            }
            else
            {
                new_width = 2;
            }
        }

        frame->w              = new_width;
        frame->h              = new_height;
        frame->img            = malloc(((new_width * new_height) * 3 / 2) + 1000 ); // YUV buffer
        void* u_start         = frame->img + (new_width * new_height);
        void* v_start         = frame->img + (new_width * new_height) + ((new_width / 2) * (new_height / 2));

        scale_down_yuv420_image(utox_video_frame.y, utox_video_frame.u, utox_video_frame.v,
                                utox_video_frame.w, utox_video_frame.h,
                                frame->img, u_start, v_start,
                                new_width, new_height);

        toxav_video_send_frame_age(av, get_friend(i)->number, frame->w, frame->h,
                               frame->img, u_start, v_start, &error, (int32_t)(current_time_monotonic_default() - timspan_in_ms2));

        if (error == TOXAV_ERR_SEND_FRAME_SYNC)
        {
            int tries = 0;
            for (tries=0;tries<10;tries++)
            {
                toxav_video_send_frame_age(av, get_friend(i)->number, frame->w, frame->h,
                                       frame->img, u_start, v_start, &error, (int32_t)(current_time_monotonic_default() - timspan_in_ms2));

                if (error == TOXAV_ERR_SEND_FRAME_OK)
                {
                    break;
                }
            }

            if (error != TOXAV_ERR_SEND_FRAME_OK)
            {
                LOG_ERR("uTox Audio", "toxav_video_send_frame_age:ERR:tries=%d", (int) tries);
            }
        }

        free(frame->img);
        free(frame);
        free(utox_video_frame.y);
        free(utox_video_frame.u);
        free(utox_video_frame.v);

        //LOG_ERR("uToxVideo:thread", "ending video play thread: w=%d h=%d", new_width, new_height);

    }

    dec_video_t_counter();

#ifdef VIDEO_SEND_DEBUG_LOGGING
    LOG_ERR("uToxVideo", "video_send:2:time_spent=%d", (int)(current_time_monotonic_default() - timspan_in_ms444));
#endif

    pthread_exit(0);
}

void utox_video_thread(void *args) {
    ToxAV *av = args;

    LOG_ERR("uToxVideo", "utox_video_thread: *********ENTER*********");
    LOG_ERR("uToxVideo", "utox_video_thread: *********ENTER*********");
    LOG_ERR("uToxVideo", "utox_video_thread: *********ENTER*********");
    LOG_ERR("uToxVideo", "utox_video_thread: *********ENTER*********");

    pthread_mutex_init(&video_thread_lock, NULL);

    init_video_devices();

    // ------ thread priority ------
    struct sched_param param;
    int policy;
    int s;
    display_thread_sched_attr("Scheduler attributes of [1]: utox_video_thread");
    get_policy('f', &policy);
    param.sched_priority = strtol("99", NULL, 0);
    s = pthread_setschedparam(pthread_self(), policy, &param);

    if (s != 0)
    {
        LOG_ERR("uToxVideo", "Scheduler attributes of [2]: error setting scheduling attributes of utox_video_thread");
    }

    display_thread_sched_attr("Scheduler attributes of [3]: utox_video_thread");
    // ------ thread priority ------

    pthread_setname_np(pthread_self(), "t_v_record");

    utox_video_thread_init = 1;
    int32_t sleep_delay_corrected = 24; // set to 24ms default sleep delay
    int32_t timspan_current = sleep_delay_corrected;
    uint64_t timspan_in_ms2 = 0;

    while (1) {
        if (video_thread_msg) {
            if (!video_msg.msg || video_msg.msg == UTOXVIDEO_KILL) {
                break;
            }

            switch (video_msg.msg) {
                case UTOXVIDEO_NEW_AV_INSTANCE: {
                    av = video_msg.data;
                    init_video_devices();
                    break;
                }
            }
            video_thread_msg = false;
        }

        if (video_active) {
            pthread_mutex_lock(&video_thread_lock);

            static struct timeval tt1;
            __utimer_start(&tt1);

#ifdef VIDEO_SEND_DEBUG_LOGGING
            LOG_ERR("uToxVideo", "REC:DELTA=%d", (int)(current_time_monotonic_default() - timspan_in_ms2));
#endif
            timspan_in_ms2 = current_time_monotonic_default();
            // capturing is enabled, capture frames
            // LOG_ERR("uToxVideo", "native_video_getframe: START:w=%d h=%d", utox_video_frame.w, utox_video_frame.h);
            const int r = native_video_getframe(utox_video_frame.y, utox_video_frame.u, utox_video_frame.v,
                                                utox_video_frame.w, utox_video_frame.h);
#ifdef VIDEO_SEND_DEBUG_LOGGING
            LOG_ERR("uToxVideo", "native_video_getframe: DONE : %d ms", (int)(current_time_monotonic_default() - timspan_in_ms2));
#endif

            if ((r == 1) || (r == -99)) {

                if (r == 1) // only when using a real video device (not screen capture)
                {
                    if (settings.video_preview) {
                        /* Make a copy of the video frame for uTox to display */
                        UTOX_FRAME_PKG *frame = malloc(sizeof(UTOX_FRAME_PKG));
                        frame->w              = utox_video_frame.w;
                        frame->h              = utox_video_frame.h;
                        frame->img            = malloc(utox_video_frame.w * utox_video_frame.h * 4);

                        yuv420tobgr(utox_video_frame.w, utox_video_frame.h, utox_video_frame.y, utox_video_frame.u,
                                    utox_video_frame.v, utox_video_frame.w, (utox_video_frame.w / 2),
                                    (utox_video_frame.w / 2), frame->img);

                        postmessage_utox(AV_VIDEO_FRAME, UINT16_MAX, 1, (void *)frame);
                    }
                }

                // fprintf(stderr, "CT=%d\n", (int)timspan_in_ms2);
                // LOG_ERR("uToxVideo", "native_video_getframe: OK");

                // ----------- SEND to all friends -----------
                // ----------- SEND to all friends -----------
                // ----------- SEND to all friends -----------
                size_t active_video_count = 0;
                for (size_t i = 0; i < self.friend_list_count; i++) {
                    if (SEND_VIDEO_FRAME(i)) {

                        // LOG_ERR("uToxVideo", "sending video frame to friend %lu,w=%d,h=%d" , i, utox_video_frame.w, utox_video_frame.h);
                        active_video_count++;
                        TOXAV_ERR_SEND_FRAME error = 0;

                        if (get_video_t_counter() < MAX_VIDEO_PLAY_THREADS)
                        {
                            inc_video_t_counter();

#ifdef VIDEO_SEND_DEBUG_LOGGING
                            uint64_t timspan_in_ms44 = current_time_monotonic_default();
#endif
                            sem_wait(&video_play_lock_);
#ifdef VIDEO_SEND_DEBUG_LOGGING
                            LOG_ERR("uToxVideo", "sem_wait=%d", (int)(current_time_monotonic_default() - timspan_in_ms44));
#endif

                            struct video_play_thread_args args2;
                            args2.timspan_in_ms2 = timspan_in_ms2;
                            args2.av = av;
                            args2.tt1 = tt1;
                            args2.i = i;
                            args2.type = r;
                            args2.w = utox_video_frame.w;
                            args2.h = utox_video_frame.h;
                            args2.y = utox_video_frame.y;
                            args2.u = utox_video_frame.u;
                            args2.v = utox_video_frame.v;
                            args2.thread_number = video_send_t_num;

                            pthread_t video_play_thread;
                            int res_ = pthread_create(&video_play_thread, NULL, video_play, (void *)&args2);
                            if (res_ != 0)
                            {
                                dec_video_t_counter();
                                LOG_ERR("uToxVideo:thread", "error creating video play thread ERRNO=%d", res_);
                                sem_post(&video_play_lock_);
                            }
                            else
                            {
                                if (pthread_detach(video_play_thread))
                                {
                                    LOG_ERR("uToxVideo:thread", "error detaching video play thread");
                                }
                                else
                                {
                                    // LOG_ERR("uToxVideo:thread", "creating video play thread number: %d", video_send_t_num);
                                    video_send_t_num++;
                                }
                                sem_wait(&video_play_lock_);
                                sem_post(&video_play_lock_);
                            }
                        }
                        else
                        {
#ifdef VIDEO_SEND_DEBUG_LOGGING
                            LOG_ERR("uToxVideo:thread", "error too many threads");
#endif
                        }

#if 0
                        // LOG_TRACE("uToxVideo", "Sent video frame to friend %u" , i);
                        if (error) {
                            if (error == TOXAV_ERR_SEND_FRAME_SYNC) {
                                LOG_ERR("uToxVideo", "Vid Frame sync error: w=%u h=%u", utox_video_frame.w, utox_video_frame.h);
                            } else if (error == TOXAV_ERR_SEND_FRAME_PAYLOAD_TYPE_DISABLED) {
                                LOG_ERR("uToxVideo",
                                    "ToxAV disagrees with our AV state for friend %lu, self %u, friend %u",
                                        i, get_friend(i)->call_state_self, get_friend(i)->call_state_friend);
                            } else {
                                LOG_ERR("uToxVideo", "toxav_send_video error friend: %i error: %u",
                                        get_friend(i)->number, error);
                            }
                        } else {
                            if (active_video_count >= UTOX_MAX_CALLS) {
                                LOG_ERR("uToxVideo", "Trying to send video frame to too many peers. Please report this bug!");
                                break;
                            }
                        }
#endif

#if 0
#ifdef HAVE_TOXAV_OPTION_SET
                        TOXAV_ERR_OPTION_SET error2;
                        toxav_option_set(av, get_friend(i)->number,
                                         TOXAV_CLIENT_VIDEO_CAPTURE_DELAY_MS,
                                         (int32_t)(current_time_monotonic_default() - timspan_in_ms2),
                                         &error2);
#endif
#endif
                    }
                }
                // ----------- SEND to all friends -----------
                // ----------- SEND to all friends -----------
                // ----------- SEND to all friends -----------
                // --- FPS ----
                // --- FPS ----
                // --- FPS ----
                timspan_in_ms[timspan_in_ms_cur_index] = __utimer_stop(&tm_outgoing_video_frames);
                if (timspan_in_ms[timspan_in_ms_cur_index] > 9999)
                {
                    timspan_in_ms[timspan_in_ms_cur_index] = 24;
                }

                timspan_current = timspan_in_ms[timspan_in_ms_cur_index];

                if ((timspan_in_ms[timspan_in_ms_cur_index] > 0) && (timspan_in_ms[timspan_in_ms_cur_index] < 99999))
                {
                    global_video_out_fps = (int)(1000 / timspan_in_ms[timspan_in_ms_cur_index]);
                }
                else
                {
                    global_video_out_fps = 0;
                }

                timspan_in_ms_cur_index++;
                if (timspan_in_ms_cur_index > (TIMESPAN_IN_MS_ELEMENTS - 1))
                {
                    timspan_in_ms_cur_index = 0;
                }

                showfps_cur_index++;
                if (showfps_cur_index > SHOW_FPS_EVERY_X_FRAMES)
                {
                    showfps_cur_index = 0;
                    LOG_ERR("uToxVideo", "outgoing fps=%d" , global_video_out_fps);
                }

                __utimer_start(&tm_outgoing_video_frames);
                // --- FPS ----
                // --- FPS ----
                // --- FPS ----

            } else if (r == -1) {
                LOG_ERR("uToxVideo", "Err... something really bad happened trying to get this frame, I'm just going "
                            "to plots now!");
                // video_device_stop();
                // LOG_ERR("uToxVideo", "utox_video_thread:close_video_device:006");
                // close_video_device(video_device);
            }
            else
            {
                LOG_ERR("uToxVideo", "Err..: r=%d", (int)r);
            }

            pthread_mutex_unlock(&video_thread_lock);

            //if (r == -99)
            //{

                // --- FPS ----
                // --- FPS ----
                // --- FPS ----
                
                // use fps+1 here to compensate for inaccurate delay values
                sleep_between_frames = (1000 / (settings.video_fps + 1));

                int32_t timspan_average = 0;
                for (int i=0;i < TIMESPAN_IN_MS_ELEMENTS;i++)
                {
                    timspan_average = timspan_average + timspan_in_ms[i];
                }
                timspan_average = timspan_average / TIMESPAN_IN_MS_ELEMENTS;

                // fprintf(stderr, "settings fps=%d sleep_between_frames=%d timspan_average=%d\n" ,
                //     (int)settings.video_fps, (int)sleep_between_frames, (int)timspan_current);
                // fprintf(stderr, "outgoing fps=%d\n" , global_video_out_fps);

                if (timspan_current != (int32_t)sleep_between_frames)
                {
                    int32_t delta_ms = ((int32_t)timspan_current - (int32_t)sleep_between_frames);

                    // fprintf(stderr, "x:%d %d %d\n" ,
                    //     (int)delta_ms, (int)sleep_between_frames, (int)timspan_current);

#if 1
                    if (delta_ms < -20)
                    {
                       delta_ms = -20;
                    }
                    else if (delta_ms > 20)
                    {
                       delta_ms = 20;
                    }
#endif

                    // fprintf(stderr, "0:%d %d\n" , (int)sleep_delay_corrected, (int)delta_ms);
                    sleep_delay_corrected = sleep_delay_corrected - delta_ms;
                    // fprintf(stderr, "1:%d %d\n" , (int)sleep_delay_corrected, (int)delta_ms);

                    if (sleep_delay_corrected < 0)
                    {
                        sleep_delay_corrected = 0;
                    }
                    else if (sleep_delay_corrected > 2000)
                    {
                        sleep_delay_corrected = 2000;
                    }
                }


                // -------- new --------
                int32_t delay_time_frame_sending = (int32_t)(current_time_monotonic_default() - timspan_in_ms2);
                sleep_between_frames = (1000 / settings.video_fps);
                
                if (delay_time_frame_sending >= (int32_t)sleep_between_frames)
                {
                    sleep_delay_corrected = 0;
                }
                else
                {
                    sleep_delay_corrected = (int32_t)sleep_between_frames - delay_time_frame_sending;
                }
                // -------- new --------

                if (sleep_delay_corrected != 0)
                {
                    yieldcpu(sleep_delay_corrected);
                }
                // --- FPS ----
                // --- FPS ----
                // --- FPS ----

            //}
            //else
            //{
                // TODO: make it better
                // real camera, so do NOT sleep here, the camera will block anyway
            //}

            continue;     /* We're running video, so don't sleep for an extra 100 ms */
        }
        else
        {
            // LOG_ERR("uToxVideo", "video_active: false");
        }

        sleep_delay_corrected = 24;
        timspan_current = sleep_delay_corrected;
        yieldcpu(100);
    }

    video_device_count   = 0;
    video_device_current = 0;
    video_active         = false;

    for (uint8_t i = 0; i < 16; ++i) {
        video_device[i] = NULL;
    }

    video_thread_msg       = 0;
    utox_video_thread_init = 0;
    LOG_TRACE("uToxVideo", "Clean thread exit!");
}

void yuv420tobgr(uint16_t width, uint16_t height, const uint8_t *y, const uint8_t *u, const uint8_t *v,
                 unsigned int ystride, unsigned int ustride, unsigned int vstride, uint8_t *out) {
    for (unsigned long int i = 0; i < height; ++i) {
        for (unsigned long int j = 0; j < width; ++j) {
            uint8_t *point = out + 4 * ((i * width) + j);
            int       t_y   = y[((i * ystride) + j)];
            const int t_u   = u[(((i / 2) * ustride) + (j / 2))];
            const int t_v   = v[(((i / 2) * vstride) + (j / 2))];
            t_y            = t_y < 16 ? 16 : t_y;

            const int r = (298 * (t_y - 16) + 409 * (t_v - 128) + 128) >> 8;
            const int g = (298 * (t_y - 16) - 100 * (t_u - 128) - 208 * (t_v - 128) + 128) >> 8;
            const int b = (298 * (t_y - 16) + 516 * (t_u - 128) + 128) >> 8;

            point[2] = r > 255 ? 255 : r < 0 ? 0 : r;
            point[1] = g > 255 ? 255 : g < 0 ? 0 : g;
            point[0] = b > 255 ? 255 : b < 0 ? 0 : b;
            point[3] = ~0;
        }
    }
}

void yuv422to420(uint8_t *plane_y, uint8_t *plane_u, uint8_t *plane_v, uint8_t *input, uint16_t width, uint16_t height) {
    const uint8_t *end = input + width * height * 2;
    while (input != end) {
        uint8_t *line_end = input + width * 2;
        while (input != line_end) {
            *plane_y++ = *input++;
            *plane_v++ = *input++;
            *plane_y++ = *input++;
            *plane_u++ = *input++;
        }

        line_end = input + width * 2;
        while (input != line_end) {
            *plane_y++ = *input++;
            input++; // u
            *plane_y++ = *input++;
            input++; // v
        }
    }
}

static uint8_t rgb_to_y(int r, int g, int b) {
    const int y = ((9798 * r + 19235 * g + 3736 * b) >> 15);
    return y > 255 ? 255 : y < 0 ? 0 : y;
}

static uint8_t rgb_to_u(int r, int g, int b) {
    const int u = ((-5538 * r + -10846 * g + 16351 * b) >> 15) + 128;
    return u > 255 ? 255 : u < 0 ? 0 : u;
}

static uint8_t rgb_to_v(int r, int g, int b) {
    const int v = ((16351 * r + -13697 * g + -2664 * b) >> 15) + 128;
    return v > 255 ? 255 : v < 0 ? 0 : v;
}

void bgrtoyuv420(uint8_t *plane_y, uint8_t *plane_u, uint8_t *plane_v, uint8_t *rgb, uint16_t width, uint16_t height) {
    uint8_t *p;
    uint8_t  r, g, b;

    for (uint16_t y = 0; y != height; y += 2) {
        p = rgb;
        for (uint16_t x = 0; x != width; x++) {
            b          = *rgb++;
            g          = *rgb++;
            r          = *rgb++;
            *plane_y++ = rgb_to_y(r, g, b);
        }

        for (uint16_t x = 0; x != width / 2; x++) {
            b          = *rgb++;
            g          = *rgb++;
            r          = *rgb++;
            *plane_y++ = rgb_to_y(r, g, b);

            b          = *rgb++;
            g          = *rgb++;
            r          = *rgb++;
            *plane_y++ = rgb_to_y(r, g, b);

            b = ((int)b + (int)*(rgb - 6) + (int)*p + (int)*(p + 3) + 2) / 4;
            p++;
            g = ((int)g + (int)*(rgb - 5) + (int)*p + (int)*(p + 3) + 2) / 4;
            p++;
            r = ((int)r + (int)*(rgb - 4) + (int)*p + (int)*(p + 3) + 2) / 4;
            p++;

            *plane_u++ = rgb_to_u(r, g, b);
            *plane_v++ = rgb_to_v(r, g, b);

            p += 3;
        }
    }
}

void bgrxtoyuv420(uint8_t *plane_y, uint8_t *plane_u, uint8_t *plane_v, uint8_t *rgb, uint16_t width, uint16_t height) {
    uint8_t *p;
    uint8_t  r, g, b;

    for (uint16_t y = 0; y != height; y += 2) {
        p = rgb;
        for (uint16_t x = 0; x != width; x++) {
            b = *rgb++;
            g = *rgb++;
            r = *rgb++;
            rgb++;

            *plane_y++ = rgb_to_y(r, g, b);
        }

        for (uint16_t x = 0; x != width / 2; x++) {
            b = *rgb++;
            g = *rgb++;
            r = *rgb++;
            rgb++;

            *plane_y++ = rgb_to_y(r, g, b);

            b = *rgb++;
            g = *rgb++;
            r = *rgb++;
            rgb++;

            *plane_y++ = rgb_to_y(r, g, b);

            b = ((int)b + (int)*(rgb - 8) + (int)*p + (int)*(p + 4) + 2) / 4;
            p++;
            g = ((int)g + (int)*(rgb - 7) + (int)*p + (int)*(p + 4) + 2) / 4;
            p++;
            r = ((int)r + (int)*(rgb - 6) + (int)*p + (int)*(p + 4) + 2) / 4;
            p++;
            p++;

            *plane_u++ = rgb_to_u(r, g, b);
            *plane_v++ = rgb_to_v(r, g, b);

            p += 4;
        }
    }
}

void scale_rgbx_image(uint8_t *old_rgbx, uint16_t old_width, uint16_t old_height, uint8_t *new_rgbx, uint16_t new_width,
                      uint16_t new_height) {
    for (int y = 0; y != new_height; y++) {
        const int y0 = y * old_height / new_height;
        for (int x = 0; x != new_width; x++) {
            const int x0 = x * old_width / new_width;

            const int a         = x + y * new_width;
            const int b         = x0 + y0 * old_width;
            new_rgbx[a * 4]     = old_rgbx[b * 4];
            new_rgbx[a * 4 + 1] = old_rgbx[b * 4 + 1];
            new_rgbx[a * 4 + 2] = old_rgbx[b * 4 + 2];
        }
    }
}

void scale_down_yuv420_image(uint8_t *old_y, uint8_t *old_u, uint8_t *old_v,
                             uint32_t old_width, uint32_t old_height,
                             uint8_t *new_y, uint8_t *new_u, uint8_t *new_v,
                             uint32_t new_width, uint32_t new_height)
{
    // scale down Y layer
    for (int y = 0; y != new_height; y++) {
        const int y0 = y * old_height / new_height;
        for (int x = 0; x != new_width; x++) {
            const int x0 = x * old_width / new_width;

            const int a         = x + y * new_width;
            const int b         = x0 + y0 * old_width;
            new_y[a]            = old_y[b];
        }
    }

    // scale down U + V layer
    for (int y = 0; y != (new_height / 2); y++) {
        const int y0 = y * (old_height / 2) / (new_height / 2);
        for (int x = 0; x != (new_width / 2); x++) {
            const int x0 = x * (old_width / 2) / (new_width / 2);

            const int a         = x + y * (new_width / 2);
            const int b         = x0 + y0 * (old_width / 2);
            new_u[a]            = old_u[b];
            new_v[a]            = old_v[b];
        }
    }

}



