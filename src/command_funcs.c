#include "command_funcs.h"

#include "friend.h"
#include "groups.h"
#include "debug.h"
#include "tox.h"
#include <tox/toxav.h>
#include "macros.h"

#include <stdlib.h>
#include <string.h>


#include <tox/toxav.h>

// ---------------------
#include <stdio.h>
#define CLEAR(x) memset(&(x), 0, sizeof(x))
// ---------------------

bool slash_send_file(void *object, char *filepath, int UNUSED(arg_length)) {
    if (filepath) {
        FRIEND *f = object;
        LOG_TRACE("slash_send_file", "File path is: %s" , filepath);
        postmessage_toxcore(TOX_FILE_SEND_NEW_SLASH, f->number, 0xFFFF, (void *)filepath);
        return true;
    }

    LOG_ERR("slash_send_file", " filepath was NULL.");
    return false;
}

bool slash_device(void *object, char *arg, int UNUSED(arg_length)) {
    FRIEND *f =  object;
    uint8_t id[TOX_ADDRESS_SIZE * 2];
    string_to_id(id, arg);
    void *data = malloc(TOX_ADDRESS_SIZE * sizeof(char));

    if (data) {
        memcpy(data, id, TOX_ADDRESS_SIZE);
        postmessage_toxcore(TOX_FRIEND_NEW_DEVICE, f->number, 0, data);
        return true;
    }
    LOG_ERR("slash_device", " Could not allocate memory.");
    return false;
}


bool slash_alias(void *object, char *arg, int arg_length) {
    FRIEND *f =  object;
    if (arg) {
        friend_set_alias(f, (uint8_t *)arg, arg_length);
    } else {
        friend_set_alias(f, NULL, 0);
    }

    utox_write_metadata(f);
    return true;
}

bool slash_invite(void *object, char *arg, int UNUSED(arg_length)) {
    GROUPCHAT *g =  object;
    FRIEND *f = find_friend_by_name((uint8_t *)arg);
    if (f != NULL && f->online) {
        postmessage_toxcore(TOX_GROUP_SEND_INVITE, g->number, f->number, NULL);
        return true;
    }
    return false;
}

bool slash_topic(void *object, char *arg, int arg_length) {
    GROUPCHAT *g = object;
    void *d = malloc(arg_length);
    if (d) {
        memcpy(d, arg, arg_length);
        postmessage_toxcore(TOX_GROUP_SET_TOPIC, g->number, arg_length, d);
        return true;
    }
    LOG_ERR("slash_topic", " Could not allocate memory.");
    return false;
}



static int get_number_in_string(const char *str, int default_value)
{
    int number = default_value;
    if (!str)
    {
        return default_value;
    }

    int len = strlen(str);
    int pos = 0;

    while (!(*str >= '0' && *str <= '9') && (*str != '-') && (*str != '+'))
    {
        if (pos >= len)
        {
            return default_value;
        }
        pos++;
        str++;
    }

    if (sscanf(str, "%d", &number) == 1)
    {
        return number;
    }

    // no int found, return default value
    return default_value;
}


#ifdef HAVE_TOXAV_OPTION_SET

#define MAX_OUTGOING_VIDEO_BITRATE 30000

bool slash_vbr(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("slash_vbr", "*arg=%s* len=%d" , arg1, arg_length);

	int num_new = get_number_in_string(arg1, (int)UTOX_DEFAULT_BITRATE_V);

	if ((num_new >= 10) && (num_new <= MAX_OUTGOING_VIDEO_BITRATE))
	{
		UTOX_DEFAULT_BITRATE_V = num_new;
        TOXAV_ERR_OPTION_SET error2;

        toxav_option_set(global_toxav, f->number, TOXAV_ENCODER_VIDEO_BITRATE_AUTOSET, 0, &error2);


		TOXAV_ERR_BIT_RATE_SET error = 0;
		LOG_ERR("slash_vbr", "toxav_bit_rate_set: global_toxav=%p fnum=%d", global_toxav, (int)f->number);
		toxav_bit_rate_set(global_toxav, f->number, 64, UTOX_DEFAULT_BITRATE_V, &error);

        if (error)
		{
            LOG_ERR("slash_vbr", "Setting new Video bitrate has failed with error #%u" , error);
        }
		else
		{
			LOG_ERR("slash_vbr", "vbr new:%d", (int)UTOX_DEFAULT_BITRATE_V);
		}

	}

	return true;
}

extern int global_iter_delay_ms;

bool slash_iter(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("slash_iter", "*arg=%s* len=%d" , arg1, arg_length);

	int num_new = get_number_in_string(arg1, global_iter_delay_ms);

	if ((num_new >= 0) && (num_new <= 3000))
	{
		LOG_ERR("slash_iter", "setting new value:%d", (int)num_new);
        global_iter_delay_ms = num_new;
	}

	return true;
}

extern int global_aviter_delay_ms;

bool slash_aviter(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("slash_aviter", "*arg=%s* len=%d" , arg1, arg_length);

	int num_new = get_number_in_string(arg1, global_aviter_delay_ms);

	if ((num_new >= 0) && (num_new <= 3000))
	{
		LOG_ERR("slash_aviter", "setting new value:%d", (int)num_new);
        global_aviter_delay_ms = num_new;
	}

	return true;
}


extern int global_show_mouse_cursor;

bool slash_mou(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("slash_mou", "*arg=%s* len=%d" , arg1, arg_length);

	int num_new = get_number_in_string(arg1, (int)1);

	if (num_new == 1)
	{
		LOG_ERR("slash_mou", "show_mouse_cursor:1");
        global_show_mouse_cursor = 1;
	}
    else
    {
		LOG_ERR("slash_mou", "show_mouse_cursor:0");
        global_show_mouse_cursor = 0;
    }

	return true;
}


extern int global_utox_max_desktop_capture_width;
extern int global_utox_max_desktop_capture_height;

bool slash_wh(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("slash_wh", "*arg=%s* len=%d" , arg1, arg_length);

	int num_new = get_number_in_string(arg1, (int)1);

	if (num_new == 2)
	{
		LOG_ERR("slash_wh", "max_screen_res:1920x1080");
        global_utox_max_desktop_capture_width = 1920;
        global_utox_max_desktop_capture_height = 1080;
	}
	else if (num_new == 1)
	{
		LOG_ERR("slash_wh", "max_screen_res:1280x720");
        global_utox_max_desktop_capture_width = 1280;
        global_utox_max_desktop_capture_height = 720;
	}
    else
    {
		LOG_ERR("slash_wh", "max_screen_res:640x480");
        global_utox_max_desktop_capture_width = 640;
        global_utox_max_desktop_capture_height = 480;
    }

	return true;
}

bool slash_vpxenc(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("slash_vpxenc", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)0);

	if ((num_new == 0) || (num_new == 2))
	{
        TOXAV_ERR_OPTION_SET error;
        toxav_option_set(global_toxav, f->number, TOXAV_ENCODER_CODEC_USED, (int32_t)num_new, &error);
        LOG_ERR("ARG:", "vpxenc TOXAV_ENCODER_CODEC_USED new:%d res=%d", (int)num_new, (int)error);
	}

	return true;
}


bool slash_vpxxqt(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("ARG:", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)60);

    if (num_new > 0)
    {
        TOXAV_ERR_OPTION_SET error;
        toxav_option_set(global_toxav, f->number, TOXAV_ENCODER_RC_MAX_QUANTIZER,
            (int32_t)num_new, &error);
        LOG_ERR("ARG:", "TOXAV_ENCODER_RC_MAX_QUANTIZER new res=%d", (int)error);
    }

	return true;
}


#endif

