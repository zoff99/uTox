#include "utox.h"

#include "avatar.h"
#include "commands.h"
#include "debug.h"
#include "file_transfers.h"
#include "filesys.h"
#include "flist.h"
#include "friend.h"
#include "groups.h"
#include "settings.h"
#include "tox.h"

#include "av/utox_av.h"
#include "av/video.h"
#include "ui/dropdown.h"
#include "ui/edit.h"
#include "ui/tooltip.h"

#include "layout/friend.h"
#include "layout/settings.h"

// TODO including native.h files should never be needed, refactor filesys.h to provide necessary API
#include "native/filesys.h"
#include "native/notify.h"
#include "native/ui.h"
#include "native/video.h"

#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

int global_show_mouse_cursor = 0;
int global_utox_max_desktop_capture_width = 1920;
int global_utox_max_desktop_capture_height = 1080;

extern int count_video_frames_x11_messages;
extern sem_t sem_video_frames_x11_msgs;

extern int global_decoder_video_bitrate;
extern int global_encoder_video_bitrate;
extern int global_network_round_trip_ms;


/** Translates status code to text then sends back to the user */
static void file_notify(FRIEND *f, MSG_HEADER *msg) {
    STRING *str;
    switch (msg->via.ft.file_status) {
        case FILE_TRANSFER_STATUS_NONE: {
            str = SPTR(TRANSFER_NEW);
            break;
        }
        case FILE_TRANSFER_STATUS_ACTIVE: {
            str = SPTR(TRANSFER_STARTED);
            break;
        }
        case FILE_TRANSFER_STATUS_PAUSED_BOTH: {
            str = SPTR(TRANSFER___);
            break;
        }
        case FILE_TRANSFER_STATUS_PAUSED_US:
        case FILE_TRANSFER_STATUS_PAUSED_THEM: {
            str = SPTR(TRANSFER_PAUSED);
            break;
        }
        case FILE_TRANSFER_STATUS_KILLED: {
            str = SPTR(TRANSFER_CANCELLED);
            break;
        }
        case FILE_TRANSFER_STATUS_COMPLETED: {
            str = SPTR(TRANSFER_COMPLETE);
            break;
        }
        case FILE_TRANSFER_STATUS_BROKEN:
        default: { // render unknown status as "transfer broken"
            str = SPTR(TRANSFER_BROKEN);
            break;
        }
    }

    friend_notify_msg(f, str->str, str->length);
}

static void call_notify(FRIEND *f, uint8_t status) {
    STRING *str;
    switch (status) {
        case UTOX_AV_INVITE: {
            str = SPTR(CALL_INVITED);
            break;
        }
        case UTOX_AV_RINGING: {
            str = SPTR(CALL_RINGING);
            break;
        }
        case UTOX_AV_STARTED: {
            str = SPTR(CALL_STARTED);
            break;
        }
        default: { // render unknown status as "call canceled"
            str = SPTR(CALL_CANCELLED);
            break;
        }
    }

    friend_notify_msg(f, str->str, str->length);
}

void utox_message_dispatch(UTOX_MSG utox_msg_id, uint16_t param1, uint16_t param2, void *data) {
    switch (utox_msg_id) {
        /* General core and networking messages */
        case TOX_DONE: {
            /* Does nothing. */
            break;
        }
        case DHT_CONNECTED: {
            /* param1: connection status (1 = connected, 0 = disconnected) */
            tox_connected = param1;
            if (tox_connected) {
                LOG_NOTE("uTox", "Connected to DHT!" );
            } else {
                LOG_NOTE("uTox", "Disconnected from DHT!" );
            }
            redraw();
            break;
        }

        /* OS interaction/integration messages */
        case AUDIO_IN_DEVICE: {
            /* param1: string
             * param2: default device?
             * data: device identifier.
             */
            if (UI_STRING_ID_INVALID == param1) {
                dropdown_list_add_hardcoded(&dropdown_audio_in, data, data);
            } else {
                dropdown_list_add_localized(&dropdown_audio_in, param1, data);
            }

            if (settings.audio_device_in == (uint16_t)~0 && param2) {
                settings.audio_device_in = (dropdown_audio_in.dropcount - 1);
            }

            if (settings.audio_device_in != 0 && (dropdown_audio_in.dropcount - 1) == settings.audio_device_in) {
                postmessage_utoxav(UTOXAV_SET_AUDIO_IN, 0, 0, data);
                dropdown_audio_in.selected = settings.audio_device_in;
                settings.audio_device_in   = 0;
            }
            break;
        }
        case AUDIO_OUT_DEVICE: {
            dropdown_list_add_hardcoded(&dropdown_audio_out, data, data);

            if (settings.audio_device_out != 0 && (dropdown_audio_out.dropcount - 1) == settings.audio_device_out) {
                postmessage_utoxav(UTOXAV_SET_AUDIO_OUT, 0, 0, data);
                dropdown_audio_out.selected = settings.audio_device_out;
                settings.audio_device_out   = 0;
            }

            break;
        }

        /* Client/User Interface messages. */
        case REDRAW: {
            if (param1) {
                ui_rescale(ui_scale);
            } else {
                ui_set_scale(0);
            }
            redraw();
            break;
        }
        case TOOLTIP_SHOW: {
            tooltip_show();
            redraw();
            break;
        }
        case SELF_AVATAR_SET: {
            /* param1: size of data
             * data: png data
             */
            self_set_and_save_avatar(data, param1);
            free(data);
            redraw();
            break;
        }
        case UPDATE_TRAY: {
            update_tray();
            break;
        }
        case PROFILE_DID_LOAD: {
            if (g_select_add_friend_later) {
                g_select_add_friend_later = 0;
                flist_selectaddfriend();
            }
            redraw();
            break;
        }


        /* File transfer messages */

        // data:   FILE_TRANSFER *file
        // param1: uint32_t friend_number
        // param2: uint32_t file_number
        case FILE_SEND_NEW: {
            if (!data) {
                break;
            }

            FRIEND *f = get_friend(param1);
            if (!f) {
                LOG_ERR("uTox", "Could not get friend with number: %u", param1);
                return;
            }

            FILE_TRANSFER *file = data;

            MSG_HEADER *m = message_add_type_file(&f->msg, param2, file->incoming, file->inline_img, file->status,
                                                  file->name, file->name_length,
                                                  file->target_size, file->current_size);
            file_notify(f, m);
            ft_set_ui_data(file->friend_number, file->file_number, m);

            free(data);
            redraw();
            break;
        }

        case FILE_INCOMING_NEW: {
            if (!data) {
                break;
            }

            FRIEND *f = get_friend(param1);
            if (!f) {
                LOG_ERR("uTox", "Could not get friend with number: %u", param1);
                return;
            }

            FILE_TRANSFER *file = data;

            if (f->ft_autoaccept) {
                LOG_TRACE("Toxcore", "Auto Accept enabled for this friend: sending accept to system" );
                native_autoselect_dir_ft(param1, file);
            }

            MSG_HEADER *m = message_add_type_file(&f->msg, (param2 + 1) << 16, file->incoming, file->inline_img,
                                                  file->status, file->name, file->name_length,
                                                  file->target_size, file->current_size);
            file_notify(f, m);
            ft_set_ui_data(file->friend_number, file->file_number, m);

            free(data);
            redraw();
            break;
        }

        case FILE_INCOMING_NEW_INLINE: {
            if (!data) {
                break;
            }

            FRIEND *f = get_friend(param1);
            if (!f) {
                LOG_ERR("uTox", "Could not get friend with number: %u", param1);
                return;
            }

            // Process image data
            uint16_t width, height;
            uint8_t *image;
            memcpy(&width, data, sizeof(uint16_t));
            memcpy(&height, (uint8_t *)data + sizeof(uint16_t), sizeof(uint16_t));
            memcpy(&image, (uint8_t *)data + sizeof(uint16_t) * 2, sizeof(uint8_t *));

            // Save and store image
            friend_recvimage(f, (NATIVE_IMAGE *)image, width, height);

            redraw();
            free(data);
            break;
        }

        case FILE_INCOMING_NEW_INLINE_DONE: {
            if (!data) {
                break;
            }

            FRIEND *f = get_friend(param1);
            if (!f) {
                LOG_ERR("uTox", "Could not get friend with number: %u", param1);
                return;
            }

            FILE_TRANSFER *file = data;

            // Add file transfer message so user can save the inline.
            MSG_HEADER *m = message_add_type_file(&f->msg, param2, file->incoming, file->inline_img, file->status,
                                                  file->name, file->name_length,
                                                  file->target_size, file->current_size);
            file_notify(f, m);
            ft_set_ui_data(file->friend_number, file->file_number, m);

            redraw();
            break;
        }

        case FILE_INCOMING_ACCEPT: {
            postmessage_toxcore(TOX_FILE_ACCEPT, param1, param2 << 16, data);
            break;
        }

        case FILE_STATUS_UPDATE: {
            if (!data) {
                break;
            }

            FILE_TRANSFER *file = data;

            if (file->ui_data) {
                file->ui_data->via.ft.progress    = file->current_size;
                file->ui_data->via.ft.speed       = file->speed;
                file->ui_data->via.ft.file_status = param1;
            }

            free(data);
            redraw();
            break;
        }

        case FILE_STATUS_UPDATE_DATA: {
            if (!data) {
                break;
            }

            FILE_TRANSFER *file = data;

            if (file->ui_data) {
                if (param1 == FILE_TRANSFER_STATUS_COMPLETED) {
                    if (file->in_memory) {
                        file->ui_data->via.ft.data = file->via.memory;
                        file->ui_data->via.ft.data_size = file->current_size;
                    } else {
                        memcpy(file->ui_data->via.ft.path, file->path, UTOX_FILE_NAME_LENGTH);
                    }
                }
            }

            file->decon_wait = false;
            LOG_NOTE("uTox", "FT data was saved" );
            redraw();
            break;
        }

        // data:   MSG_HEADER *ui_data
        // param1: UTOX_FILE_TRANSFER_STATUS file_status
        // File is done, failed or broken.
        case FILE_STATUS_DONE: {
            LOG_INFO("uTox", "FT done. Updating UI.");
            if (!data) {
                LOG_INFO("uTox", "FT done but no data about it.");
                break;
            }

            MSG_HEADER *msg = data;
            msg->via.ft.file_status = param1;

            redraw();
            break;
        }

        /* Friend interaction messages. */
        /* Handshake
         * param1: friend id
         * param2: connection_status */
        case FRIEND_ONLINE: {
            FRIEND *f = get_friend(param1);
            if (f)
            {
                friend_set_online(f, param2);
            }

            redraw();
            messages_send_from_queue(&f->msg, param1);
            break;
        }
        case FRIEND_NAME: {
            FRIEND *f = get_friend(param1);
            if (f)
            {
                friend_setname(f, data, param2);
            }

            redraw();
            free(data);
            break;
        }
        case FRIEND_STATUS_MESSAGE: {
            FRIEND *f = get_friend(param1);
            if (f)
            {
                free(f->status_message);
                f->status_message = NULL;
                f->status_length  = param2;
                f->status_message = data;
            }
            redraw();
            break;
        }
        case FRIEND_STATE: {
            FRIEND *f = get_friend(param1);
            if (f)
            {
                f->status = param2;
            }
            redraw();
            break;
        }
        case FRIEND_AVATAR_SET: {
            /* param1: friend id
             * param2: png size
             * data: png data    */
            uint8_t *avatar = data;
            size_t   size   = param2;

            FRIEND *f = get_friend(param1);
            if (f)
            {
                avatar_set(f->avatar, avatar, size);
                avatar_save(f->id_str, avatar, size);
            }

            free(avatar);
            redraw();
            break;
        }
        case FRIEND_AVATAR_UNSET: {
            FRIEND *f = get_friend(param1);
            if (f)
            {
                avatar_unset(f->avatar);
                // remove avatar from disk
                avatar_delete(f->id_str);
            }

            redraw();
            break;
        }
        /* Interactions */
        case FRIEND_TYPING: {
            FRIEND *f = get_friend(param1);
            if (f)
            {
                friend_set_typing(f, param2);
            }
            redraw();
            break;
        }
        case FRIEND_MESSAGE: {
            // TODO implement notification
            //notify_new(NULL, NULL);
            redraw();
            break;
        }
        case FRIEND_MESSAGE_UPDATE: {
            redraw();
            break;
        }
        /* Adding and deleting */
        case FRIEND_INCOMING_REQUEST: {
            /* data: pointer to FREQUEST structure
             */
            flist_add_frequest(get_frequest(param1));
            redraw();
            break;
        }
        case FRIEND_ACCEPT_REQUEST: {
            /* confirmation that friend has been added to friend list (accept) */
            FREQUEST *req = data;

            FRIEND *f = get_friend(param1);
            if (!f) {
                LOG_ERR("uTox", "Could not get friend with number: %u", param2);
                return;
            }

            flist_add_friend_accepted(f, req);
            flist_reselect_current();
            redraw();
            break;
        }

        case FRIEND_ADD_NO_REQ: {
            /* confirmation that friend has been added to friend list (add) */
            if (param1) {
                /* friend was not added */
                addfriend_status = param2;
            } else {
                /* friend was added */
                edit_add_new_friend_id.length  = 0;
                edit_add_new_friend_msg.length = 0;

                FRIEND *f = get_friend(param2);
                if (!f) {
                    LOG_ERR("uTox", "Could not get friend with number: %u", param2);
                    free(data);
                    return;
                }

                flist_add_friend(f, NULL, 0);
                flist_selectchat(f->number);

                addfriend_status = ADDF_NOFREQUESTSENT;
            }
            free(data);
            redraw();
            break;
        }

        case FRIEND_SEND_REQUEST: {
            /* confirmation that friend has been added to friend list (add) */
            if (param1) {
                /* friend was not added */
                addfriend_status = param2;
            } else {
                /* friend was added */
                FRIEND *f = get_friend(param2);
                if (!f) {
                    edit_add_new_friend_id.length  = 0;
                    edit_add_new_friend_msg.length = 0;
                    LOG_ERR("uTox", "Could not get friend with number: %u", param2);
                    free(data);
                    return;
                }

                memcpy(f->id_bin, data, TOX_PUBLIC_KEY_SIZE);

                char *request_message = strdup(edit_add_new_friend_msg.data);
                if (request_message) {
                    flist_add_friend(f, request_message, edit_add_new_friend_msg.length);
                    free(request_message);
                } else {
                    LOG_ERR("uTox", "Could not allocate memory for request message.");
                }

                flist_selectchat(f->number);
                addfriend_status = ADDF_SENT;

                edit_add_new_friend_id.length  = 0;
                edit_add_new_friend_msg.length = 0;
            }
            free(data);
            redraw();
            break;
        }
        case FRIEND_REMOVE: {
            FRIEND *f = data;
            // commented out in case you have multiple clients in the same data dir
            // and remove one as friend from the other
            //   (it would remove his avatar locally too otherwise)
            // char cid[TOX_PUBLIC_KEY_SIZE * 2];
            // cid_to_string(cid, f->cid);
            // delete_saved_avatar(friend_number);
            if (f)
            {
                friend_free(f);
            }
            break;
        }

        case AV_CALL_INCOMING: {
            FRIEND *f = get_friend(param1);
            if (!f) {
                LOG_ERR("uTox", "Could not get friend with number: %u", param1);
                return;
            }

            sem_wait(&sem_video_frames_x11_msgs);
            count_video_frames_x11_messages = 0;
            sem_post(&sem_video_frames_x11_msgs);

            call_notify(f, UTOX_AV_INVITE);
            redraw();
            break;
        }
        case AV_CALL_RINGING: {
            FRIEND *f = get_friend(param1);
            if (!f) {
                LOG_ERR("uTox", "Could not get friend with number: %u", param1);
                return;
            }

            call_notify(f, UTOX_AV_RINGING);
            redraw();
            break;
        }
        case AV_CALL_ACCEPTED: {
            FRIEND *f = get_friend(param1);
            if (!f) {
                LOG_ERR("uTox", "Could not get friend with number: %u", param1);
                return;
            }

            sem_wait(&sem_video_frames_x11_msgs);
            count_video_frames_x11_messages = 0;
            sem_post(&sem_video_frames_x11_msgs);

            call_notify(f, UTOX_AV_STARTED);
            redraw();
            break;
        }
        case AV_CALL_DISCONNECTED: {

            global_decoder_video_bitrate = 0;
            global_encoder_video_bitrate = 0;
            global_network_round_trip_ms = 999;

            FRIEND *f = get_friend(param1);
            if (!f) {
                LOG_ERR("uTox", "Could not get friend with number: %u", param1);
                return;
            }

            call_notify(f, UTOX_AV_NONE);
            redraw();
            break;
        }
        case AV_VIDEO_FRAME: {
            /* param1: video handle to send frame to ("friendnumber" -> incoming video or "UINT16_MAX" -> preview)
               param2: self preview frame for pending call.
               data: packaged frame data */

            bool need_resize = 0;
            FRIEND *f = get_friend(param1);
            if (f) {
                if (f->video_changed_size == 1)
                {
                    f->video_changed_size = 0;
                    need_resize = 1;
                }
            }

            UTOX_FRAME_PKG *frame = data;
            STRING *s = SPTR(WINDOW_TITLE_VIDEO_PREVIEW);
            // TODO: Don't try to start a new video session every frame.
            video_begin(param1, s->str, s->length, frame->w, frame->h);
            video_frame(param1, frame->img, frame->w, frame->h, need_resize);
            free(frame->img);
            free(data);
            redraw();

            sem_wait(&sem_video_frames_x11_msgs);
            if (count_video_frames_x11_messages > 0)
            {
                count_video_frames_x11_messages--;
            }
            sem_post(&sem_video_frames_x11_msgs);

            break;
        }
        case AV_INLINE_FRAME: {
            redraw();
            break;
        }
        case AV_CLOSE_WINDOW: {
            LOG_INFO("uTox", "Closing video feed" );
            video_end(param1);
            redraw();
            break;
        }
        /* Group chat functions */
        case GROUP_ADD: {
            /* param1: group number
               param2: whether its an av call or not */
            GROUPCHAT *g = get_group(param1);
            if (!g) {
                return;
            }

            flist_add_group(g);
            flist_select_last();
            redraw();
            break;
        }
        case GROUP_MESSAGE: {
            GROUPCHAT *g = get_group(param1);
            if (!g) {
                return;
            }

            GROUPCHAT *selected = flist_get_sel_group();
            if (selected != g) {
                g->unread_msg = true;
            }
            redraw(); // ui_drawmain();

            break;
        }
        case GROUP_PEER_DEL: {
            GROUPCHAT *g = get_group(param1);
            if (!g) {
                return;
            }

            if (g->av_group) {
                g->last_recv_audio[param2]        = g->last_recv_audio[g->peer_count];
                g->last_recv_audio[g->peer_count] = 0;
                group_av_peer_remove(g, param2);
                LOG_INFO("uTox", "GROUP_PEER_DEL:param2 g->peer_count", (int)param2, (int)g->peer_count);
                g->source[param2] = g->source[g->peer_count];
            }

            snprintf((char *)g->topic, sizeof(g->topic), "%u users in chat", g->peer_count);
            g->topic_length = strnlen(g->topic, sizeof(g->topic) - 1);

            redraw();

            break;
        }

        case GROUP_PEER_ADD:
        case GROUP_PEER_NAME:
        case GROUP_PEER_CHANGE: {
            /* param1: group number
             * param2: peer number
             */
            GROUPCHAT *g = get_group(param1);
            if (!g) {
                return;
            }

            snprintf((char *)g->topic, sizeof(g->topic), "%u users in chat", g->peer_count);
            g->topic_length = strnlen(g->topic, sizeof(g->topic) - 1);

            GROUPCHAT *selected = flist_get_sel_group();
            if (selected != g) {
                g->unread_msg = true;
            }

            redraw();
            break;
        }

        case GROUP_TOPIC: {
            GROUPCHAT *g = get_group(param1);
            if (!g) {
                return;
            }

            if (param2 > sizeof(g->name)) {
                memcpy(g->name, data, sizeof(g->name));
                g->name_length = sizeof(g->name);
            } else {
                memcpy(g->name, data, param2);
                g->name_length = param2;
            }

            free(data);
            redraw();
            break;
        }
        case GROUP_AUDIO_START: {
            /* param1: group number
             */
            GROUPCHAT *g = get_group(param1);
            if (!g) {
                LOG_ERR("uTox", "Can't get group %u", param1);
                return;
            }

            if (g->av_group) {
                LOG_INFO("uTox", "We are in an audio group starting call.");
                g->active_call = true;
                postmessage_utoxav(UTOXAV_GROUPCALL_START, param1, 0, NULL);
                redraw();
            }
            break;
        }
        case GROUP_AUDIO_END: {
            /* param1: group number
             */
            GROUPCHAT *g = get_group(param1);
            if (!g) {
                LOG_ERR("uTox", "Can't get group %u", param1);
                return;
            }

            LOG_INFO("uTox", "We are in an audio group ending call.");
            g->active_call = false;
            postmessage_utoxav(UTOXAV_GROUPCALL_END, param1, 0, NULL);
            redraw();
            break;
        }

        case GROUP_UPDATE: {
            redraw();
            break;
        }
    }
}
