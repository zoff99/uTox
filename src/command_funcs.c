#include "command_funcs.h"

#include "friend.h"
#include "groups.h"
#include "debug.h"
#include "tox.h"
#include "macros.h"

#include <stdlib.h>
#include <string.h>

#include <tox/toxav.h>

// ---------------------
#include <stdio.h>
#define CLEAR(x) memset(&(x), 0, sizeof(x))
// ---------------------

#if 0
extern int global__ON_THE_FLY_CHANGES;
extern int global__VPX_RESIZE_ALLOWED;
extern int global__VPX_DROPFRAME_THRESH;
extern int global__VPX_END_RESIZE_UP_THRESH;
extern int global__VPX_END_RESIZE_DOWN_THRESH;
extern int global__MAX_DECODE_TIME_US;
extern int global__MAX_ENCODE_TIME_US;
extern int global__VP8E_SET_CPUUSED_VALUE;
extern int global__VPX_END_USAGE;

extern int UTOX_DEFAULT_BITRATE_V;
extern int UTOX_MIN_BITRATE_VIDEO;
#else
int global__ON_THE_FLY_CHANGES;
int global__VPX_RESIZE_ALLOWED;
int global__VPX_DROPFRAME_THRESH;
int global__VPX_END_RESIZE_UP_THRESH;
int global__VPX_END_RESIZE_DOWN_THRESH;
int global__MAX_DECODE_TIME_US;
int global__MAX_ENCODE_TIME_US;
int global__VP8E_SET_CPUUSED_VALUE;
int global__VPX_END_USAGE;
int global__VPX_KF_MAX_DIST;
int global__VPX_G_LAG_IN_FRAMES;

extern int UTOX_DEFAULT_BITRATE_V;
extern int UTOX_MIN_BITRATE_VIDEO;

int global__VPX_ENCODER_USED;
int global__VPX_DECODER_USED;
int global__SEND_VIDEO_VP9_LOSSLESS_QUALITY;
int global__SEND_VIDEO_LOSSLESS;
int global__SEND_VIDEO_RAW_YUV;
#endif



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
    int number;

    while (!(*str >= '0' && *str <= '9') && (*str != '-') && (*str != '+')) str++;

    if (sscanf(str, "%d", &number) == 1)
    {
        return number;
    }

    // no int found, return default value
    return default_value;
}


bool slash_vbr(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("slash_vbr", "*arg=%s* len=%d" , arg1, arg_length);

	int num_new = get_number_in_string(arg1, (int)UTOX_DEFAULT_BITRATE_V);

	if ((num_new >= 350) && (num_new <= 500000))
	{
		UTOX_DEFAULT_BITRATE_V = num_new;
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

bool slash_maxdist(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("slash_maxdist", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)global__VPX_KF_MAX_DIST);

	if ((num_new >= 1) && (num_new <= 200))
	{
		global__VPX_KF_MAX_DIST = num_new;
		TOXAV_ERR_BIT_RATE_SET error = 0;
		toxav_bit_rate_set(global_toxav, f->number, 64, UTOX_DEFAULT_BITRATE_V, &error);

        if (error)
		{
            LOG_ERR("slash_maxdist", "Setting new Video bitrate has failed with error #%u" , error);
        }
		else
		{
			LOG_ERR("slash_maxdist", "maxdist new:%d", (int)global__VPX_KF_MAX_DIST);
		}
	}

	return true;
}


bool slash_vpxcpu(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("ARG:", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)16);

    TOXAV_ERR_OPTION_SET error;
    toxav_option_set(global_toxav, f->number, TOXAV_ENCODER_CPU_USED, (int32_t)num_new, &error);
    LOG_ERR("ARG:", "vpxcpu new:%d res=%d", (int)num_new, (int)error);

	return true;
}


bool slash_vpxusage(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("ARG:", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)TOXAV_ENCODER_VP8_QUALITY_NORMAL);

    if (num_new > 0)
    {
        TOXAV_ERR_OPTION_SET error;
        toxav_option_set(global_toxav, f->number, TOXAV_ENCODER_VP8_QUALITY, (int32_t)TOXAV_ENCODER_VP8_QUALITY_HIGH, &error);
        LOG_ERR("ARG:", "vpxusage new:TOXAV_ENCODER_VP8_QUALITY_HIGH res=%d", (int)error);
    }
    else
    {
        TOXAV_ERR_OPTION_SET error;
        toxav_option_set(global_toxav, f->number, TOXAV_ENCODER_VP8_QUALITY, (int32_t)TOXAV_ENCODER_VP8_QUALITY_NORMAL, &error);
        LOG_ERR("ARG:", "vpxusage new:TOXAV_ENCODER_VP8_QUALITY_NORMAL res=%d", (int)error);
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

	int num_new = get_number_in_string(arg1, (int)global__VPX_ENCODER_USED);

	if ((num_new >= 0) && (num_new <= 2))
	{
		global__VPX_ENCODER_USED = num_new;

        if (num_new == 2)
        {
            global__SEND_VIDEO_RAW_YUV = 1;
        }
        else
        {
            global__SEND_VIDEO_RAW_YUV = 0;
        }

		TOXAV_ERR_BIT_RATE_SET error = 0;
		toxav_bit_rate_set(global_toxav, f->number, 64, UTOX_DEFAULT_BITRATE_V, &error);

        if (error)
		{
            LOG_ERR("slash_vpxenc", "Setting new Video encoder has failed with error #%u" , error);
        }
		else
		{
			LOG_ERR("slash_vpxenc", "vpxenc new:%d", (int)global__VPX_ENCODER_USED);
		}
	}

	return true;
}

bool slash_vpxloss(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("slash_vpxloss", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)global__SEND_VIDEO_LOSSLESS);

	if ((num_new >= 0) && (num_new <= 1))
	{
		global__SEND_VIDEO_LOSSLESS = num_new;
		TOXAV_ERR_BIT_RATE_SET error = 0;
		toxav_bit_rate_set(global_toxav, f->number, 64, UTOX_DEFAULT_BITRATE_V, &error);

        if (error)
		{
            LOG_ERR("slash_vpxloss", "Setting Video lossy/lossless has failed with error #%u" , error);
        }
		else
		{
			LOG_ERR("slash_vpxloss", "vpxloss new:%d", (int)global__SEND_VIDEO_LOSSLESS);
		}
	}

	return true;
}


bool slash_sza(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("ARG:", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)global__VPX_RESIZE_ALLOWED);

	
	{
		global__VPX_RESIZE_ALLOWED = num_new;
        global__ON_THE_FLY_CHANGES = 1;
        LOG_ERR("ARG:", "sza new:%d", (int)global__VPX_RESIZE_ALLOWED);
	}

	return true;
}


bool slash_rzup(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("ARG:", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)global__VPX_END_RESIZE_UP_THRESH);

	
	{
		global__VPX_END_RESIZE_UP_THRESH = num_new;
        global__ON_THE_FLY_CHANGES = 1;
        LOG_ERR("ARG:", "rzup new:%d", (int)global__VPX_END_RESIZE_UP_THRESH);
	}

	return true;
}

bool slash_rzdn(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("ARG:", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)global__VPX_END_RESIZE_DOWN_THRESH);

	
	{
		global__VPX_END_RESIZE_DOWN_THRESH = num_new;
        global__ON_THE_FLY_CHANGES = 1;
        LOG_ERR("ARG:", "rzdn new:%d", (int)global__VPX_END_RESIZE_DOWN_THRESH);
	}

	return true;
}

bool slash_enctime(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("ARG:", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)global__MAX_ENCODE_TIME_US);

	
	{
		global__MAX_ENCODE_TIME_US = num_new;
        global__ON_THE_FLY_CHANGES = 1;
        LOG_ERR("ARG:", "enctime new:%d", (int)global__MAX_ENCODE_TIME_US);
	}

	return true;
}


bool slash_vbm(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("ARG:", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)UTOX_MIN_BITRATE_VIDEO);
	
	{
		UTOX_MIN_BITRATE_VIDEO = num_new;
        LOG_ERR("ARG:", "vbm new:%d", (int)UTOX_MIN_BITRATE_VIDEO);
	}

	return true;
}


