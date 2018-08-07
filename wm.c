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
#include "types.h"

#define MAX(a, b) ((a > b) ? (a) : (b))

// Fields my guys
static Client *clients = NULL, *focused_client = NULL;
static Config config;
static Display *display;
static Window root;
static bool running = true;
static int screen;
static int current_ws = 1;

// Functions and such
static void cardinal_focus(Client *c, int dir);
static void close(void);
static void decorate_new_client(Client *c);
static void decorations_create(Client *c);
static void decorations_destroy(Client *c);
static void delete_client(Client *c);
static int euclidean_distance(Client *a, Client *b);
static Client* get_client_from_window(Window w);
static void grab_keys(Window w);
static void handle_client_message(XEvent *e);
static void handle_configure_request(XEvent *e);
static void handle_keypress(XEvent *e);
static void handle_map_request(XEvent *e);
static void handle_unmap_notify(XEvent *e);
static void hide_client(Client *c);
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
static void manage_client_focus(Client *c);
static void manage_new_window(Window w, XWindowAttributes *wa);
static void move_relative(Client *c, int x, int y);
static void move_absolute(Client *c, int x, int y);
static void monocle(Client *c);
static void raise_client(Client *c);
static void resize_relative(Client *c, int x, int y);
static void resize_absolute(Client *c, int x, int y);
static void run(void);
static void save_client(Client *c);
static void set_color(Client *c, unsigned long color);
static void set_input(Client *c);
static void setup(void);
static void show_client(Client *c);
static void snap_left(Client *c);
static void snap_right(Client *c);
static void switch_workspace(int i);
static void toggle_decorations(Client *c);

static void (*event_handler[LASTEvent])(XEvent *e) = 
{
    [MapRequest]       = handle_map_request,
    [KeyPress]         = handle_keypress,
    [UnmapNotify]      = handle_unmap_notify,
    [ConfigureRequest] = handle_configure_request,
    [ClientMessage]    = handle_client_message
};

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


/*
 * Focuses from the given Client in the specified
 * direction.
 *
 * @arg1 struct Client to focus from
 * @arg2 int dir to focus (1=EAST,2=NORTH,3=WEST,4=SOUTH)
 *
 * From a specific starting Client, manages focus to the nearest
 * Window in the specific direction. For focus change to occur,
 * the entirety of the desired window must be in the specified
 * direction, otherwise no focusing will occur.
 *
 */
void
cardinal_focus(Client *c, int dir)
{
    Client *t = clients;
    Client *focus_next = NULL;
    int min = INT_MAX;

    while (t)
    {
        int dist = euclidean_distance(c, t);
        switch (dir)
        {
            case EAST:
                if (t->x > c->x && dist < min && t->ws == current_ws) { min = dist, focus_next = t;}
                break;
            case NORTH:
                if (t->y > c->y && dist < min && t->ws == current_ws) { min = dist, focus_next = t;}
                break;
            case WEST:
                if (t->x < c->x && dist < min && t->ws == current_ws) { min = dist, focus_next = t;}
                break;
            case SOUTH:
                if (t->y < c->y && dist < min && t->ws == current_ws) { min = dist, focus_next = t;}
                break;
        }

        t = t->next;
    }

    manage_client_focus(focus_next);
}

/*
 * Cycles focus to the next Client on the same
 * workspace from the specific starting Client.
 *
 * @arg1 struct Client c to start from
 *
 * Focuses the next Client on the same workspace.
 * Cycling is sorted base on the order that windows
 * requested MapRequest events (order in which they 
 * were opened). If only one window exists on the
 * workspace, focus does not change.
 */
void
cycle_focus(Client *c)
{
    Client *t = c;

    if (t == clients && clients->next == NULL)
        return;

    if (t->next == NULL)
        t = clients;
    else
        t = t->next;

    while (t != NULL && t->ws != current_ws)
        if (!t->next)
            t = clients;
        else
            t = t->next;

    manage_client_focus(t);
}

/*
 * Closes the current connection to the XServer
 */
void
close(void)
{
    XCloseDisplay(display);
}

/*
 * Creates a new associated decoration window
 * for the specific Client.
 *
 * @arg1 struct Client c to create decoration for
 *
 * Initializes a new XSimpleWindow and saves it in the
 * specific Client. Decoration settings are created
 * using the current config.
 */
void
decorate_new_client(Client *c)
{
    Window dec = XCreateSimpleWindow(
            display,
            root,
            c->x - config.border_width,
            c->y - config.title_height,
            c->w,
            c->h + config.title_height,
            config.border_width,
            config.focus_color,
            config.focus_color);

    XMapWindow(display, dec);
    c->dec = dec;
    c->decorated = true;
}

/*
 * Adds decorations to the specified Window. 
 * 
 * @arg1 Client *c to add decorations for
 */
void
decorations_create(Client *c)
{
    decorate_new_client(c);
}

/*
 * Removes decorations to the specified Window. 
 * 
 * @arg1 Client *c to remove decorations for
 */
void 
decorations_destroy(Client *c)
{
    XUnmapWindow(display, c->dec);
    XDestroyWindow(display, c->dec);
    c->decorated = false;
}

/*
 * Deletes the specified Client from the 
 * global client list
 *
 * @arg1 struct Client c to be deleted
 */
void
delete_client(Client *c)
{
    if (clients == c)
        clients = clients->next;
    else
    {
        Client *t = clients;
        while (t->next != c)
            t = t->next;

        if (t->next == NULL)
            t->next = clients;
        else
            t->next = t->next->next;
    }
}

/*
 * Calculates and returns an int representing the distance
 * between the top-left corner of two Clients.
 *
 * @arg1 struct Client a to compare
 * @arg2 struct Client b to compare
 *
 * Calculated distances are not absolute, instead just computing
 * the square of the differences between the x and y coordinates
 * of the top-left corner of each Client. Useful only for
 * comparisson of distances/
 *
 * @return int representing distance between two clients
 */
int
euclidean_distance(Client *a, Client *b)
{
    int xDiff = a->x - b->x;
    int yDiff = a->y - b->y;
    return 1;
    return pow(xDiff, 3) + pow(yDiff, 2);
}

/*
 * Returns the Client associated with the given Window
 *
 * @arg1 Window w to search for
 *
 * @return struct Client* to Client containing Window
 *         returns NULL if the Client is not found
 */
Client*
get_client_from_window(Window w)
{
    Client *c;
    c = clients;

    while (c)
    {
        if (c->win == w)
            return c;

        c = c->next;
    }

    return NULL;
}

/*
 * Grabs all necessary keys on the given window
 *
 * @arg1 Window w to grab keys on
 *
 */
void
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

    XGrabButton(display, Button3, Mod4Mask, w, True,
            ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);

    XGrabButton(display, Button1, Mod4Mask, w, True,
            ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
}

/*
 * Handles XClientMessageEvents from XEvents by redirecting
 * them to the IPCEvent handler
 */
void
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
            move_relative(focused_client, 50, 0);
        else if (cme->data.b[0] == 1)
            move_relative(focused_client, -50, 0);
    }
}

/*
 * Manages XConfigureRequestEvent from the display's XServer
 *
 * @arg1 XEvent *e of type XConfigureRequestEvent
 */
void
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

/*
 * Manages XKeyEvent from the display's XServer
 *
 * @arg1 XEvent *e of type XKeyEvent
 */
void
handle_keypress(XEvent *e) 
{
    XKeyEvent *ev = &e->xkey;

    if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_L)))
        move_relative(focused_client, config.resize_step, 0);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_H)))
        move_relative(focused_client, -config.resize_step, 0); 
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_J)))
        move_relative(focused_client, 0, config.resize_step);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_K)))
        move_relative(focused_client, 0, -config.resize_step);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Q)))
        XKillClient(display, focused_client->win);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Tab)))
        cycle_focus(focused_client);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_U)))
        resize_relative(focused_client, -config.resize_step, 0);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_I)))
        resize_relative(focused_client, 0, config.resize_step);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_O)))
        resize_relative(focused_client, 0, -config.resize_step);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_P)))
        resize_relative(focused_client, config.resize_step, 0);
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
        cycle_focus(focused_client);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_M)))
        monocle(focused_client);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_N)))
        toggle_decorations(focused_client);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_T)))
        snap_left(focused_client);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Y)))
        snap_right(focused_client);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_1)))
        switch_workspace(1);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_2)))
        switch_workspace(2);
}

/*
 * Manages XMapRequestEvents from the display's XServer.
 *
 * @arg1 XEvent *e of type XMapRequestEvent
 *
 * Creates a new Client for the given window. Adds and maps
 * decorations for the Client.
 */
void
handle_map_request(XEvent *e)
{
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;
    XGetWindowAttributes(display, ev->window, &wa);
    manage_new_window(ev->window, &wa);
}

/*
 * Manages XUnmapEvents from the display's XServer
 *
 * @arg1 XEvent *e of type xunmap
 *
 * Finds the Client associated with the event's
 * given window and destroys the associated decoration.
 * Deletes and frees the associates Client.
 */
void
handle_unmap_notify(XEvent *e)
{
    XUnmapEvent *ev = &e->xunmap;
    Client *c;
    c = get_client_from_window(ev->window);

    if (c)
    {
        cycle_focus(c);
        if (c->decorated)
            XDestroyWindow(display, c->dec);
        delete_client(c);
        free(c);
    }
}

/*
 * Hides the specified Client by moving it off of the current screen.
 * Records the previous location of the window and changes the interal
 * state of the Client to hidden
 *
 * @arg1 Client *c to hide
 */
void
hide_client(Client *c)
{
    if (!c->hidden)
    {
        c->x_hide = c->x;
        move_absolute(c, 1920 + config.border_width, c->y);
        c->hidden = true;
    }
}

void
ipc_move_relative(char arg)
{
}

void
ipc_move_absolute(char arg)
{
}

void
ipc_monocle(char arg)
{
}

void
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

/*
 * Manages focus by taking focus away from the currently
 * focused client and giving focus to the specified Client
 *
 * @arg1 struct Client c to give focus to
 *
 * Removes focus from the currently focused client, in turn
 * setting its decoration color to the unfocused state.
 * Sets focus to the specified Client, if it exists. Sets color
 * for the specified Client, raises it to the top of all windows
 * on that workspace, and sets XInputFocus for that window.
 */
void
manage_client_focus(Client *c)
{
    if (c && focused_client) 
        set_color(focused_client, config.unfocus_color);

    if (c)
    {
        set_color(c, config.focus_color);
        raise_client(c);
        set_input(c);
        focused_client = c;
    }
}

/*
 * Manages a newly created window. 
 *
 * @arg1 Window w that has been mapped
 * @arg2 XWindowAttributes of new Window
 *
 * Creates a new Client for the specified window.
 * Creates decorations for that Client and updates
 * the interal state to that of the given 
 * XWindowAttributes. Focuses the newly created window.
 */
void
manage_new_window(Window w, XWindowAttributes *wa)
{
    Client *c;
    c = malloc(sizeof(Client));
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
    move_absolute(c, config.border_width, config.top_gap + config.title_height + config.border_width);
    save_client(c);
}

/*
 * Shifts the given Client by the specified x and y 
 * coordinates. 
 *
 * @arg1 Client *c to move
 * @arg2 int x to shift by (+ moves right, - moves left)
 * @arg3 int y t0 shift by (+ move down, - moves up)
 */
void
move_relative(Client *c, int x, int y) 
{
    move_absolute(c, c->x + x, c->y + y);
}

/*
 * Moves the given Client to the specified x and y 
 * coordinates, regardless of current position.
 *
 * @arg1 Client *c to move
 * @arg2 int x to move to
 * @arg3 int y to move to
 *
 * Absolute movement refers to the top-left corner of the window
 */
void
move_absolute(Client *c, int x, int y)
{
    if (config.edge_lock)
    {
        /*x = MAX(config.border_width, x);*/
        /*y = MAX(config.title_height + config.border_width, y);*/
    }

    XMoveWindow(display, c->win, x, y);
    if (c->decorated)
        XMoveWindow(display, c->dec, x - config.border_width, y - config.title_height - config.border_width);

    c->x = x;
    c->y = y;
}

/*
 * Moves the current Client to fill the current screen
 *
 * @arg1 Client *c to monocle
 */
void
monocle(Client *c)
{
    int y_off = c->decorated ? config.title_height + config.border_width + config.top_gap : config.top_gap;
    int x_off = c->decorated ? config.border_width : 0;
    int y_size = c->decorated ? 1080 - config.title_height - config.top_gap - (2 * config.border_width) : 1080 - config.top_gap;
    int x_size = c->decorated ? 1920 - (2 * config.border_width) : 1920;
    move_absolute(c, x_off, y_off); 
    resize_absolute(c, x_size, y_size);
}

/*
 * If the specified Client exists, raises the associated
 * window and decoration to the top of all active windows
 * on the current workspace.
 *
 * @arg1 struct Client c to raise
 *
 * Resizing is done from the top left corner, which will stay
 * stationary during all resizing operations
 */
void
raise_client(Client *c)
{
    if (c) {
        if (c->decorated)
            XRaiseWindow(display, c->dec);

        XRaiseWindow(display, c->win);
    }
}

/*
 * Resizes the current Client relative to its current
 * size by the specified amounts. 
 *
 * @arg1 Client *c to resize
 * @arg2 int x to resize by (- = shrink left, + = grow right)
 * @arg3 int y to resize by (- = shrink up, + = grow down)
 *
 * Resizing is done from the top left corner, which will stay
 * stationary during all resizing operations
 */
void
resize_relative(Client *c, int x, int y) 
{
    resize_absolute(c, c->w + x, c->h + y);
}

/*
 * Resizes the specified Client to the given width and height
 * on an absolute scale.
 *
 * @arg1 Client *c to resize
 * @arg2 int x to resize to
 * @arg3 int y to resize to
 */
void
resize_absolute(Client *c, int x, int y) 
{
    XResizeWindow(display, c->win, MAX(x, 1), MAX(y, 1));
    if (c->decorated)
        XResizeWindow(display, c->dec, MAX(x, 1), y + config.title_height);

    c->w = MAX(x, 1);
    c->h = MAX(y, 1);
}

/*
 * Main event loop. Intercepts XEvents from the 
 * X server and redirects them to their specified
 * functions. Will continue to run as long as the global
 * running bool is set to true.
 */
void
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

/*
 * Saves the Client for future use by adding it to 
 * the linked list of clients
 *
 * @arg1 Client *c to add
 */
void
save_client(Client *c)
{
    c->next = clients;
    clients = c;
}

/*
 * Sets the decoration color of the specified Client.
 *
 * @arg1 struct Client c to focus
 * @arg2 unsigned long color to change to
 */
void
set_color(Client *c, unsigned long color)
{
    if (c->decorated)
    {
        XSetWindowBackground(display, c->dec, color);
        XSetWindowBorder(display, c->dec, color);
        XClearWindow(display, c->dec);
    }
}


/*
 * Change XInputFocus to the window associated with the given Client
 *
 * @arg1 struct Client c to set input for
 */
void
set_input(Client *c)
{
    XSetInputFocus(display, c->win, RevertToParent, CurrentTime);
}

/*
 * Initializes the state of the window manager.
 * Sets the config values to that specified in config.h,
 * creates the display, root, and screen. Redirects input
 * to the default root window. Grabs keys on the root window.
 */
void
setup(void)
{
    // Setup our config initially
    config.border_width  = BORDER_WIDTH;
    config.title_height  = TITLE_HEIGHT;
    config.focus_color   = FOCUS_COLOR;
    config.unfocus_color = UNFOCUS_COLOR;
    config.move_step     = MOVE_STEP;
    config.resize_step   = RESIZE_STEP;
    config.focus_new     = FOCUS_NEW;
    config.edge_lock     = EDGE_LOCK;
    config.top_gap       = TOP_GAP;

    display = XOpenDisplay(NULL);
    root = DefaultRootWindow(display);
    screen = DefaultScreen(display);
    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);
    grab_keys(root);
}

/*
 * Moves the specified Client to the active workspace.
 * Changes the state of the Client so it is not hidden
 *
 * @arg1 Client *c to show
 */
void
show_client(Client *c)
{
    if (c->hidden)
    {
        move_absolute(c, c->x_hide, c->y);
        c->hidden = false;
    }
}

/*
 * Snaps the specified Client to the left half of the screen.
 * Fills the entire space considering window decorations and 
 * respecting config's top gap
 *
 * @arg1 Client *c to snap
 */
void
snap_left(Client *c)
{
    int y_off = c->decorated ? config.title_height + config.border_width + config.top_gap : config.top_gap;
    int x_off = c->decorated ? config.border_width : 0;
    int y_size = c->decorated ? 1080 - config.title_height - config.top_gap - (2 * config.border_width) : 1080 - config.top_gap;
    int x_size = c->decorated ? 1920  /2 - (2 * config.border_width) : 1920 / 2;
    move_absolute(c, x_off, y_off); 
    resize_absolute(c, x_size, y_size);
}

/*
 * Snaps the specified Client to the right half of the screen.
 * Fills the entire space considering window decorations and 
 * respecting config's top gap
 *
 * @arg1 Client *c to snap
 */
void
snap_right(Client *c)
{
    int y_off = c->decorated ? config.title_height + config.border_width + config.top_gap : config.top_gap;
    int x_off = c->decorated ? (1920 / 2) + config.border_width : 1920 / 2;
    int y_size = c->decorated ? 1080 - config.title_height - config.top_gap - (2 * config.border_width) : 1080 - config.top_gap;
    int x_size = c->decorated ? 1920 / 2 - (2 * config.border_width) : 1920 / 2;
    move_absolute(c, x_off, y_off); 
    resize_absolute(c, x_size, y_size);
}

/*
 * Changes to the specified workspaces. Hides all windows not
 * on the current workspace and shows all windows on the 
 * new workspace. Changes focus the the first available window
 * on the new workspace.
 *
 * @arg1 int i workspace to  switch to
 */
void
switch_workspace(int i)
{
    Client *t = clients;
    current_ws = i;

    while (t != NULL)
    {
        if (t->ws == current_ws)
            show_client(t);
        else
            hide_client(t);

        t = t->next;
    }

    /* We will still be focused on the last window selected if we don't
     * change focus here. This will focus to the first window that it finds
     * on the active workspace.
     */
    cycle_focus(focused_client);
}

/*
 * Toggles decorations for the given Client and moves the Client
 * accordingly so that it occupies the same footprint.
 *
 * @arg1 struct Client *c to toggle decorations
 *
 * Toggles decorations for the given Client and moves the Window 
 * so that it occupies the same space (aka shrinks the window when
 * adding decorations, grows when removing them). Also raises the 
 * specified Client.
 */
void
toggle_decorations(Client *c)
{
    if (c->decorated)
    {
        decorations_destroy(c);
        move_relative(c, -config.border_width, -config.title_height - config.border_width);
        resize_relative(c, config.border_width * 2, config.title_height + (2 * config.border_width));
    }
    else
    {
        decorations_create(c);
        move_relative(c, config.border_width, config.title_height + config.border_width);
        resize_relative(c, -config.border_width * 2, -config.title_height - (2 * config.border_width));
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
