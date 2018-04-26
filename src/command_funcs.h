#ifndef COMMAND_FUNCS_H
#define COMMAND_FUNCS_H


#include "tox.h"

#ifndef TOXCOMPAT_H_
#define TOXCOMPAT_H_

#if TOX_VERSION_IS_API_COMPATIBLE(0, 2, 0)
static void toxav_callback_bit_rate_status(ToxAV *av,
     void *callback, void *user_data)
{
        // dummy function
}

#define TOXAV_ERR_BIT_RATE_SET_INVALID_AUDIO_BIT_RATE (TOXAV_ERR_BIT_RATE_SET_INVALID_BIT_RATE)
#define TOXAV_ERR_BIT_RATE_SET_INVALID_VIDEO_BIT_RATE (TOXAV_ERR_BIT_RATE_SET_INVALID_BIT_RATE)

static bool toxav_bit_rate_set(ToxAV *av, uint32_t friend_number, int32_t audio_bit_rate,
int32_t video_bit_rate, TOXAV_ERR_BIT_RATE_SET *error)
{
    bool res = toxav_video_set_bit_rate(av, friend_number, video_bit_rate, error);
    if (*error == TOXAV_ERR_BIT_RATE_SET_INVALID_BIT_RATE)
    {
        *error = TOXAV_ERR_BIT_RATE_SET_INVALID_VIDEO_BIT_RATE;
    }
    
    return res;
}
#else
    // no need to fake the function
#endif

#endif


#include <stdbool.h>

/** slash_send_file()
 *
 * takes a file from the message box, and send it to the current friend.
 *
 * TODO, make sure the file exists.
 */
bool slash_send_file(void *friend_handle, char *filepath, int arg_length);

/**
 * Adds a device to a friend
 */
bool slash_device(void *f, char *argument, int arg_length);

/**
 * Sets the current friend's alias to the value of arg
 */
bool slash_alias(void *f, char *arg, int arg_length);

/**
 * Invites a friend to the current groupchat
 */
bool slash_invite(void *f, char *arg, int arg_length);

/**
 * Sets the topic of the current groupchat to the value of arg
 */
bool slash_topic(void *object, char *arg, int arg_length);


bool slash_vbr(void *object, char *arg, int arg_length);
bool slash_vpxxqt(void *object, char *arg, int arg_length)

#endif
