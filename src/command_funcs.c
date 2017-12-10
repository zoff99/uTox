#include "command_funcs.h"

#include "friend.h"
#include "groups.h"
#include "debug.h"
#include "tox.h"
#include "macros.h"

#include <stdlib.h>
#include <string.h>

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

bool slash_fps(void *object, char *arg, int arg_length)
{
	// TODO: dummy for now
	LOG_ERR("slash_fps", "dummy");
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

	LOG_ERR("slash_vpxcpu", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)global__VP8E_SET_CPUUSED_VALUE);

	if ((num_new >= -16) && (num_new <= 16))
	{
		global__VP8E_SET_CPUUSED_VALUE = num_new;
		TOXAV_ERR_BIT_RATE_SET error = 0;
		toxav_bit_rate_set(global_toxav, f->number, 64, UTOX_DEFAULT_BITRATE_V, &error);

        if (error)
		{
            LOG_ERR("slash_vpxcpu", "Setting new Video bitrate has failed with error #%u" , error);
        }
		else
		{
			LOG_ERR("slash_vpxcpu", "vpxcpu new:%d", (int)global__VP8E_SET_CPUUSED_VALUE);
		}
	}

	return true;
}


bool slash_vpxusage(void *object, char *arg, int arg_length)
{
	FRIEND *f = object;

	char arg1[300];
	CLEAR(arg1);
    snprintf(arg1, arg_length, "%s", arg);

	LOG_ERR("slash_vpxusage", "arg=%s" , arg1);

	int num_new = get_number_in_string(arg1, (int)global__VPX_END_USAGE);

	if ((num_new >= 0) && (num_new <= 3))
	{
		global__VPX_END_USAGE = num_new;
		TOXAV_ERR_BIT_RATE_SET error = 0;
		toxav_bit_rate_set(global_toxav, f->number, 64, UTOX_DEFAULT_BITRATE_V, &error);

        if (error)
		{
            LOG_ERR("slash_vpxusage", "Setting new Video bitrate has failed with error #%u" , error);
        }
		else
		{
			LOG_ERR("slash_vpxusage", "vpxusage new:%d", (int)global__VPX_END_USAGE);
		}
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



