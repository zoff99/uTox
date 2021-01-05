#include "groups.h"

#include "chatlog.h"
#include "flist.h"
#include "debug.h"
#include "macros.h"
#include "self.h"
#include "settings.h"
#include "text.h"

#include "av/audio.h"
#include "av/utox_av.h"

#include "native/notify.h"

#include "ui/edit.h"

#include "layout/group.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <tox/tox.h>

static GROUPCHAT *group = NULL;

#define UTOX_MAX_BACKLOG_GROUP_MESSAGES 256

GROUPCHAT *get_group(uint32_t group_number) {
    if (group_number >= self.groups_list_size) {
        LOG_ERR("get_group", " index: %u is out of bounds." , group_number);
        return NULL;
    }

    return &group[group_number];
}

/*
 * Create a new slot for the group if group_number is greater than self.groups_list_size and return a pointer to it
 * If group_number is less than self.groups_list_size return a pointer to that slot
 */
static GROUPCHAT *group_make(uint32_t group_number) {
    if (group_number >= self.groups_list_size) {
        LOG_INFO("Groupchats", "Reallocating groupchat array to %u. Current size: %u", (group_number + 1), self.groups_list_size);
        GROUPCHAT *tmp = realloc(group, sizeof(GROUPCHAT) * (group_number + 1));
        if (!tmp) {
            LOG_FATAL_ERR(EXIT_MALLOC, "Groupchats", "Could not reallocate groupchat array to %u.", group_number + 1);
        }

        group = tmp;
        self.groups_list_size++;
    }

    memset(&group[group_number], 0, sizeof(GROUPCHAT));

    return &group[group_number];
}

GROUPCHAT *group_create(uint32_t group_number, bool av_group, Tox *tox) {
    GROUPCHAT *g = group_make(group_number);
    if (!g) {
        LOG_ERR("Groupchats", "Could not get/create group %u", group_number);
        return NULL;
    }

    LOG_ERR("Groupchats", "group_create:av_group=%d", (int)av_group);

    g->tox = tox;
    group_init(g, group_number, av_group);
    return g;
}

static bool group_message_log_to_disk(MESSAGES *m, MSG_HEADER *msg, char group_id_hexstr[(TOX_CONFERENCE_UID_SIZE * 2) + 1], char peer_pubkey_hexstr[(TOX_PUBLIC_KEY_SIZE * 2) + 1]) {

    LOG_TRACE("Groupchats", "group_message_log_to_disk:001");

    if (!settings.logging_enabled) {
        return false;
    }

    LOG_FILE_MSG_HEADER header;
    memset(&header, 0, sizeof(header));

    switch (msg->msg_type) {
        case MSG_TYPE_TEXT:
        case MSG_TYPE_ACTION_TEXT: {

            size_t author_length;
            char   *author;
            char   *author_pubkey_and_author;

            if (msg->our_msg) {
                author_length = (TOX_PUBLIC_KEY_SIZE * 2) + self.name_length;
                author        = self.name;
                author_pubkey_and_author = calloc(1, author_length);
                memcpy(author_pubkey_and_author, peer_pubkey_hexstr, (TOX_PUBLIC_KEY_SIZE * 2));
                memcpy(author_pubkey_and_author + (TOX_PUBLIC_KEY_SIZE * 2), author, self.name_length);
            } else {
                author_length = (TOX_PUBLIC_KEY_SIZE * 2) + msg->via.grp.author_length;
                author        = msg->via.grp.author;
                author_pubkey_and_author = calloc(1, author_length);
                memcpy(author_pubkey_and_author, peer_pubkey_hexstr, (TOX_PUBLIC_KEY_SIZE * 2));
                memcpy(author_pubkey_and_author + (TOX_PUBLIC_KEY_SIZE * 2), msg->via.grp.author, msg->via.grp.author_length);
            }

            header.log_version   = LOGFILE_SAVE_VERSION;
            header.time          = msg->time;
            header.author_length = author_length;
            header.msg_length    = msg->via.grp.length;
            header.author        = msg->our_msg; // HINT: this save only 1 bit. the actual author name is never saved!!??!!
            header.receipt       = !!msg->receipt_time; // bool only
            header.msg_type      = msg->msg_type;

            size_t length = sizeof(header) + msg->via.grp.length + author_length + 1; /* extra \n char*/
            uint8_t *data = calloc(1, length);
            if (!data) {
                LOG_FATAL_ERR(EXIT_MALLOC, "GroupMessages", "Can't calloc for chat logging data. size:%lu", length);
            }
            memcpy(data, &header, sizeof(header));
            memcpy(data + sizeof(header), author_pubkey_and_author, author_length);
            memcpy(data + sizeof(header) + author_length, msg->via.grp.msg, msg->via.grp.length);
            strcpy2(data + length - 1, "\n");

            LOG_TRACE("Groupchats", "group_message_log_to_disk:%s", group_id_hexstr);

            msg->disk_offset = utox_save_chatlog(group_id_hexstr, data, length);

            free(data);
            free(author_pubkey_and_author);
            return true;
        }
        default: {
            LOG_NOTE("Messages", "uTox GroupLogging:\tUnsupported message type %i", msg->msg_type);
        }
    }
    return false;

}

bool group_messages_read_from_log(Tox *tox, uint32_t group_number) {
    size_t    actual_count = 0;

    GROUPCHAT *g = get_group(group_number);
    if (!g)
    {
        LOG_ERR("GroupMessages", "group not found:%d", group_number);
        return false;
    }

    uint8_t group_id_bin[TOX_CONFERENCE_UID_SIZE];
    memset(group_id_bin, 0, TOX_CONFERENCE_UID_SIZE);
    bool res = tox_conference_get_id(tox, group_number, group_id_bin);

    char group_id_hexstr[(TOX_CONFERENCE_UID_SIZE * 2) + 1];
    memset(group_id_hexstr, 0, ((TOX_CONFERENCE_UID_SIZE * 2) + 1));
    to_hex(group_id_hexstr, group_id_bin, TOX_CONFERENCE_UID_SIZE);

    MSG_HEADER **data = utox_load_chatlog(group_id_hexstr, &actual_count, UTOX_MAX_BACKLOG_GROUP_MESSAGES, 0, true);
    if (!data) {
        LOG_ERR("GroupMessages", "uTox Logging:\tsome error:004.");
        if (actual_count > 0) {
            LOG_ERR("GroupMessages", "uTox Logging:\tFound chat log entries, but couldn't get any data. This is a problem.");
        }
        return false;
    }

    MSG_HEADER **p = data;
    MSG_HEADER *msg;
    time_t last = 0;
    while (actual_count--) {
        msg = *p++;
        if (!msg) {
            continue;
        }

        MESSAGES *m = &g->msg;
        message_add_group(m, msg);
    }

    free(data);
    return true;
}

void group_init(GROUPCHAT *g, uint32_t group_number, bool av_group) {
    pthread_mutex_lock(&messages_lock); /* make sure that messages has posted before we continue */
    if (!g->peer) {
        g->peer = calloc(UTOX_MAX_GROUP_PEERS, sizeof(GROUP_PEER *));
        if (!g->peer) {
            LOG_FATAL_ERR(EXIT_MALLOC, "Groupchats", "Could not alloc for group peers (%uB)",
                          UTOX_MAX_GROUP_PEERS * sizeof(GROUP_PEER *));
        }
    }

    snprintf((char *)g->name, sizeof(g->name), "Groupchat #%u", group_number);
    g->name_length = strnlen(g->name, sizeof(g->name) - 1);

    g->topic_length = sizeof("Drag friends to invite them") - 1;
    memcpy(g->topic, "Drag friends to invite them", sizeof("Drag friends to invite them") - 1);

    g->msg.scroll               = 1.0;
    g->msg.panel.type           = PANEL_MESSAGES;
    g->msg.panel.content_scroll = &scrollbar_group;
    g->msg.panel.y              = MAIN_TOP;
    g->msg.panel.height         = CHAT_BOX_TOP;
    g->msg.panel.width          = -SCROLL_WIDTH;
    g->msg.is_groupchat         = true;

    g->number   = group_number;
    g->notify   = settings.group_notifications;

    // Get the chat backlog
    LOG_ERR("Groupchats", "group_init:tox=%p gid=%d", g->tox, g->number);
    group_messages_read_from_log(g->tox, g->number);

    LOG_ERR("Groupchats", "group_init:av_group=%d", (int)av_group);

    g->av_group = av_group;
    pthread_mutex_unlock(&messages_lock);
    self.groups_list_count++;
}

uint32_t group_add_message(GROUPCHAT *g, uint32_t peer_id, const uint8_t *message, size_t length, uint8_t m_type, uint8_t *group_id_bin) {
    pthread_mutex_lock(&messages_lock); /* make sure that messages has posted before we continue */

    if (peer_id >= UTOX_MAX_GROUP_PEERS) {
        LOG_ERR("Groupchats", "Unable to add message from peer %u - peer id too large.", peer_id);
        pthread_mutex_unlock(&messages_lock);
        return UINT32_MAX;
    }

    const GROUP_PEER *peer = g->peer[peer_id];
    if (!peer) {
        LOG_ERR("Groupchats", "Unable to get peer %u for adding message.", peer_id);
        pthread_mutex_unlock(&messages_lock);
        return UINT32_MAX;
    }

    MSG_HEADER *msg = calloc(1, sizeof(MSG_HEADER));
    if (!msg) {
        LOG_ERR("Groupchats", "Unable to allocate memory for message header.");
        pthread_mutex_unlock(&messages_lock);
        return UINT32_MAX;
    }

    msg->our_msg  = (g->our_peer_number == peer_id ? true : false);
    msg->msg_type = m_type;

    msg->via.grp.length    = length;
    msg->via.grp.author_id = peer_id;

    msg->via.grp.author_length = peer->name_length;
    msg->via.grp.author_color  = peer->name_color;
    time(&msg->time);

    msg->via.grp.author = calloc(1, peer->name_length);
    if (!msg->via.grp.author) {
        LOG_ERR("Groupchat", "Unable to allocate space for author nickname.");
        free(msg);
        pthread_mutex_unlock(&messages_lock);
        return UINT32_MAX;
    }
    memcpy(msg->via.grp.author, peer->name, peer->name_length);

    msg->via.grp.msg = calloc(1, length);
    if (!msg->via.grp.msg) {
        LOG_ERR("Groupchat", "Unable to allocate space for message.");
        free(msg->via.grp.author);
        free(msg);
        pthread_mutex_unlock(&messages_lock);
        return UINT32_MAX;
    }
    memcpy(msg->via.grp.msg, message, length);

    pthread_mutex_unlock(&messages_lock);

    MESSAGES *m = &g->msg;

    char group_id_hexstr[(TOX_CONFERENCE_UID_SIZE * 2) + 1];
    memset(group_id_hexstr, 0, ((TOX_CONFERENCE_UID_SIZE * 2) + 1));
    to_hex(group_id_hexstr, group_id_bin, TOX_CONFERENCE_UID_SIZE);

    uint8_t peer_pubkey_bin[TOX_PUBLIC_KEY_SIZE];
    TOX_ERR_CONFERENCE_PEER_QUERY error;
    bool res_peer_pk = tox_conference_peer_get_public_key(g->tox, g->number, peer_id,
                                        peer_pubkey_bin, &error);

    char peer_pubkey_hexstr[(TOX_PUBLIC_KEY_SIZE * 2) + 1];
    memset(peer_pubkey_hexstr, 0, ((TOX_PUBLIC_KEY_SIZE * 2) + 1));
    to_hex(peer_pubkey_hexstr, peer_pubkey_bin, TOX_PUBLIC_KEY_SIZE);

    group_message_log_to_disk(m, msg, group_id_hexstr, peer_pubkey_hexstr);

    return message_add_group(m, msg);
}

void group_peer_add(GROUPCHAT *g, uint32_t peer_id, bool UNUSED(our_peer_number), uint32_t name_color) {
    pthread_mutex_lock(&messages_lock); /* make sure that messages has posted before we continue */
    if (!g->peer) {
        g->peer = calloc(UTOX_MAX_GROUP_PEERS, sizeof(GROUP_PEER *));
        if (!g->peer) {
            LOG_FATAL_ERR(EXIT_MALLOC, "Groupchats", "Could not alloc for group peers (%uB)",
                          UTOX_MAX_GROUP_PEERS * sizeof(GROUP_PEER *));
        }
        LOG_NOTE("Groupchat", "Needed to calloc peers for this group chat. (%u)" , peer_id);
    }

    const char *default_peer_name = "<unknown>";

    // Allocate space for the struct and the dynamic array holding the peer's name.
    GROUP_PEER *peer = calloc(1, sizeof(GROUP_PEER) + strlen(default_peer_name) + 1);
    if (!peer) {
        LOG_FATAL_ERR(EXIT_MALLOC, "Groupchat", "Unable to allocate space for group peer.");
    }
    strcpy2(peer->name, default_peer_name);
    peer->name_length = 0;
    peer->name_color  = name_color;
    peer->id          = peer_id;

    g->peer[peer_id] = peer;
    g->peer_count++;

    if (g->av_group) {
        group_av_peer_add(g, peer_id); //add a source for the peer
    }

    pthread_mutex_unlock(&messages_lock);
}

void group_peer_del(GROUPCHAT *g, uint32_t peer_id) {

    uint8_t group_id_bin[TOX_CONFERENCE_UID_SIZE];
    memset(group_id_bin, 0, TOX_CONFERENCE_UID_SIZE);
    bool res = tox_conference_get_id(g->tox, g->number, group_id_bin);

    group_add_message(g, peer_id, (uint8_t *)"<- has Quit!", 12, MSG_TYPE_NOTICE, group_id_bin);

    pthread_mutex_lock(&messages_lock); /* make sure that messages has posted before we continue */

    if (!g->peer) {
        LOG_TRACE("Groupchat", "Unable to del peer from NULL group");
        pthread_mutex_unlock(&messages_lock);
        return;
    }

    GROUP_PEER *peer = g->peer[peer_id];

    if (peer) {
        LOG_TRACE("Groupchat", "Freeing peer %u, name %.*s" , peer_id, (int)peer->name_length, peer->name);
        free(peer);
    } else {
        LOG_TRACE("Groupchat", "Unable to find peer for deletion");
        pthread_mutex_unlock(&messages_lock);
        return;
    }
    g->peer_count--;
    g->peer[peer_id] = NULL;
    pthread_mutex_unlock(&messages_lock);
}

void group_peer_name_change(GROUPCHAT *g, uint32_t peer_id, const uint8_t *name, size_t length) {
    pthread_mutex_lock(&messages_lock); /* make sure that messages has posted before we continue */
    if (!g->peer) {
        LOG_TRACE("Groupchat", "Unable to add peer to NULL group");
        pthread_mutex_unlock(&messages_lock);
        return;
    }

    GROUP_PEER *peer = g->peer[peer_id];
    if (!peer) {
        LOG_FATAL_ERR(EXIT_FAILURE, "Groupchat", "We can't set a name for a null peer! %u" , peer_id);
    }

    if (peer->name_length) {
        char old[TOX_MAX_NAME_LENGTH];
        char msg[TOX_MAX_NAME_LENGTH];

        memcpy(old, peer->name, peer->name_length);
        snprintf(msg, sizeof(msg), "<- has changed their name from %.*s",
                 peer->name_length, old);

        GROUP_PEER *new_peer = realloc(peer, sizeof(GROUP_PEER) + sizeof(char) * length);
        if (!new_peer) {
            free(peer);
            LOG_FATAL_ERR(EXIT_MALLOC, "Groupchat", "couldn't realloc for group peer name!");
        }

        peer = new_peer;
        peer->name_length = utf8_validate(name, length);
        memcpy(peer->name, name, length);
        g->peer[peer_id] = peer;

        pthread_mutex_unlock(&messages_lock);
        size_t msg_length = strnlen(msg, sizeof(msg) - 1);

        uint8_t group_id_bin[TOX_CONFERENCE_UID_SIZE];
        memset(group_id_bin, 0, TOX_CONFERENCE_UID_SIZE);
        bool res = tox_conference_get_id(g->tox, g->number, group_id_bin);

        group_add_message(g, peer_id, (uint8_t *)msg, msg_length, MSG_TYPE_NOTICE, group_id_bin);
        return;
    }

    /* Hopefully, they just joined, because that's the UX message we're going with! */
    GROUP_PEER *new_peer = realloc(peer, sizeof(GROUP_PEER) + sizeof(char) * length);
    if (!new_peer) {
        free(peer);
        LOG_FATAL_ERR(EXIT_MALLOC, "Groupchat", "Unable to realloc for group peer who just joined.");
    }

    peer = new_peer;
    peer->name_length = utf8_validate(name, length);
    memcpy(peer->name, name, length);
    g->peer[peer_id] = peer;

    pthread_mutex_unlock(&messages_lock);

    uint8_t group_id_bin[TOX_CONFERENCE_UID_SIZE];
    memset(group_id_bin, 0, TOX_CONFERENCE_UID_SIZE);
    bool res = tox_conference_get_id(g->tox, g->number, group_id_bin);

    group_add_message(g, peer_id, (uint8_t *)"<- has joined the chat!", 23, MSG_TYPE_NOTICE, group_id_bin);
}

void group_reset_peerlist(GROUPCHAT *g) {
    /* ARE YOU KIDDING... WHO THOUGHT THIS API WAS OKAY?! */
    for (size_t i = 0; i < g->peer_count; ++i) {
        if (g->peer[i]) {
            free(g->peer[i]);
        }
    }
    free(g->peer);
}

void group_free(GROUPCHAT *g) {
    LOG_INFO("Groupchats", "Freeing group %u", g->number);
    for (size_t i = 0; i < g->edit_history_length; ++i) {
        free(g->edit_history[i]);
    }

    free(g->edit_history);

    group_reset_peerlist(g);

    for (size_t i = 0; i < g->msg.number; ++i) {
        free(g->msg.data[i]->via.grp.author);

        // Freeing this here was causing a double free.
        // TODO: Is it needed to prevent a memory leak in some cases?
        // free(g->msg.data[i]->via.grp.msg);

        message_free(g->msg.data[i]);
    }
    free(g->msg.data);

    memset(g, 0, sizeof(GROUPCHAT));

    self.groups_list_count--;
}

void raze_groups(void) {
    LOG_INFO("Groupchats", "Freeing groupchat array");
    for (size_t i = 0; i < self.groups_list_count; i++) {
        GROUPCHAT *g = get_group(i);
        if (!g) {
            LOG_ERR("Groupchats", "Could not get group %u. Skipping...", i);
            continue;
        }
        group_free(g);
    }

    free(group);
    group = NULL;
}

/* this is called on startup, after loading the toxsave file */
void init_groups(Tox *tox) {
    self.groups_list_size = tox_conference_get_chatlist_size(tox);

    if (self.groups_list_size == 0) {
        return;
    }

    LOG_ERR("Groupchats", "init_groups:Group list size: %u", self.groups_list_size);
    group = calloc(self.groups_list_size, sizeof(GROUPCHAT));
    if (!group) {
        LOG_FATAL_ERR(EXIT_MALLOC, "Groupchats", "Could not allocate memory for groupchat array with size of: %u", self.groups_list_size);
    }

    uint32_t groups[self.groups_list_size];
    tox_conference_get_chatlist(tox, groups);

    for (size_t i = 0; i < self.groups_list_size; i++) {
        TOX_ERR_CONFERENCE_GET_TYPE error;
        TOX_CONFERENCE_TYPE type = tox_conference_get_type(tox, (uint32_t)groups[i], &error);
        if (type == TOX_CONFERENCE_TYPE_AV)
        {
            group_create(groups[i], true, tox);
        }
        else
        {
            group_create(groups[i], false, tox);
        }

        /* load conference titles */
        TOX_ERR_CONFERENCE_TITLE error2;
        size_t g_title_len =
            tox_conference_get_title_size(tox, (uint32_t)groups[i], &error2);

#define MAX_STR_LEN_FOR_TITLE 300

        if ((error2 == TOX_ERR_CONFERENCE_TITLE_OK) && (g_title_len > 0) && (g_title_len < MAX_STR_LEN_FOR_TITLE))
        {
            GROUPCHAT *g = get_group(i);
            if (g)
            {
                uint8_t *gr_title_string = calloc(1, (size_t)(g_title_len + 1));
                if (gr_title_string)
                {
                    bool res4 = tox_conference_get_title(tox, (uint32_t)groups[i], gr_title_string, &error2);

                    if (error2 == TOX_ERR_CONFERENCE_TITLE_OK)
                    {
                        // TODO: check that there always a NULL byte at the end of those strings
                        if (sizeof(g->name) < MAX_STR_LEN_FOR_TITLE)
                        {
                            snprintf((char *)g->name, sizeof(g->name), (const char *)gr_title_string);
                            g->name_length = strnlen(g->name, sizeof(g->name) - 1);
                        }
                        else
                        {
                            snprintf((char *)g->name, MAX_STR_LEN_FOR_TITLE, (const char *)gr_title_string);
                            g->name_length = strnlen(g->name, MAX_STR_LEN_FOR_TITLE - 1);
                        }
                    }

                    free(gr_title_string);
                }
            }
        }
        /* load conference titles */

    }
    LOG_INFO("Groupchat", "Initialzied groupchat array with %u groups", self.groups_list_size);
}


void group_notify_msg(GROUPCHAT *g, const char *msg, size_t msg_length) {
    if (g->notify == GNOTIFY_NEVER) {
        return;
    }

    if (g->notify == GNOTIFY_HIGHLIGHTS && strstr(msg, self.name) == NULL) {
        return;
    }

    char title[g->name_length + 25];

    snprintf(title, sizeof(title), "uTox new message in %.*s", g->name_length, g->name);
    size_t title_length = strnlen(title, sizeof(title) - 1);
    notify(title, title_length, msg, msg_length, g, 1);

    if (flist_get_sel_group() != g) {
        postmessage_audio(UTOXAUDIO_PLAY_NOTIFICATION, NOTIFY_TONE_FRIEND_NEW_MSG, 0, NULL);
    }
}


