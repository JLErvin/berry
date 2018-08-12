#include <math.h>
#include <stdbool.h>
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
#define NORTH 2
#define SOUTH 4
#define EAST 1
#define WEST 3

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
    int8_t border_width, title_height, top_gap;
    uint32_t focus_color, unfocus_color;
    int32_t resize_step, move_step;
    bool focus_new, edge_lock;
};


/* List of ALL clients, currently focused client */
static struct Client *focused_client = NULL;
static struct Client *clients[WORKSPACE_NUMBER];
/* Config struct to keep track of internal state*/
static struct Config conf;
static int current_ws = 0;
static Display *display;
static Window root;
static bool running = true;
static int screen;
static int screen_width;
static int screen_height;
/* Currently active workspace */

/* All functions */
static void cardinal_focus(struct Client *c, int dir);
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
static void ipc_move_relative(char arg);
static void ipc_move_absolute(char arg);
static void ipc_monocle(char arg);
static void ipc_raise(char arg);
static void ipc_resize_relative(char arg);
static void ipc_resize_absolute(char arg);
static void ipc_toggle_decorations(char arg);
static void ipc_focus_color(char arg);
static void ipc_unfocus_color(char arg);
static void ipc_border_width(char arg);
static void ipc_title_height(char arg);
static void manage_client_focus(struct Client *c);
static void manage_new_window(Window w, XWindowAttributes *wa);
static void move_relative(struct Client *c, int x, int y);
static void move_absolute(struct Client *c, int x, int y);
static void monocle(struct Client *c);
static void raise_client(struct Client *c);
static void resize_relative(struct Client *c, int x, int y);
static void resize_absolute(struct Client *c, int x, int y);
static void run(void);
static void save_client(struct Client *c, int ws);
static void send_to_workspace(struct Client *c, int s);
static void set_color(struct Client *c, unsigned long color);
static void set_input(struct Client *c);
static void setup(void);
static void show_client(struct Client *c);
static void snap_left(struct Client *c);
static void snap_right(struct Client *c);
static void switch_workspace(uint16_t ws);
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
static void (*ipc_handler[IPCLast])(char arg) =
{
    [IPCWindowMoveRelative]         = ipc_move_relative,
    [IPCWindowMoveAbsolute]         = ipc_move_absolute,
    [IPCWindowMonocle]              = ipc_monocle,
    [IPCWindowRaise]                = ipc_raise,
    [IPCWindowResizeRelative]       = ipc_resize_relative,
    [IPCWindowResizeAbsolute]       = ipc_resize_absolute,
    [IPCWindowToggleDecorations]    = ipc_toggle_decorations,
    [IPCFocusColor]                 = ipc_focus_color,
    [IPCUnfocusColor]               = ipc_unfocus_color,
    [IPCBorderWidth]                = ipc_border_width,
    [IPCTitleHeight]                = ipc_title_height
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
close(void)
{
    XCloseDisplay(display);
}

static void
decorate_new_client(struct Client *c)
{
    Window dec = XCreateSimpleWindow(display, root, c->x - conf.border_width,
            c->y - conf.title_height, c->w, c->h + conf.title_height,
            conf.border_width, conf.focus_color, conf.focus_color);

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
    /*for (int i = 0; i < WORKSPACE_NUMBER; i++)*/
    /*{*/
        /*struct Client *tmp;*/
        /*tmp = clients[i];*/
        /*//if (tmp != NULL)*/
        /*//{*/
            /*if (tmp == c) */
            /*{*/
                /*clients[i] = clients[i]->next;*/
                /*return;*/
            /*}*/
            /*else*/
            /*{*/
                /*while (tmp->next != NULL && tmp->next != c)*/
                    /*tmp = tmp->next;*/

                /*if (tmp->next == c)*/
                /*{*/
                    /*tmp->next = tmp->next->next;*/
                    /*return;*/
                /*}*/
            /*}*/
        /*//}*/
    /*}*/

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
    enum IPCCommand cmd;
    char arg1, arg2;

    // We need to handle our own client message,
    // we can redirect them to our IPC Handler,
    // much like we do with regular XEvents
    if (cme->message_type == XInternAtom(display, "BERRY_CLIENT_EVENT", False))
    {
        cmd = (enum IPCCommand)cme->data.b[0];
        arg1 = cme->data.b[1];
        arg2 = cme->data.b[2];

        if (cme->data.b[0] == 0)
            move_relative(focused_client, conf.move_step, 0);
        else if (cme->data.b[0] == 1)
            move_relative(focused_client, -conf.move_step, 0);
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
        if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_L)))
            move_relative(focused_client, conf.resize_step, 0);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_H)))
            move_relative(focused_client, -conf.resize_step, 0); 
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_J)))
            move_relative(focused_client, 0, conf.resize_step);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_K)))
            move_relative(focused_client, 0, -conf.resize_step);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Q)))
            XKillClient(display, focused_client->win);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Tab)))
            focus_next(focused_client);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_U)))
            resize_relative(focused_client, -conf.resize_step, 0);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_I)))
            resize_relative(focused_client, 0, conf.resize_step);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_O)))
            resize_relative(focused_client, 0, -conf.resize_step);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_P)))
            resize_relative(focused_client, conf.resize_step, 0);
        else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_C)))
            running = False;
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
        move_absolute(c, screen_width + conf.border_width, c->y);
        c->hidden = true;
    }
}

static void
ipc_move_relative(char arg)
{
}

static void
ipc_move_absolute(char arg)
{
}

static void
ipc_monocle(char arg)
{
}

static void
ipc_raise(char arg) 
{
}

void 
ipc_resize_relative(char arg)
{
}

void 
ipc_resize_absolute(char arg)
{
}

void 
ipc_toggle_decorations(char arg)
{
}

void 
ipc_focus_color(char arg)
{
}

void 
ipc_unfocus_color(char arg)
{
}

void 
ipc_border_width(char arg)
{
}

void 
ipc_title_height(char arg)
{
}

static void
manage_client_focus(struct Client *c)
{
    if (c != NULL && focused_client != NULL) 
        set_color(focused_client, conf.unfocus_color);

    if (c != NULL)
    {
        set_color(c, conf.focus_color);
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
    move_absolute(c, conf.border_width, 
            conf.top_gap + conf.title_height + conf.border_width);
    save_client(c, current_ws);
}

static void
move_relative(struct Client *c, int x, int y) 
{
    move_absolute(c, c->x + x, c->y + y);
}

static void
move_absolute(struct Client *c, int x, int y)
{
    XMoveWindow(display, c->win, x, y);
    if (c->decorated)
        XMoveWindow(display, c->dec, x - conf.border_width, 
                y - conf.title_height - conf.border_width);

    c->x = x;
    c->y = y;
}

static void
monocle(struct Client *c)
{
    int y_off = c->decorated ? conf.title_height 
        + conf.border_width + conf.top_gap : conf.top_gap;
    int x_off = c->decorated ? conf.border_width : 0;
    int y_size = c->decorated ? screen_height - conf.title_height 
        - conf.top_gap - (2 * conf.border_width) : screen_height - conf.top_gap;
    int x_size = c->decorated ? screen_width - (2 * conf.border_width) : screen_width;
    move_absolute(c, x_off, y_off); 
    resize_absolute(c, x_size, y_size);
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
resize_relative(struct Client *c, int x, int y) 
{
    resize_absolute(c, c->w + x, c->h + y);
}

static void
resize_absolute(struct Client *c, int x, int y) 
{
    XResizeWindow(display, c->win, MAX(x, 1), MAX(y, 1));
    if (c->decorated)
        XResizeWindow(display, c->dec, MAX(x, 1), y + conf.title_height);

    c->w = MAX(x, MINIMUM_DIM);
    c->h = MAX(y, MINIMUM_DIM);
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
set_color(struct Client *c, unsigned long color)
{
    if (c->decorated)
    {
        XSetWindowBackground(display, c->dec, color);
        XSetWindowBorder(display, c->dec, color);
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
    // Setup our conf initially
    conf.border_width  = BORDER_WIDTH;
    conf.title_height  = TITLE_HEIGHT;
    conf.focus_color   = FOCUS_COLOR;
    conf.unfocus_color = UNFOCUS_COLOR;
    conf.move_step     = MOVE_STEP;
    conf.resize_step   = RESIZE_STEP;
    conf.focus_new     = FOCUS_NEW;
    conf.edge_lock     = EDGE_LOCK;
    conf.top_gap       = TOP_GAP;

    display = XOpenDisplay(NULL);
    root = DefaultRootWindow(display);
    screen = DefaultScreen(display);
    screen_height = DisplayHeight(display, screen);
    screen_width = DisplayWidth(display, screen);
    XSelectInput(display, root,
            SubstructureRedirectMask|SubstructureNotifyMask);
    grab_keys(root);
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
    int y_off = c->decorated ? conf.title_height 
        + conf.border_width + conf.top_gap : conf.top_gap;
    int x_off = c->decorated ? conf.border_width : 0;
    int y_size = c->decorated ? screen_height - conf.title_height 
        - conf.top_gap - (2 * conf.border_width) 
        : screen_height - conf.top_gap;
    int x_size = c->decorated ? screen_width / 2 - (2 * conf.border_width) 
        : screen_width / 2;
    move_absolute(c, x_off, y_off); 
    resize_absolute(c, x_size, y_size);
}

static void
snap_right(struct Client *c)
{
    int y_off = c->decorated ? conf.title_height 
        + conf.border_width + conf.top_gap : conf.top_gap;
    int x_off = c->decorated ? (screen_width / 2) + conf.border_width 
        : screen_width / 2;
    int y_size = c->decorated ? screen_height - conf.title_height 
        - conf.top_gap - (2 * conf.border_width) 
        : screen_height - conf.top_gap;
    int x_size = c->decorated ? screen_width / 2 - (2 * conf.border_width) 
        : screen_width / 2;
    move_absolute(c, x_off, y_off); 
    resize_absolute(c, x_size, y_size);
}

static void
switch_workspace(uint16_t ws)
{
    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        if (i != ws)
            for (struct Client *tmp = clients[i]; tmp != NULL; tmp = tmp->next)
                hide_client(tmp);
        else
            for (struct Client *tmp = clients[i]; tmp != NULL; tmp = tmp->next)
                show_client(tmp);

    current_ws = ws;

    manage_client_focus(clients[current_ws]);
}

static void
toggle_decorations(struct Client *c)
{
    if (c->decorated)
    {
        decorations_destroy(c);
        move_relative(c, -conf.border_width, 
                -conf.title_height - conf.border_width);
        resize_relative(c, conf.border_width * 2,
                conf.title_height + (2 * conf.border_width));
    }
    else
    {
        decorations_create(c);
        move_relative(c, conf.border_width,
                conf.title_height + conf.border_width);
        resize_relative(c, -conf.border_width * 2,
                -conf.title_height - (2 * conf.border_width));
    }

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
