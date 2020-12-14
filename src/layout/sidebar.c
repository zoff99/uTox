#include "sidebar.h"

#include "settings.h"
#include "friend.h"

#include "../avatar.h"
#include "../flist.h"
#include "../macros.h"
#include "../self.h"
#include "../theme.h"
#include "../tox.h"

#include "../native/ui.h"

#include "../ui.h"
#include "../ui/draw.h"
#include "../ui/scrollable.h"
#include "../ui/edit.h"
#include "../ui/button.h"
#include "../ui/svg.h"

#include <string.h>

// Scrollbar or friend list
SCROLLABLE scrollbar_flist = {
    .panel = { .type = PANEL_SCROLLABLE, },
    .color = C_SCROLL,
    .x     = 2,
    .left  = 1,
    .small = 1,
};

void force_redraw_soft(void);

#include <pthread.h>
static int audio_get_tid()
{
    pthread_t pid = pthread_self();
    return (int)pid;
}

#include <semaphore.h>
extern sem_t sem_draw_audio_bars;

static void draw_background_sidebar(int x, int y, int width, int height) {
    /* Friend list (roster) background   */
    drawrect(x, y, width, height, COLOR_BKGRND_LIST);
    /* Current user badge background     */
    drawrect(x, y, width, SCALE(70), COLOR_BKGRND_MENU); // TODO magic numbers are bad
}

static int draw_audio_bars_every_cur = 0;
static int draw_audio_bars_every = 1;

void draw_bitrate(int rate, int type)
{
    // LOG_ERR("uTox", "draw_bitrate:rate=%d type=%d tid=%d", rate, type, audio_get_tid());

    if ((rate < 0) || (rate > 99000))
    {
        rate = -1;
    }

    char *num_as_string = calloc(1, 10);
    if (!num_as_string)
    {
        return;
    }

    snprintf(num_as_string, 9, "%d", rate);

    if (type == 0)
    {
        // clear area
        draw_rect_fill(SCALE(1), SCALE(1), SCALE(40), SCALE(16), COLOR_STATUS_ONLINE);
        // draw text
        setcolor(COLOR_MENU_TEXT);
        setfont(FONT_STATUS);
        drawtextwidth(SCALE(1), SCALE(40), SCALE(1), num_as_string, strlen(num_as_string));

        enddraw(SCALE(1), SCALE(1), SCALE(40), SCALE(16));
    }
    else if (type == 1)
    {
        // clear area
        draw_rect_fill(SCALE(1), SCALE(17), SCALE(40), SCALE(16), COLOR_STATUS_ONLINE);
        // draw text
        setcolor(COLOR_MENU_TEXT);
        setfont(FONT_STATUS);
        drawtextwidth(SCALE(1), SCALE(40), SCALE(17), num_as_string, strlen(num_as_string));

        enddraw(SCALE(1), SCALE(17), SCALE(40), SCALE(16));
    }

    force_redraw_soft();

    free(num_as_string);
}

/* hack to draw audio bars in the top left corner of the utox window */
void draw_audio_bars(int x, int y, int UNUSED(width), int UNUSED(height), int level, int level_med, int level_red, int level_max, int channels)
{
    sem_wait(&sem_draw_audio_bars);

    // LOG_ERR("uTox", "draw_audio_bars:x=%d y=%d level=%d tid=%d", x, y, level, audio_get_tid());

    draw_audio_bars_every_cur++;
    if (draw_audio_bars_every_cur > draw_audio_bars_every)
    {
        draw_audio_bars_every_cur = 0;
    }
    else
    {
        // do not draw audio bars on every update
        sem_post(&sem_draw_audio_bars);
        return;
    }

    int level_use = level;
    if (level_use < 0)
    {
        level_use = 0;
    }

    if (y < 0)
    {
        y = SCALE(SIDEBAR_FILTER_FRIENDS_TOP) + y;
    }
    else
    {
        y += SCALE(SIDEBAR_PADDING / 2);
    }

    x += SCALE(SIDEBAR_PADDING / 2);

    // LOG_ERR("uTox", "draw_audio_bars:x=%d y=%d level=%d", x, y, level);

    uint32_t bg_color = COLOR_BKGRND_MENU;

    drawhline(x, y, x + SCALE(level_max), bg_color);
    drawhline(x, y + 1, x + SCALE(level_max), bg_color);
    drawhline(x, y + 2, x + SCALE(level_max), bg_color);
    drawhline(x, y + 3, x + SCALE(level_max), bg_color);

    if (level_use > level_red)
    {
        drawhline(x, y, x + SCALE(level_use), status_color[2]);
        drawhline(x, y + 1, x + SCALE(level_use), status_color[2]);
        drawhline(x, y + 2, x + SCALE(level_use), status_color[2]);
        drawhline(x, y + 3, x + SCALE(level_use), status_color[2]);

        drawhline(x, y, x + SCALE(level_red), status_color[1]);
        drawhline(x, y + 1, x + SCALE(level_red), status_color[1]);
        drawhline(x, y + 2, x + SCALE(level_red), status_color[1]);
        drawhline(x, y + 3, x + SCALE(level_red), status_color[1]);

        drawhline(x, y, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 1, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 2, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 3, x + SCALE(level_med), status_color[0]);
    }
    else if (level_use > level_med)
    {
        drawhline(x, y, x + SCALE(level_use), status_color[1]);
        drawhline(x, y + 1, x + SCALE(level_use), status_color[1]);
        drawhline(x, y + 2, x + SCALE(level_use), status_color[1]);
        drawhline(x, y + 3, x + SCALE(level_use), status_color[1]);

        drawhline(x, y, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 1, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 2, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 3, x + SCALE(level_med), status_color[0]);
    }
    else
    {
        drawhline(x, y, x + SCALE(level_use), status_color[0]);
        drawhline(x, y + 1, x + SCALE(level_use), status_color[0]);
        drawhline(x, y + 2, x + SCALE(level_use), status_color[0]);
        drawhline(x, y + 3, x + SCALE(level_use), status_color[0]);
    }

    if (channels == 2)
    {
        // draw a small red indicator if audio is stereo
        drawhline(x, y, x + SCALE(5), status_color[3]);
        drawhline(x, y + 1, x + SCALE(5), status_color[3]);
        drawhline(x, y + 2, x + SCALE(5), status_color[3]);
        drawhline(x, y + 3, x + SCALE(5), status_color[3]);
    }

    enddraw(x, y, x + SCALE(level_max), y + 3);

    sem_post(&sem_draw_audio_bars);

    force_redraw_soft();
}

/* hack to draw audio bars in the top left corner of the utox window */
void draw_audio_bars2(int x, int y, int UNUSED(width), int UNUSED(height), int level, int level_med, int level_red, int level_max)
{
    sem_wait(&sem_draw_audio_bars);

    draw_audio_bars_every_cur++;
    if (draw_audio_bars_every_cur > draw_audio_bars_every)
    {
        draw_audio_bars_every_cur = 0;
    }
    else
    {
        // do not draw audio bars on every update
        sem_post(&sem_draw_audio_bars);
        return;
    }

    int level_use = level - 20;
    if (level_use < 0)
    {
        level_use = 0;
    }
    
    x += SCALE(SIDEBAR_PADDING);
    y += SCALE(SIDEBAR_PADDING);

    // LOG_ERR("uTox", "draw_audio_bars:x=%d y=%d level=%d", x, y, level);

    uint32_t bg_color = COLOR_BKGRND_MENU;

    drawhline(x, y, x + SCALE(level_max), bg_color);
    drawhline(x, y + 1, x + SCALE(level_max), bg_color);
    drawhline(x, y + 2, x + SCALE(level_max), bg_color);
    drawhline(x, y + 3, x + SCALE(level_max), bg_color);

    if (level_use > level_red)
    {
        drawhline(x, y, x + SCALE(level_use), status_color[2]);
        drawhline(x, y + 1, x + SCALE(level_use), status_color[2]);
        drawhline(x, y + 2, x + SCALE(level_use), status_color[2]);
        drawhline(x, y + 3, x + SCALE(level_use), status_color[2]);

        drawhline(x, y, x + SCALE(level_red), status_color[1]);
        drawhline(x, y + 1, x + SCALE(level_red), status_color[1]);
        drawhline(x, y + 2, x + SCALE(level_red), status_color[1]);
        drawhline(x, y + 3, x + SCALE(level_red), status_color[1]);

        drawhline(x, y, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 1, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 2, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 3, x + SCALE(level_med), status_color[0]);
    }
    else if (level_use > level_med)
    {
        drawhline(x, y, x + SCALE(level_use), status_color[1]);
        drawhline(x, y + 1, x + SCALE(level_use), status_color[1]);
        drawhline(x, y + 2, x + SCALE(level_use), status_color[1]);
        drawhline(x, y + 3, x + SCALE(level_use), status_color[1]);

        drawhline(x, y, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 1, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 2, x + SCALE(level_med), status_color[0]);
        drawhline(x, y + 3, x + SCALE(level_med), status_color[0]);
    }
    else
    {
        drawhline(x, y, x + SCALE(level_use), status_color[0]);
        drawhline(x, y + 1, x + SCALE(level_use), status_color[0]);
        drawhline(x, y + 2, x + SCALE(level_use), status_color[0]);
        drawhline(x, y + 3, x + SCALE(level_use), status_color[0]);
    }

    enddraw(0, 0, x + SCALE(level_max), y + 3);

    sem_post(&sem_draw_audio_bars);

    force_redraw_soft();
}


/* Top left self interface Avatar, name, statusmsg, status icon */
static void draw_user_badge(int x, int y, int width, int UNUSED(height)) {
    if (tox_thread_init == UTOX_TOX_THREAD_INIT_SUCCESS) {
        /* Only draw the user badge if toxcore is running */
        /*draw avatar or default image */
        x += SCALE(SIDEBAR_PADDING);
        y += SCALE(SIDEBAR_PADDING);
        if (self_has_avatar()) {
            draw_avatar_image(self.avatar->img, x, y, self.avatar->width,
                              self.avatar->height, BM_CONTACT_WIDTH, BM_CONTACT_WIDTH);
        } else {
            drawalpha(BM_CONTACT, x, y, BM_CONTACT_WIDTH, BM_CONTACT_WIDTH,
                      COLOR_MENU_TEXT);
        }

        /* Draw online/all friends filter text. */
        setcolor(!button_filter_friends.mouseover ? COLOR_MENU_TEXT_SUBTEXT : COLOR_MAIN_TEXT_HINT);
        setfont(FONT_STATUS);
        drawtextrange(x, width - SCALE(SIDEBAR_PADDING),
                      y + SCALE(SIDEBAR_PADDING) + BM_CONTACT_WIDTH,
                      flist_get_filter() ? S(FILTER_ONLINE) : S(FILTER_ALL),
                      flist_get_filter() ? SLEN(FILTER_ONLINE) : SLEN(FILTER_ALL));

        x += SCALE(SIDEBAR_PADDING) + BM_CONTACT_WIDTH;

        /* Draw name */
        setcolor(!button_name.mouseover ? COLOR_MENU_TEXT : COLOR_MENU_TEXT_SUBTEXT);
        setfont(FONT_SELF_NAME);
        drawtextrange(x, width - SCALE(SIDEBAR_PADDING * 5), y + SCALE(SIDEBAR_PADDING), self.name, self.name_length);

        /*&Draw current status message
        @TODO: separate these colors if needed (COLOR_MAIN_TEXT_HINT) */
        setcolor(!button_status_msg.mouseover ? COLOR_MENU_TEXT_SUBTEXT : COLOR_MAIN_TEXT_HINT);
        setfont(FONT_STATUS);
        drawtextrange(x, width - SCALE(SIDEBAR_PADDING * 5), y + SCALE(SIDEBAR_PADDING * 4), self.statusmsg,
                      self.statusmsg_length);

        /* Draw status button icon */
        drawalpha(BM_STATUSAREA,
                  width - BM_STATUSAREA_WIDTH - SCALE(SIDEBAR_PADDING),
                  y + SCALE(SIDEBAR_PADDING),
                  BM_STATUSAREA_WIDTH, BM_STATUSAREA_HEIGHT,
                  button_usr_state.mouseover ? COLOR_BKGRND_LIST_HOVER : COLOR_BKGRND_LIST);
        uint8_t status = tox_connected ? self.status : 3;
        drawalpha(BM_ONLINE + status,
                  width - BM_STATUS_WIDTH - BM_STATUSAREA_WIDTH / 2 - SCALE(1),
                  y + SCALE(SIDEBAR_PADDING) + BM_STATUSAREA_HEIGHT / 2 - BM_STATUS_WIDTH / 2,
                  BM_STATUS_WIDTH, BM_STATUS_WIDTH, status_color[status]);

        if ((self.connection_status == 1) || (self.connection_status == 2))
        {
            int connection_status_color = 1;
            if (self.connection_status == 2)
            {
                connection_status_color = 0;
            }
            drawvline(width - BM_STATUS_WIDTH - BM_STATUSAREA_WIDTH / 2 - SCALE(3) -3 ,
                y + SCALE(SIDEBAR_PADDING) + BM_STATUSAREA_HEIGHT / 2 - BM_STATUS_WIDTH / 2,
                y + SCALE(SIDEBAR_PADDING) + BM_STATUSAREA_HEIGHT / 2 - BM_STATUS_WIDTH / 2 + BM_STATUSAREA_HEIGHT / 5,
                status_color[connection_status_color]);

            drawvline(width - BM_STATUS_WIDTH - BM_STATUSAREA_WIDTH / 2 - SCALE(3) -2 ,
                y + SCALE(SIDEBAR_PADDING) + BM_STATUSAREA_HEIGHT / 2 - BM_STATUS_WIDTH / 2,
                y + SCALE(SIDEBAR_PADDING) + BM_STATUSAREA_HEIGHT / 2 - BM_STATUS_WIDTH / 2 + BM_STATUSAREA_HEIGHT / 5,
                status_color[connection_status_color]);

            drawvline(width - BM_STATUS_WIDTH - BM_STATUSAREA_WIDTH / 2 - SCALE(3) -1 ,
                y + SCALE(SIDEBAR_PADDING) + BM_STATUSAREA_HEIGHT / 2 - BM_STATUS_WIDTH / 2,
                y + SCALE(SIDEBAR_PADDING) + BM_STATUSAREA_HEIGHT / 2 - BM_STATUS_WIDTH / 2 + BM_STATUSAREA_HEIGHT / 5,
                status_color[connection_status_color]);

            drawvline(width - BM_STATUS_WIDTH - BM_STATUSAREA_WIDTH / 2 - SCALE(3) ,
                y + SCALE(SIDEBAR_PADDING) + BM_STATUSAREA_HEIGHT / 2 - BM_STATUS_WIDTH / 2,
                y + SCALE(SIDEBAR_PADDING) + BM_STATUSAREA_HEIGHT / 2 - BM_STATUS_WIDTH / 2 + BM_STATUSAREA_HEIGHT / 5,
                status_color[connection_status_color]);

            drawvline(width - BM_STATUS_WIDTH - BM_STATUSAREA_WIDTH / 2 - SCALE(3) + 1 ,
                y + SCALE(SIDEBAR_PADDING) + BM_STATUSAREA_HEIGHT / 2 - BM_STATUS_WIDTH / 2,
                y + SCALE(SIDEBAR_PADDING) + BM_STATUSAREA_HEIGHT / 2 - BM_STATUS_WIDTH / 2 + BM_STATUSAREA_HEIGHT / 5,
                status_color[connection_status_color]);
        }

    } else {
        drawalpha(BM_CONTACT, SCALE(SIDEBAR_PADDING), SIDEBAR_AVATAR_TOP, BM_CONTACT_WIDTH, BM_CONTACT_WIDTH,
                  COLOR_MENU_TEXT);

        setcolor(!button_name.mouseover ? COLOR_MENU_TEXT : COLOR_MENU_TEXT_SUBTEXT);
        setfont(FONT_SELF_NAME);
        drawtextrange(SCALE(SIDEBAR_NAME_LEFT), SCALE(230) - SCALE(SIDEBAR_PADDING), SIDEBAR_NAME_TOP, S(NOT_CONNECTED), SLEN(NOT_CONNECTED)); // TODO rm magic number

        if (tox_thread_init == UTOX_TOX_THREAD_INIT_ERROR) {
            setcolor(!button_status_msg.mouseover ? COLOR_MENU_TEXT_SUBTEXT : COLOR_MAIN_TEXT_HINT);
            setfont(FONT_STATUS);
            drawtextrange(SIDEBAR_STATUSMSG_LEFT, SCALE(230), SIDEBAR_STATUSMSG_TOP, S(NOT_CONNECTED_SETTINGS), SLEN(NOT_CONNECTED_SETTINGS)); // TODO rm magic number
        }
    }
}

/* Left side bar, holds the user, the roster, and the setting buttons */
PANEL

panel_side_bar = {
    .type = PANEL_NONE,
    .disabled = 0,
    .drawfunc = draw_background_sidebar,
    .width = 230,
    .child = (PANEL*[]) {
        &panel_self,
        &panel_quick_buttons,
        &panel_flist,
        NULL
    }
},
/* The user badge and buttons */
panel_self = {
    .type     = PANEL_NONE,
    .disabled = 0,
    .drawfunc = draw_user_badge,
    .child    = (PANEL*[]) {
        (PANEL*)&button_avatar, (PANEL*)&button_name,       (PANEL*)&button_usr_state,
                                (PANEL*)&button_status_msg,
        NULL
    }
},
/* Left sided toggles */
panel_quick_buttons = {
    .type     = PANEL_NONE,
    .disabled = 0,
    .child    = (PANEL*[]) {
        (PANEL*)&button_filter_friends, /* Top of roster */
        (PANEL*)&edit_search,           /* Bottom of roster*/
        (PANEL*)&button_settings,
        (PANEL*)&button_add_new_contact,
        NULL
    }
},
/* The friends and group was called list */
panel_flist = {
    .type     = PANEL_NONE,
    .disabled = 0,
    .child    = (PANEL*[]) {
        // TODO rename these
        &panel_flist_list,
        (PANEL*)&scrollbar_flist,
        NULL
    }
},
    panel_flist_list = {
        .type           = PANEL_LIST,
        .content_scroll = &scrollbar_flist,
    };


#include "../friend.h"
static void e_search_onchange(EDIT *edit) {
    char *data = edit->data;
    uint16_t length = edit->length > sizeof search_data ? sizeof search_data - 1 : edit->length;

    if (length) {
        button_add_new_contact.panel.disabled = 0;
        button_add_new_contact.nodraw         = 0;
        button_settings.panel.disabled        = 1;
        button_settings.nodraw                = 1;
        memcpy(search_data, data, length);
        search_data[length] = 0;
        flist_search((char *)search_data);
    } else {
        button_add_new_contact.panel.disabled = 1;
        button_add_new_contact.nodraw         = 1;
        button_settings.panel.disabled        = 0;
        button_settings.nodraw                = 0;
        flist_search(NULL);
    }

    redraw();
}

static void e_search_onenter(EDIT *edit) {
    char *   data   = edit->data;
    uint16_t length = edit->length;

    if (length == 76) { // FIXME, this should be error checked!
                        // No, srsly... this is lucky, not right.
        friend_add(data, length, (char *)"", 0);
        edit_setstr(&edit_search, (char *)"", 0);
    } else {
        if (tox_thread_init == UTOX_TOX_THREAD_INIT_SUCCESS) {
            /* Only change if we're logged in! */
            edit_setstr(&edit_add_new_friend_id, data, length);
            edit_setstr(&edit_search, (char *)"", 0);
            flist_selectaddfriend();
            edit_setfocus(&edit_add_new_friend_msg);
        }
    }
    return;
}

static char e_search_data[1024];
EDIT edit_search = {
    .panel = {
        .type   = PANEL_EDIT,
        .x      = SIDEBAR_SEARCH_LEFT,
        .y      = SIDEBAR_SEARCH_TOP,
        .width  = SIDEBAR_SEARCH_WIDTH,
        .height = SIDEBAR_SEARCH_HEIGHT,
    },
    .data      = e_search_data,
    .data_size = sizeof e_search_data,
    .onchange  = e_search_onchange,
    .onenter   = e_search_onenter,
    .style     = AUXILIARY_STYLE,
    .vcentered = true,
    .empty_str = { .i18nal = STR_CONTACT_SEARCH_ADD_HINT },
};
