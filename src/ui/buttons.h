#ifndef UI_BUTTONS_H
#define UI_BUTTONS_H

#include "button.h"

extern BUTTON   button_avatar,
                button_name,
                button_status_msg,
                button_usr_state,

                button_filter_friends,
                button_add_new_contact,

                // button_create_group,

                button_add_new_device_to_self,

                button_change_id_type,
                button_send_friend_request,
                button_call_decline,
                button_call_audio,

                button_call_video,
                button_group_audio,
                button_accept_friend,

                button_send_file,
                button_send_screenshot,
                button_chat_send,

                button_export_chatlog,

                button_move_notify,
                button_notify_create;

bool btn_move_window_down;

void button_setcolors_success(BUTTON *b);
void button_setcolors_danger(BUTTON *b);
void button_setcolors_warning(BUTTON *b);
void button_setcolors_disabled(BUTTON *b);

#endif
