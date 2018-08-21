#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "config.h"
#include "ipc.h"

#define MAX(a, b) ((a > b) ? (a) : (b)) 
#define MIN(a, b) ((a < b) ? (a) : (b)) 
#define WORKSPACE_NUMBER 10

struct Client 
{
    int x, y, w, h, x_hide, ws;
    bool decorated, hidden;
    Window win;
    Window dec;
    struct Client *next;
};

struct Config
{
    int b_width, i_width, t_height, top_gap;
    unsigned long bf_color, bu_color, if_color, iu_color;
    int r_step, m_step;
    bool focus_new, edge_lock;
};

enum
{
    NetWMFullscreen, 
    NetActiveWindow, 
    NetWMName, 
    NetWMCheck,
    NetWMSupported,
    NetWMNumberDesktops,
    NetDesktopGeometry,
    NetCurrentDesktop,
    NetLast 
};

enum direction
{
    EAST,
    NORTH,
    WEST,
    SOUTH
};

/* List of ALL clients, currently focused client */
static struct Client *focused_client = NULL;
static struct Client *clients[WORKSPACE_NUMBER];
/* Config struct to keep track of internal state*/
static struct Config conf;
static int current_ws = 0;
static Display *display;
static Atom net_atom[NetLast];
static Window root, check;
static bool running = true;
static int screen;
static int screen_width;
static int screen_height;
/* Currently active workspace */

/* All functions */
static void cardinal_focus(struct Client *c, int dir);
static void center_client(struct Client *c);
static void close(void);
static void decorate_new_client(struct Client *c);
static void decorations_create(struct Client *c);
static void decorations_destroy(struct Client *c);
static void delete_client(struct Client *c);
static int euclidean_distance(struct Client *a, struct Client *b);
static void fullscreen(struct Client *c);
static struct Client* get_client_from_window(Window w);
static void grab_keys(Window w);
static void handle_client_message(XEvent *e);
static void handle_configure_request(XEvent *e);
static void handle_keypress(XEvent *e);
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
static void ipc_bf_color(long *d);
static void ipc_bu_color(long *d);
static void ipc_if_color(long *d);
static void ipc_iu_color(long *d);
static void ipc_b_width(long *d);
static void ipc_i_width(long *d);
static void ipc_t_height(long *d);
static void ipc_switch_ws(long *d);
static void ipc_send_to_ws(long *d);
static void manage_client_focus(struct Client *c);
static void manage_new_window(Window w, XWindowAttributes *wa);
static void move_relative(struct Client *c, int x, int y);
static void move_absolute(struct Client *c, int x, int y);
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

/* Native X11 Event handler */
static void (*event_handler[LASTEvent])(XEvent *e) = 
{
    [MapRequest]       = handle_map_request,
    [KeyPress]         = handle_keypress,
    [UnmapNotify]      = handle_unmap_notify,
    [ConfigureRequest] = handle_configure_request,
    [ClientMessage]    = handle_client_message
};

/* Event handler for our IPC protocle */
/*static void (*ipc_handler[IPCLast])(long *) =*/
/*{*/
    /*[IPCWindowMoveRelative]         = ipc_move_relative,*/
    /*[IPCWindowMoveAbsolute]         = ipc_move_absolute,*/
    /*[IPCWindowMonocle]              = ipc_monocle,*/
    /*[IPCWindowRaise]                = ipc_raise,*/
    /*[IPCWindowResizeRelative]       = ipc_resize_relative,*/
    /*[IPCWindowResizeAbsolute]       = ipc_resize_absolute,*/
    /*[IPCWindowToggleDecorations]    = ipc_toggle_decorations,*/
    /*[IPCFocusColor]                 = ipc_bf_color,*/
    /*[IPCUnfocusColor]               = ipc_bu_color,*/
    /*[IPCBorderWidth]                = ipc_b_width,*/
    /*[IPCTitleHeight]                = ipc_t_height,*/
    /*[IPCSwitchWorkspace]            = ipc_switch_ws,*/
    /*[IPCSendWorkspace]              = ipc_send_to_ws,*/
/*};*/

static void (*ipc_handler[IPCLast])(long *) = 
{
    [IPCWindowMoveRelative]= ipc_move_relative,
    [IPCWindowMoveAbsolute]= ipc_move_absolute,
    [IPCWindowMonocle]=  ipc_monocle,
    [IPCWindowRaise]= ipc_raise,
    [IPCWindowResizeRelative]= ipc_resize_relative,
    [IPCWindowResizeAbsolute]= ipc_resize_absolute,
    [IPCWindowToggleDecorations]= ipc_toggle_decorations,
    [IPCFocusColor]= ipc_bf_color,
    [IPCUnfocusColor]= ipc_bu_color,
    [IPCInnerFocusColor]= ipc_if_color,
    [IPCInnerUnfocusColor]= ipc_iu_color,
    [IPCBorderWidth]= ipc_b_width,
    [IPCInnerBorderWidth]= ipc_i_width,
    [IPCTitleHeight]= ipc_t_height,
    [IPCSwitchWorkspace]= ipc_switch_ws,
    [IPCSendWorkspace]= ipc_send_to_ws
};


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
                if (tmp->x > c->x && dist < min)
                {
                    min = dist;
                    focus_next = tmp;
                }
                break;
            case NORTH:
                if (tmp->y > c->y && dist < min)
                {
                    min = dist;
                    focus_next = tmp;
                }
                break;
            case WEST:
                if (tmp->x < c->x && dist < min)
                {
                    min = dist;
                    focus_next = tmp;
                }
                break;
            case SOUTH:
                if (tmp->y < c->y && dist < min)
                {
                    min = dist;
                    focus_next = tmp;
                }
                break;
        }
        tmp = tmp->next;
    }

    manage_client_focus(focus_next);
}

static void
center_client(struct Client *c)
{
    move_absolute(c, screen_width / 2 - (c->w / 2), screen_height / 2 - (c->h / 2));
}

static void
close(void)
{
    XCloseDisplay(display);
}

static void
decorate_new_client(struct Client *c)
{
    int w = c->w + 2 * conf.i_width;
    int h = c->h + 2 * conf.i_width + conf.t_height;
    int x = c->x - conf.i_width - conf.b_width;
    int y = c->y - conf.i_width - conf.b_width - conf.t_height; 
    Window dec = XCreateSimpleWindow(display, root, x, y, w, h, conf.b_width,
            conf.bu_color, conf.bf_color);

    XMapWindow(display, dec);
    c->dec = dec;
    c->decorated = true;
}

static void
decorations_create(struct Client *c)
{
    decorate_new_client(c);
}

static void 
decorations_destroy(struct Client *c)
{
    XUnmapWindow(display, c->dec);
    XDestroyWindow(display, c->dec);
    c->decorated = false;
}

static void
delete_client(struct Client *c)
{
    /* TODO: will we ever have to delete a client
     * that is not on the active workspace?
     */
    int ws = current_ws;
    if (clients[ws] == c)
        clients[ws] = clients[ws]->next;
    else
    {
        struct Client *tmp = clients[ws];
        while (tmp != NULL && tmp->next != c)
            tmp = tmp->next;

        tmp->next = tmp->next->next;
    }

    if (clients[ws] == NULL)
        focused_client = NULL;
}

static int
euclidean_distance(struct Client *a, struct Client *b)
{
    int xDiff = a->x - b->x;
    int yDiff = a->y - b->y;
    return pow(xDiff, 2) + pow(yDiff, 2);
}

static void
fullscreen(struct Client *c)
{
    move_absolute(c, 0, 0);
    resize_absolute(c, screen_width, screen_height);
}

static void
focus_next(struct Client *c)
{
    if (c == NULL)
        return;

    if (clients[current_ws] == c && clients[current_ws]->next == NULL)
        return;

    struct Client *tmp = c;
    if (tmp->next == NULL)
        manage_client_focus(clients[current_ws]);
    else
        manage_client_focus(tmp->next);
}

static struct Client*
get_client_from_window(Window w)
{
    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        for (struct Client *tmp = clients[i]; tmp != NULL; tmp = tmp->next)
            if (tmp->win == w)
                return tmp;

    return NULL;
}

static void
grab_keys(Window w)
{
    XGrabKey(display, XKeysymToKeycode(display, XK_L), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_H), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_J), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_K), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_Q), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_Tab), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_1), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_2), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_9), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_0), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);

    XGrabKey(display, XKeysymToKeycode(display, XK_U), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_I), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_O), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_P), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);

    // Cardinal focusing
    XGrabKey(display, XKeysymToKeycode(display, XK_A), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_S), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_X), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_Z), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);

    XGrabKey(display, XKeysymToKeycode(display, XK_C), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_N), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    // monocle
    XGrabKey(display, XKeysymToKeycode(display, XK_M), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_T), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_Y), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_1), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_2), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_F), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_B), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_V), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);

    XGrabButton(display, Button3, Mod4Mask, w, True,
            ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
            GrabModeAsync, GrabModeAsync, None, None);

    XGrabButton(display, Button1, Mod4Mask, w, True,
            ButtonPressMask|ButtonReleaseMask|PointerMotionMask, 
            GrabModeAsync, GrabModeAsync, None, None);
}

static void
handle_client_message(XEvent *e)
{
    XClientMessageEvent *cme = &e->xclient;
    long cmd, *data;

    if (cme->message_type == XInternAtom(display, BERRY_CLIENT_EVENT, False))
    {
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
}

static void
handle_keypress(XEvent *e) 
{
    XKeyEvent *ev = &e->xkey;

    if (focused_client != NULL)
    {
        if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_L)))
            move_relative(focused_client, conf.r_step, 0);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_H)))
            move_relative(focused_client, -conf.r_step, 0); 
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_J)))
            move_relative(focused_client, 0, conf.r_step);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_K)))
            move_relative(focused_client, 0, -conf.r_step);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Q)))
            XKillClient(display, focused_client->win);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Tab)))
            focus_next(focused_client);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_U)))
            resize_relative(focused_client, -conf.r_step, 0);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_I)))
            resize_relative(focused_client, 0, conf.r_step);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_O)))
            resize_relative(focused_client, 0, -conf.r_step);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_P)))
            resize_relative(focused_client, conf.r_step, 0);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_C)))
            center_client(focused_client);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_A)))
            cardinal_focus(focused_client, 3);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_S)))
            cardinal_focus(focused_client, 1);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Z)))
            cardinal_focus(focused_client, 2);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_X)))
            cardinal_focus(focused_client, 4);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Tab)))
            focus_next(focused_client);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_M)))
            monocle(focused_client);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_N)))
            toggle_decorations(focused_client);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_T)))
            snap_left(focused_client);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Y)))
            snap_right(focused_client);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_9)))
            send_to_workspace(focused_client, 0);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_0)))
            send_to_workspace(focused_client, 1);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_F)))
            fullscreen(focused_client);
        /*else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_B)))*/
            /*[>resize_relative_limit(focused_client, 50, 0);<]*/
        /*else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_V)))*/
            /*[>resize_relative_limit(focused_client, 0, 50);<]*/
    }


    /* We can switch clients even if we have no focused windows */
    if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_1)))
        switch_workspace(0);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_2)))
        switch_workspace(1);
}

static void
handle_map_request(XEvent *e)
{
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;
    XGetWindowAttributes(display, ev->window, &wa);
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
}

static void
ipc_iu_color(long *d)
{
}

static void 
ipc_b_width(long *d)
{
    int w;
    w = d[1];
    conf.b_width = w;

    refresh_config();
}

static void
ipc_i_width(long *d)
{
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
    switch_workspace(ws);
}

static void
ipc_send_to_ws(long *d)
{
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
        focused_client = c;
    }
}

static void
manage_new_window(Window w, XWindowAttributes *wa)
{
    struct Client *c;
    c = malloc(sizeof(struct Client));
    c->win = w;
    c->ws = current_ws;
    c->x = wa->x;
    c->y = wa->y;
    c->w = wa->width;
    c->h = wa->height;
    c->hidden = false;

    decorate_new_client(c);
    XMapWindow(display, c->win);
    manage_client_focus(c);
    refresh_client(c); // using our current factoring, w/h are set incorrectly
    save_client(c, current_ws);
    center_client(c);
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

        if (c->y + y < 0)
            y = -c->y;
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
    move_relative(c, 0, 0);
    resize_relative(c, 0, 0);
}

static void
refresh_config(void)
{
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
        XNextEvent(display, &e);
        if (event_handler[e.type])
            event_handler[e.type](&e);
    }
}

static void
save_client(struct Client *c, int ws)
{
    c->next = clients[ws];
    clients[ws] = c;
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
    unsigned long data[1];
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
    grab_keys(root);

    Atom utf8string;
    check = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);

    utf8string = XInternAtom(display, "UTF8_STRING", False);
    net_atom[NetWMFullscreen] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN" , False);
    net_atom[NetActiveWindow] = XInternAtom(display, "_NET_ACTIVE_WINDOW" , False);
    net_atom[NetWMName] = XInternAtom(display, "_NET_WM_NAME" , False);
    net_atom[NetWMCheck] = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK" , False);
    net_atom[NetWMSupported] = XInternAtom(display, "_NET_SUPPORTED" , False);
    net_atom[NetWMNumberDesktops] = XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS" , False);
    net_atom[NetDesktopGeometry] = XInternAtom(display, "_NET_DESKTOP_GEOMETRY" , False);
    net_atom[NetCurrentDesktop] = XInternAtom(display, "_NET_CURRENT_DESKTOP" , False);

    XChangeProperty(display, check, net_atom[NetWMCheck], XA_WINDOW, 32,
            PropModeReplace, (unsigned char *) &check, 1);
    XChangeProperty(display, check, net_atom[NetWMName], utf8string, 8, PropModeReplace, 
            (unsigned char *) "berry", 5);
    XChangeProperty(display, root, net_atom[NetWMCheck], XA_WINDOW, 32,
            PropModeReplace, (unsigned char *) &check, 1);

    XChangeProperty(display, root, net_atom[NetWMSupported], XA_ATOM, 32,
            PropModeReplace, (unsigned char *) net_atom, NetLast);

    data[0] = WORKSPACE_NUMBER;
    XChangeProperty(display, root, net_atom[NetWMNumberDesktops], XA_CARDINAL, 32,
            PropModeReplace, (unsigned char *) data, 1);
}

static void
show_client(struct Client *c)
{
    if (c->hidden)
    {
        move_absolute(c, c->x_hide, c->y);
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
                show_client(tmp);

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
}

int
main(void)
{
    display = XOpenDisplay(NULL);
    if (!display)
        return 0;

    setup();
    run();
    close();
}
