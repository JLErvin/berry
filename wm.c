/* Copyright (c) 2018 Joshua L Ervin. All rights reserved. */
/* Licensed under the MIT License. See the LICENSE file in the project root for full license information. */

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>

#include "config.h"
#include "globals.h"
#include "ipc.h"

#define MAX(a, b) ((a > b) ? (a) : (b)) 

/* A Client is any window from the system that we have decided to manage */
struct Client 
{
    int x, y, w, h, x_hide, ws;
    bool decorated, hidden, fullscreen;
    Window win, dec;
    struct Client *next, *fnext;
};

struct Config
{
    int b_width, i_width, t_height, top_gap;
    unsigned long bf_color, bu_color, if_color, iu_color;
    int r_step, m_step;
    bool focus_new, edge_lock;
};

enum AtomsNet
{
    NetSupported,
    NetNumberOfDesktops,
    NetActiveWindow,
    NetWMStateFullscreen,
    NetWMCheck,
    NetCurrentDesktop,
    NetWMState,
    NetWMName,
    NetClientList,
    NetWMWindowType,
    NetWMWindowTypeMenu,
    NetWMWindowTypeToolbar,
    NetWMWindowTypeDock,
    NetWMWindowTypeDialog,
    NetWMWindowTypeUtility,
    NetWMWindowTypeSplash,
    NetLast
};

enum AtomsWm
{
    WMDeleteWindow,
    WMProtocols,
    WMLast,
};

enum Direction
{
    EAST,
    NORTH,
    WEST,
    SOUTH
};

/* List of ALL clients, currently focused client */
static struct Client *focused_client = NULL;
static struct Client *clients[WORKSPACE_NUMBER];
static struct Client *focus_list[WORKSPACE_NUMBER];
/* Config struct to keep track of internal state*/
static struct Config conf;
static int current_ws = 0;
static Display *display;
static Atom net_atom[NetLast], wm_atom[WMLast];
static int point_x = -1, point_y = -1;
static Window root, check;
static bool running = true;
static int screen, screen_width, screen_height;
static int (*xerrorxlib)(Display *, XErrorEvent *);
/* Currently active workspace */

/* All functions */
static void cardinal_focus(struct Client *c, int dir);
static void center_client(struct Client *c);
static void close_wm(void);
static void close_window(struct Client *c);
static void decorate_new_client(struct Client *c);
static void decorations_create(struct Client *c);
static void decorations_destroy(struct Client *c);
static void delete_client(struct Client *c);
static int euclidean_distance(struct Client *a, struct Client *b);
static void fullscreen(struct Client *c);
static struct Client* get_client_from_window(Window w);
static int get_ws_from_client(struct Client *c);
static void handle_client_message(XEvent *e);
static void handle_configure_request(XEvent *e);
static void handle_map_request(XEvent *e);
static void handle_unmap_notify(XEvent *e);
static void hide_client(struct Client *c);
static void ipc_move_relative(long *d);
static void ipc_move_absolute(long *d);
static void ipc_monocle(long *d);
static void ipc_raise(long *d);
static void ipc_resize_relative(long *d);
static void ipc_resize_absolute(long *d);
static void ipc_toggle_decorations(long *d);
static void ipc_window_close(long *d);
static void ipc_window_center(long *d);
static void ipc_bf_color(long *d);
static void ipc_bu_color(long *d);
static void ipc_if_color(long *d);
static void ipc_iu_color(long *d);
static void ipc_b_width(long *d);
static void ipc_i_width(long *d);
static void ipc_t_height(long *d);
static void ipc_switch_ws(long *d);
static void ipc_send_to_ws(long *d);
static void ipc_fullscreen(long *d);
static void ipc_snap_left(long *d);
static void ipc_snap_right(long *d);
static void ipc_cardinal_focus(long *d);
static void ipc_cycle_focus(long *d);
static void ipc_pointer_move(long *d);
static void load_config(char *conf_path);
static void manage_client_focus(struct Client *c);
static void manage_new_window(Window w, XWindowAttributes *wa);
static void move_relative(struct Client *c, int x, int y);
static void move_absolute(struct Client *c, int x, int y);
static void move_to_front(struct Client *c);
static void monocle(struct Client *c);
static void raise_client(struct Client *c);
static void refresh_client(struct Client *c);
static void refresh_config(void);
static void resize_relative(struct Client *c, int w, int h);
static void resize_absolute(struct Client *c, int w, int h); 
static void run(void);
static void save_client(struct Client *c, int ws);
static void send_to_workspace(struct Client *c, int s);
static void set_color(struct Client *c, unsigned long i_color, unsigned long b_color);
static void set_input(struct Client *c);
static void setup(void);
static void show_client(struct Client *c);
static void snap_left(struct Client *c);
static void snap_right(struct Client *c);
static void switch_workspace(int ws);
static void toggle_decorations(struct Client *c);
static void update_client_list(void);
static void usage(void);
static void version(void);
static int xerror(Display *display, XErrorEvent *e);

/* Native X11 Event handler */
static void (*event_handler[LASTEvent])(XEvent *e) = 
{
    [MapRequest]       = handle_map_request,
    [UnmapNotify]      = handle_unmap_notify,
    [ConfigureRequest] = handle_configure_request,
    [ClientMessage]    = handle_client_message
};

static void (*ipc_handler[IPCLast])(long *) = 
{
    [IPCWindowMoveRelative]       = ipc_move_relative,
    [IPCWindowMoveAbsolute]       = ipc_move_absolute,
    [IPCWindowMonocle]            = ipc_monocle,
    [IPCWindowRaise]              = ipc_raise,
    [IPCWindowResizeRelative]     = ipc_resize_relative,
    [IPCWindowResizeAbsolute]     = ipc_resize_absolute,
    [IPCWindowToggleDecorations]  = ipc_toggle_decorations,
    [IPCWindowClose]              = ipc_window_close,
    [IPCWindowCenter]             = ipc_window_center,
    [IPCFocusColor]               = ipc_bf_color,
    [IPCUnfocusColor]             = ipc_bu_color,
    [IPCInnerFocusColor]          = ipc_if_color,
    [IPCInnerUnfocusColor]        = ipc_iu_color,
    [IPCBorderWidth]              = ipc_b_width,
    [IPCInnerBorderWidth]         = ipc_i_width,
    [IPCTitleHeight]              = ipc_t_height,
    [IPCSwitchWorkspace]          = ipc_switch_ws,
    [IPCSendWorkspace]            = ipc_send_to_ws,
    [IPCFullscreen]               = ipc_fullscreen,
    [IPCSnapLeft]                 = ipc_snap_left,
    [IPCSnapRight]                = ipc_snap_right,
    [IPCSnapRight]                = ipc_snap_right,
    [IPCCardinalFocus]            = ipc_cardinal_focus,
    [IPCCycleFocus]               = ipc_cycle_focus,
    [IPCPointerMove]              = ipc_pointer_move,
};

/* Give focus to the given client in the given direction */
static void
cardinal_focus(struct Client *c, int dir)
{
    struct Client *tmp = clients[current_ws];
    struct Client *focus_next = NULL;
    int min = INT_MAX;

    while (tmp != NULL)
    {
        int dist = euclidean_distance(c, tmp);
        switch (dir)
        {
            case EAST:
                fprintf(stderr, "Focusing EAST\n");
                if (tmp->x > c->x && dist < min)
                {
                    min = dist;
                    focus_next = tmp;
                }
                break;
            case SOUTH:
                fprintf(stderr, "Focusing SOUTH\n");
                if (tmp->y > c->y && dist < min)
                {
                    min = dist;
                    focus_next = tmp;
                }
                break;
            case WEST:
                fprintf(stderr, "Focusing WEST\n");
                if (tmp->x < c->x && dist < min)
                {
                    min = dist;
                    focus_next = tmp;
                }
                break;
            case NORTH:
                fprintf(stderr, "Focusing NORTH\n");
                if (tmp->y < c->y && dist < min)
                {
                    min = dist;
                    focus_next = tmp;
                }
                break;
        }
        tmp = tmp->next;
    }

    if (focus_next == NULL)
    {
        fprintf(stderr, WINDOW_MANAGER_NAME": Cannot cardinal focus, no valid windows found\n");
        return;
    }
    else
    {
        fprintf(stderr, WINDOW_MANAGER_NAME": Valid window found in direction %d, focusing\n", dir);
        manage_client_focus(focus_next);
    }
}

/* Move a client to the center of the screen, centered vertically and horizontally
 * by the middle of the Client
 */
static void
center_client(struct Client *c)
{
    fprintf(stderr, WINDOW_MANAGER_NAME": Centering Client");
    move_absolute(c, screen_width / 2 - (c->w / 2), screen_height / 2 - (c->h / 2));
}

/* Close connection to the current display */
static void
close_wm(void)
{
    fprintf(stderr, WINDOW_MANAGER_NAME": Closing display...");
    XCloseDisplay(display);
}

/* Communicate with the given Client, kindly telling it to close itself
 * and terminate any associated processes using the WM_DELETE_WINDOW protocol
 */
static void
close_window(struct Client *c)
{
    XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = c->win;
    ev.xclient.message_type = wm_atom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = wm_atom[WMDeleteWindow];
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(display, c->win, False, NoEventMask, &ev);
    fprintf(stderr, WINDOW_MANAGER_NAME": Closing window...");
}

/* Create new "dummy" windows to be used as decorations for the given client */
static void
decorate_new_client(struct Client *c)
{
    fprintf(stderr, "Decorating new client\n");
    int w = c->w + 2 * conf.i_width;
    int h = c->h + 2 * conf.i_width + conf.t_height;
    int x = c->x - conf.i_width - conf.b_width;
    int y = c->y - conf.i_width - conf.b_width - conf.t_height; 
    Window dec = XCreateSimpleWindow(display, root, x, y, w, h, conf.b_width,
            conf.bu_color, conf.bf_color);

    fprintf(stderr, "Mapping new decorations\n");
    XMapWindow(display, dec);
    c->dec = dec;
    c->decorated = true;
}

static void
decorations_create(struct Client *c)
{
    decorate_new_client(c);
}

/* Destroy any "dummy" windows associated with the given Client as decorations */
static void 
decorations_destroy(struct Client *c)
{
    fprintf(stderr, "Removing decorations\n");
    XUnmapWindow(display, c->dec);
    XDestroyWindow(display, c->dec);
    c->decorated = false;
}

/* Remove the given Client from the list of currently managed clients 
 * Does not free the given client from memory. 
 * */
static void
delete_client(struct Client *c)
{
    int ws;
    ws = get_ws_from_client(c);

    if (ws == -1)
    {
        fprintf(stderr, "Cannot delete client, not found\n"); 
        return;
    }
    else
        fprintf(stderr, "Deleting client on workspace %d\n", ws); 

    /* Delete in the stack */
    if (clients[ws] == c)
        clients[ws] = clients[ws]->next;
    else
    {
        struct Client *tmp = clients[ws];
        while (tmp != NULL && tmp->next != c)
            tmp = tmp->next;

        tmp->next = tmp->next->next;
    }

    /* Delete in the focus list */
    /* I'll factor this out later */
    if (focus_list[ws] == c)
        focus_list[ws] = focus_list[ws]->fnext;
    else
    {
        struct Client *tmp = focus_list[ws];
        while (tmp != NULL && tmp->fnext != c)
            tmp = tmp->fnext;

        tmp->fnext = tmp->fnext->fnext;
    }

    if (clients[ws] == NULL)
        focused_client = NULL;

    update_client_list();
}

/* Calculate the distance between the upper left corners of two windows
 * NOTE: This distance can only be used for comparission 
 * (i.e. distance(a, b) > distance(c, b)
 */
static int
euclidean_distance(struct Client *a, struct Client *b)
{
    int xDiff = a->x - b->x;
    int yDiff = a->y - b->y;
    return pow(xDiff, 2) + pow(yDiff, 2);
}

/* Set the given Client to be fullscreen. Moves the window to fill the dimensions
 * of the given display. 
 * Updates the value of _NET_WM_STATE_FULLSCREEN to reflect fullscreen changes
 */
static void
fullscreen(struct Client *c)
{
    move_absolute(c, 0, 0);
    resize_absolute(c, screen_width, screen_height);
    if (!c->fullscreen)
        XChangeProperty(display, c->win, net_atom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *)&net_atom[NetWMStateFullscreen], 1);
    else
        XChangeProperty(display, c->win, net_atom[NetWMState], XA_ATOM, 32, PropModeReplace, (unsigned char *) 0, 0);

    c->fullscreen = !c->fullscreen;
}

/* Focus the next window in the list. Windows are sorted by the order in which they are 
 * created (mapped to the window manager)
 */
static void
focus_next(struct Client *c)
{
    if (c == NULL)
        return;

    int ws;
    ws = get_ws_from_client(c);

    if (focus_list[ws] == c && focus_list[ws]->fnext == NULL)
        return;

    struct Client *tmp;
    tmp = c->fnext == NULL ? focus_list[ws] : c->fnext;
    manage_client_focus(tmp);
}

/* Returns the struct Client associated with the given struct Window */
static struct Client*
get_client_from_window(Window w)
{
    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        for (struct Client *tmp = clients[i]; tmp != NULL; tmp = tmp->next)
            if (tmp->win == w)
                return tmp;

    return NULL;
}

static int
get_ws_from_client(struct Client *c)
{
    int ws;
    ws = -1;
    /* Maybe a little inefficient, but it cleans up the rest of the code 
     * for now. Find the workspace of the given window and go delete it */
    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        for (struct Client *tmp = clients[i]; tmp != NULL; tmp = tmp->next)
            if (tmp == c) {
                ws = i;
                break;
            }

    return ws;
}

/* Redirect an XEvent from berry's client program, berryc */
static void
handle_client_message(XEvent *e)
{
    XClientMessageEvent *cme = &e->xclient;
    long cmd, *data;

    if (cme->message_type == XInternAtom(display, BERRY_CLIENT_EVENT, False))
    {
        fprintf(stderr, "Recieved event from berryc\n");
        if (cme->format != 32)
            return;

        cmd = cme->data.l[0];
        data = cme->data.l;
        ipc_handler[cmd](data);
    }
}

static void
handle_configure_request(XEvent *e) 
{
    struct Client *c;
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;

    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(display, ev->window, ev->value_mask, &wc);
    c = get_client_from_window(ev->window);

    if (c != NULL)
        refresh_client(c);
}

static void
handle_map_request(XEvent *e)
{
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;
    if (!XGetWindowAttributes(display, ev->window, &wa))
        return;

    if (wa.override_redirect)
        return;

    manage_new_window(ev->window, &wa);
}

static void
handle_unmap_notify(XEvent *e)
{
    XUnmapEvent *ev = &e->xunmap;
    struct Client *c;
    c = get_client_from_window(ev->window);

    if (c != NULL)
    {
        focus_next(c);
        if (c->decorated)
            XDestroyWindow(display, c->dec);
        delete_client(c);
        free(c);
    }
}

/* Hides the given Client by moving it outside of the visible display */
static void
hide_client(struct Client *c)
{
    if (!c->hidden)
    {
        c->x_hide = c->x;
        move_absolute(c, screen_width + conf.b_width, c->y);
        c->hidden = true;
    }
}

static void
ipc_move_relative(long *d)
{
    int x, y;

    if (focused_client == NULL)
        return;

    x = d[1];
    y = d[2];

    move_relative(focused_client, x, y);
}

static void
ipc_move_absolute(long *d)
{
    int x, y;

    if (focused_client == NULL)
        return;

    x = d[1];
    y = d[2];

    move_absolute(focused_client, x, y);
}

static void
ipc_monocle(long *d)
{
    if (focused_client == NULL)
        return;

    monocle(focused_client);
}

static void
ipc_raise(long *d) 
{
    if (focused_client == NULL)
        return;

    raise_client(focused_client);
}

static void 
ipc_resize_relative(long *d)
{
    int w, h;

    if (focused_client == NULL)
        return;

    w = d[1];
    h = d[2];

    resize_relative(focused_client, w, h);
}

static void 
ipc_resize_absolute(long *d)
{
    int w, h;

    if (focused_client == NULL)
        return;

    w = d[1];
    h = d[2];

    resize_absolute(focused_client, w, h);
}

static void 
ipc_toggle_decorations(long *d)
{
    if (focused_client == NULL)
        return ;

    toggle_decorations(focused_client);
}

static void
ipc_window_close(long *d)
{
    if (focused_client == NULL)
        return;

    close_window(focused_client);
}

static void
ipc_window_center(long *d)
{
    center_client(focused_client);
}

static void 
ipc_bf_color(long *d)
{
    unsigned long nc;
    nc = d[1];
    conf.bf_color = nc;

    refresh_config();
}

void 
ipc_bu_color(long *d)
{
    unsigned long nc;
    nc = d[1];
    conf.bu_color = nc;

    refresh_config();
}

static void
ipc_if_color(long *d)
{
    unsigned long nc;
    nc = d[1];
    conf.if_color = nc;

    refresh_config();
}

static void
ipc_iu_color(long *d)
{
    unsigned long nc;
    nc = d[1];
    conf.iu_color = nc;

    refresh_config();
}

static void 
ipc_b_width(long *d)
{
    int w;
    w = d[1];
    conf.b_width = w;

    /*decorations_destroy(focused_client);*/
    /*decorations_create(focused_client);*/
    refresh_config();
    raise_client(focused_client);
}

static void
ipc_i_width(long *d)
{
    int w;
    w = d[1];
    conf.i_width = w;

    refresh_config();
}

static void 
ipc_t_height(long *d)
{
    int th;
    th = d[1];
    conf.t_height = th;

    refresh_config();
}

static void
ipc_switch_ws(long *d)
{
    int ws = d[1];
    switch_workspace(ws - 1);
}

static void
ipc_send_to_ws(long *d)
{
    if (focused_client == NULL)
        return;

    int ws = d[1];
    send_to_workspace(focused_client, ws - 1);
}

static void
ipc_fullscreen(long *d)
{
    if (focused_client == NULL)
        return;

    fullscreen(focused_client);
}

static void
ipc_snap_left(long *d)
{
    if (focused_client == NULL)
        return;

    snap_left(focused_client);
}

static void
ipc_snap_right(long *d)
{
    if (focused_client == NULL)
        return;

    snap_right(focused_client);
}

static void
ipc_cardinal_focus(long *d)
{
    int dir = d[1];
    cardinal_focus(focused_client, dir);
}

static void
ipc_cycle_focus(long *d)
{
    focus_next(focused_client);
}

static void
ipc_pointer_move(long *d)
{
    /* Shoutout to vain for this methodology */
    int x, y, di, dx, dy;
    unsigned int dui;
    Window child, dummy;
    struct Client *c;

    if (d[1] == 2)
    {
        point_x = -1;
        point_y = -1;
        return;
    }

    XQueryPointer(display, root, &dummy, &child, &x, &y, &di, &di, &dui);

    if (point_x == -1 && point_y == -1)
    {
        point_x = x;
        point_y = y;
    }

    dx = x - point_x;
    dy = y - point_y;

    point_x = x;
    point_y = y;

    c = get_client_from_window(child);
    fprintf(stderr, "Recieved pointer input, moving window by %d, %d\n", dx, dy);
    if(c != NULL)
    {
        /* Focus the client for either type of event */
        manage_client_focus(c);

        /* Only move if it is of type 1 */
        if (d[1] == 1)
            move_relative(c, dx, dy);
    }
}

static void
load_config(char *conf_path)
{
    if (fork() == 0) 
    {
        setsid();
        execl(conf_path, conf_path, NULL);
        fprintf(stderr, "CONFIG PATH: %s\n", conf_path);
    }
}

static void
manage_client_focus(struct Client *c)
{
    if (c != NULL && focused_client != NULL) 
        set_color(focused_client, conf.iu_color, conf.bu_color);

    if (c != NULL)
    {
        set_color(c, conf.if_color, conf.bf_color);
        raise_client(c);
        set_input(c);
        /* Remove focus from the old window */
        XDeleteProperty(display, root, net_atom[NetActiveWindow]);
        focused_client = c;
        /* Tell EWMH about our new window */
        XChangeProperty(display, root, net_atom[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &(c->win), 1);
        move_to_front(c);
    }
}

static void
manage_new_window(Window w, XWindowAttributes *wa)
{
    /* Credits to vain for XGWP checking */
    Atom prop, da;
    unsigned char *prop_ret = NULL;
    int di;
    unsigned long dl;
    if (XGetWindowProperty(display, w, net_atom[NetWMWindowType], 0,
                sizeof (Atom), False, XA_ATOM, &da, &di, &dl, &dl,
                &prop_ret) == Success) 
    {
        if (prop_ret)
        {
            prop = ((Atom *)prop_ret)[0];
            if (prop == net_atom[NetWMWindowTypeDock] ||
                prop == net_atom[NetWMWindowTypeToolbar] ||
                prop == net_atom[NetWMWindowTypeUtility] ||
                prop == net_atom[NetWMWindowTypeMenu])
            {
                fprintf(stderr, "Window is of type dock, toolbar, utility, or menu: not managing\n");
                fprintf(stderr, "Mapping new window, not managed\n");
                XMapWindow(display, w);
                return;
            }
        }
    }


    struct Client *c;
    c = malloc(sizeof(struct Client));
    c->win = w;
    c->ws = current_ws;
    c->x = wa->x;
    c->y = wa->y;
    c->w = wa->width;
    c->h = wa->height;
    c->hidden = false;
    c->fullscreen = false;

    decorate_new_client(c);
    XMapWindow(display, c->win);
    refresh_client(c); // using our current factoring, w/h are set incorrectly
    save_client(c, current_ws);
    manage_client_focus(c);
    center_client(c);
    update_client_list();
}

static void
move_relative(struct Client *c, int x, int y) 
{
    /* Constrain the current client to the w/h of display */
    if (conf.edge_lock)
    {
        if (c->x + c->w + x > screen_width)
            x = screen_width - c->w - c->x;

        if (c->y + c->h + y > screen_height)
            y = screen_height - c->h - c->y;

        if (c->x + x < 0)
            x = -c->x;

        if (c->y + y < conf.top_gap)
            y = -c->y + conf.top_gap;
    }

    move_absolute(c, c->x + x, c->y + y);
}

static void
move_absolute(struct Client *c, int x, int y)
{
    int dest_x = x;
    int dest_y = y;

    if (c->decorated) {
        dest_x = x + conf.i_width + conf.b_width;
        dest_y = y + conf.i_width + conf.b_width + conf.t_height;
    }

    /* move relative to where decorations should go */
    XMoveWindow(display, c->win, dest_x, dest_y);
    if (c->decorated)
        XMoveWindow(display, c->dec, x, y);

    c->x = x;
    c->y = y;
}

static void
move_to_front(struct Client *c)
{
    int ws;
    ws = get_ws_from_client(c);

    /* If we didn't find the client */
    if (ws == -1)
        return;

    /* If the Client is at the front of the list, ignore command */
    if (clients[ws] == c || clients[ws]->next == NULL)
        return;

    struct Client *tmp;
    for (tmp = clients[ws]; tmp->next != NULL; tmp = tmp->next)
        if (tmp->next == c)
            break;

    tmp->next = tmp->next->next; /* remove the Client from the list */
    c->next = clients[ws]; /* add the client to the front of the list */
    clients[ws] = c;
}

static void
monocle(struct Client *c)
{
    move_absolute(c, 0, conf.top_gap); 
    resize_absolute(c, screen_width, screen_height - conf.top_gap);
}

static void
raise_client(struct Client *c)
{
    if (c != NULL)
    {
        if (c->decorated)
            XRaiseWindow(display, c->dec);

        XRaiseWindow(display, c->win);
    }
}

static void
refresh_client(struct Client *c)
{
    for (int i = 0; i < 2; i++)
    {
        move_relative(c, 0, 0);
        resize_relative(c, 0, 0);
    }
}

static void
refresh_config(void)
{
    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        for (struct Client *tmp = clients[i]; tmp != NULL; tmp = tmp->next)
        {
            /* We run into this annoying issue when where we have to 
             * re-create these windows since the border_width has changed.
             * We end up destroying and recreating this windows, but this
             * causes them to be redrawn on the wrong screen, regardless of
             * their current desktop. The easiest way around this is to move
             * them all to the current desktop and then back agian */
            if (tmp->decorated)
            {
                decorations_destroy(tmp);
                decorations_create(tmp);
            }
            refresh_client(tmp);
            show_client(tmp);
            if (focused_client != tmp) 
                set_color(tmp, conf.iu_color, conf.bu_color);
            else
                set_color(tmp, conf.if_color, conf.bf_color);

            if (i != current_ws)
                hide_client(tmp);
            else
            {
                show_client(tmp);
                raise_client(tmp);
            }
        }
}

static void
resize_absolute(struct Client *c, int w, int h) 
{
    int dest_w = w;
    int dest_h = h;
    int dec_w = w;
    int dec_h = h;

    if (c->decorated) 
    {
        dest_w = w - (2 * conf.i_width) - (2 * conf.b_width);
        dest_h = h - (2 * conf.i_width) - (2 * conf.b_width) - conf.t_height;

        dec_w = w - (2 * conf.b_width);
        dec_h = h - (2 * conf.b_width);
    }

    XResizeWindow(display, c->win, MAX(dest_w, MINIMUM_DIM), MAX(dest_h, MINIMUM_DIM));
    if (c->decorated)
        XResizeWindow(display, c->dec, MAX(dec_w, MINIMUM_DIM), MAX(dec_h, MINIMUM_DIM));

    c->w = MAX(w, MINIMUM_DIM);
    c->h = MAX(h, MINIMUM_DIM);
}

static void
resize_relative(struct Client *c, int w, int h) 
{
    resize_absolute(c, c->w + w, c->h + h);
}

static void
run(void)
{
    XEvent e;
    XSync(display, false);
    while(running)
    {
        fprintf(stderr, "Receieved new %d event\n", e.type);
        XNextEvent(display, &e);
        if (event_handler[e.type])
        {
            fprintf(stderr, "Handling %d event\n", e.type);
            event_handler[e.type](&e);
        }
    }
}

static void
save_client(struct Client *c, int ws)
{
    c->next = clients[ws];
    clients[ws] = c;

    c->fnext = focus_list[ws];
    focus_list[ws] = c;
}

static void
send_to_workspace(struct Client *c, int ws)
{
    c->ws = ws;
    delete_client(c);
    save_client(c, ws);
    hide_client(c);
    focus_next(clients[current_ws]);
}

static void
set_color(struct Client *c, unsigned long i_color, unsigned long b_color)
{
    if (c->decorated)
    {
        XSetWindowBackground(display, c->dec, i_color);
        XSetWindowBorder(display, c->dec, b_color);
        XClearWindow(display, c->dec);
    }
}


static void
set_input(struct Client *c)
{
    XSetInputFocus(display, c->win, RevertToParent, CurrentTime);
}

static void
setup(void)
{
    unsigned long data[1], data2[1];
    // Setup our conf initially
    conf.b_width   = BORDER_WIDTH;
    conf.t_height  = TITLE_HEIGHT;
    conf.i_width   = INTERNAL_BORDER_WIDTH;
    conf.bf_color  = BORDER_FOCUS_COLOR;
    conf.bu_color  = BORDER_UNFOCUS_COLOR;
    conf.if_color  = INNER_FOCUS_COLOR;
    conf.iu_color  = INNER_UNFOCUS_COLOR; 
    conf.m_step    = MOVE_STEP;
    conf.r_step    = RESIZE_STEP;
    conf.focus_new = FOCUS_NEW;
    conf.edge_lock = EDGE_LOCK;
    conf.top_gap   = TOP_GAP;

    display = XOpenDisplay(NULL);
    root = DefaultRootWindow(display);
    screen = DefaultScreen(display);
    screen_height = DisplayHeight(display, screen);
    screen_width = DisplayWidth(display, screen);
    XSelectInput(display, root,
            SubstructureRedirectMask|SubstructureNotifyMask);
    xerrorxlib = XSetErrorHandler(xerror);

    Atom utf8string;
    check = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);

    /* ewmh supported atoms */
    utf8string                       = XInternAtom(display, "UTF8_STRING", False);
    net_atom[NetSupported]           = XInternAtom(display, "_NET_SUPPORTED", False);
    net_atom[NetNumberOfDesktops]    = XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False);
    net_atom[NetActiveWindow]        = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    net_atom[NetWMStateFullscreen]   = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    net_atom[NetWMCheck]             = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
    net_atom[NetCurrentDesktop]      = XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
    net_atom[NetWMState]             = XInternAtom(display, "_NET_WM_STATE", False);
    net_atom[NetWMName]              = XInternAtom(display, "_NET_WM_NAME", False);
    net_atom[NetClientList]          = XInternAtom(display, "_NET_CLIENT_LIST", False);
    net_atom[NetWMWindowType]        = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    net_atom[NetWMWindowTypeDock]    = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    net_atom[NetWMWindowTypeToolbar] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
    net_atom[NetWMWindowTypeMenu]    = XInternAtom(display, "_NET_WM_WINDOW_TYPE_MENU", False);
    net_atom[NetWMWindowTypeSplash]  = XInternAtom(display, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    /* Some icccm atoms */
    wm_atom[WMDeleteWindow]          = XInternAtom(display, "WM_DELETE_WINDOW", False);
    wm_atom[WMProtocols]             = XInternAtom(display, "WM_PROTOCOLS", False);

    XChangeProperty(display, check, net_atom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &check, 1);
    XChangeProperty(display, check, net_atom[NetWMName], utf8string, 8, PropModeReplace, (unsigned char *) WINDOW_MANAGER_NAME, 5);
    XChangeProperty(display, root, net_atom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &check, 1);
    XChangeProperty(display, root, net_atom[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *) net_atom, NetLast);

    /* Set the total number of desktops */
    data[0] = WORKSPACE_NUMBER;
    XChangeProperty(display, root, net_atom[NetNumberOfDesktops], XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 1);

    /* Set the intial "current desktop" to 0 */
    data2[0] = current_ws;
    XChangeProperty(display, root, net_atom[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data2, 1);
}

static void
show_client(struct Client *c)
{
    if (c->hidden)
    {
        move_absolute(c, c->x_hide, c->y);
        raise_client(c);
        c->hidden = false;
    }
}

static void
snap_left(struct Client *c)
{
    move_absolute(c, 0, conf.top_gap); 
    resize_absolute(c, screen_width / 2, screen_height - conf.top_gap);
}

static void
snap_right(struct Client *c)
{
    move_absolute(c, screen_width / 2, conf.top_gap); 
    resize_absolute(c, screen_width / 2, screen_height - conf.top_gap);
}

static void
switch_workspace(int ws)
{
    unsigned long data[1];

    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        if (i != ws)
            for (struct Client *tmp = clients[i]; tmp != NULL; tmp = tmp->next)
                hide_client(tmp);
        else
            for (struct Client *tmp = clients[i]; tmp != NULL; tmp = tmp->next)
            {
                show_client(tmp);
                XLowerWindow(display, tmp->win);
                XLowerWindow(display, tmp->dec);
            }


    current_ws = ws;
    manage_client_focus(clients[current_ws]);

    data[0] = ws;
    XChangeProperty(display, root, net_atom[NetCurrentDesktop], XA_CARDINAL, 32,
            PropModeReplace, (unsigned char *) data, 1);
}

static void
toggle_decorations(struct Client *c)
{
    if (c->decorated)
        decorations_destroy(c);
    else
        decorations_create(c);

    refresh_client(c);
    raise_client(c);
    manage_client_focus(c);
}

static void
update_client_list(void)
{
    /* Remove all current clients */
    XDeleteProperty(display, root, net_atom[NetClientList]);
    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        for (struct Client *tmp = clients[i]; tmp != NULL; tmp = tmp->next)
            XChangeProperty(display, root, net_atom[NetClientList], XA_WINDOW, 32, PropModeAppend,
                    (unsigned char *) &(tmp->win), 1);
}

static void
usage(void)
{
    fprintf(stderr, "Usage: berry [-h|-v|-c CONFIG_PATH]\n");
    exit(EXIT_SUCCESS);
}

static void
version(void)
{
    fprintf(stderr, "%s %s\n", WINDOW_MANAGER_NAME, __THIS_VERSION__);
    fprintf(stderr, "Copyright (c) 2018 Joshua L Ervin\n");
    fprintf(stderr, "Released under the MIT License\n");
    exit(EXIT_SUCCESS);
}

static int
xerror(Display *display, XErrorEvent *e)
{
    /* this is stolen verbatim from katriawm which stole it from dwm lol */
    if (e->error_code == BadWindow ||
            (e->request_code == X_SetInputFocus && e->error_code == BadMatch) ||
            (e->request_code == X_PolyText8 && e->error_code == BadDrawable) ||
            (e->request_code == X_PolyFillRectangle && e->error_code == BadDrawable) ||
            (e->request_code == X_PolySegment && e->error_code == BadDrawable) ||
            (e->request_code == X_ConfigureWindow && e->error_code == BadMatch) ||
            (e->request_code == X_GrabButton && e->error_code == BadAccess) ||
            (e->request_code == X_GrabKey && e->error_code == BadAccess) ||
            (e->request_code == X_CopyArea && e->error_code == BadDrawable))
        return 0;

    fprintf(stderr, "Fatal request. request code=%d, error code=%d\n",
            e->request_code, e->error_code);
    return xerrorxlib(display, e);
}

int
main(int argc, char *argv[])
{
    int opt;
    char *conf_path = malloc(MAXLEN * sizeof(char));
    bool conf_found = true;
    conf_path[0] = '\0';

    while ((opt = getopt(argc, argv, "hvc:")) != -1)
    {
        switch (opt) 
        {
            case 'h':
                usage();
                break;
            case 'c':
                snprintf(conf_path, MAXLEN * sizeof(char), "%s", optarg);
                break;
            case 'v':
                version();
                break;
        }
    }

    if (conf_path[0] == '\0') 
    {
        char *xdg_home = getenv("XDG_CONFIG_HOME");
        if (xdg_home != NULL)
            snprintf(conf_path, MAXLEN * sizeof(char), "%s/%s", xdg_home, BERRY_AUTOSTART);
        else
        {
            char *home = getenv("HOME");
            if (home == NULL) 
            {
                fprintf(stderr, "Warning $XDG_CONFIG_HOME and $HOME not found"
                        "autostart will not be loaded.\n");
                conf_found = false;

            }
            snprintf(conf_path, MAXLEN * sizeof(char), "%s/%s/%s", home, ".config", BERRY_AUTOSTART);
        }
    }

    display = XOpenDisplay(NULL);
    if (!display)
        exit(EXIT_FAILURE);

    fprintf(stderr, "Successfully opened display\n");

    setup();
    if (conf_found)
        load_config(conf_path);
    run();
    close_wm();
}
