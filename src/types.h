#ifndef BERRY_TYPES_H_
#define BERRY_TYPES_H_

#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdint.h>

struct client_geom {
    int16_t x, y;
    uint16_t width, height;
};

struct client {
    Window window, dec;
    uint8_t ws;
    int16_t x_hide;
    bool decorated, hidden, fullscreen;
    struct client_geom geom;
    struct client *next, *f_next;
};

struct config {
    uint16_t b_width, i_width, t_height, top_gap;
    uint32_t bf_color, bu_color, if_color, iu_color;
    uint16_t r_step, m_step;
    bool focus_new, edge_lock;
};

struct monitor {
    uint16_t x, y, width, height, screen;
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
