#ifndef _BERRY_TYPES_H_
#define _BERRY_TYPES_H_

#include "config.h"

#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "ipc.h"

struct client_geom {
    int x, y, width, height;
};

struct client {
    Window window, dec;
    int ws, x_hide;
    bool decorated, hidden, fullscreen, mono, was_fs;
    struct client_geom geom;
    struct client_geom prev;
    struct client *next, *f_next;
    char title[512];
};

struct config {
    int b_width, i_width, t_height, top_gap, bot_gap, left_gap, right_gap, r_step, m_step, move_button, move_mask, resize_button, resize_mask, pointer_interval;
    unsigned long bf_color, bu_color, if_color, iu_color;
    bool focus_new, focus_motion, edge_lock, t_center, smart_place, draw_text, json_status, decorate, fs_remove_dec, fs_max;
    bool follow_pointer, warp_pointer;
    bool manage[WindowLast];
};

struct monitor {
    int x, y, width, height, screen;
};

enum atoms_net {
    NetSupported,
    NetNumberOfDesktops,
    NetActiveWindow,
    NetCurrentDesktop,
    NetClientList,
    NetWMStateFullscreen,
    NetWMCheck,
    NetWMState,
    NetWMName,
    NetWMWindowType,
    NetWMWindowTypeMenu,
    NetWMWindowTypeToolbar,
    NetWMWindowTypeDock,
    NetWMWindowTypeDialog,
    NetWMWindowTypeUtility,
    NetWMWindowTypeSplash,
    NetWMDesktop,
    NetWMFrameExtents,
    NetWMMoveResize,
    NetDesktopNames,
    NetDesktopViewport,
    NetLast
};

enum atoms_wm {
    WMDeleteWindow,
    WMProtocols,
    WMTakeFocus,
    WMLast,
};

enum berry_net {
    BerryWindowStatus,
    BerryClientEvent,
    BerryFontProperty,
    BerryLast
};

enum direction {
    EAST,
    NORTH,
    WEST,
    SOUTH
};

enum dec {
    TOP,
    LEFT,
    RIGHT,
    BOT,
    TITLE,
    DECLast
};

#endif
