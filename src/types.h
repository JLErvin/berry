#ifndef _BERRY_TYPES_H_
#define _BERRY_TYPES_H_

#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdint.h>

struct client_geom {
    int x, y, width, height;
};

struct client {
    Window window, dec;
    int ws, x_hide;
    bool decorated, hidden, fullscreen;
    struct client_geom geom;
    struct client *next, *f_next;
};

struct config {
    int b_width, i_width, t_height, top_gap, r_step, m_step;
    unsigned long bf_color, bu_color, if_color, iu_color, tf_color, tu_color;
    bool focus_new, edge_lock;
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
    NetLast
};

enum atoms_wm {
    WMDeleteWindow,
    WMProtocols,
    WMTakeFocus,
    WMLast,
};

enum direction {
    EAST,
    NORTH,
    WEST,
    SOUTH
};

#endif
