#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "config.h"
#include "types.h"

// Fields my guys
static Client *clients = NULL, *focused_client = NULL; 
static Config config;
static Display *display;
static Window root;
static bool running = true;
static int screen;
static int current_ws = 0;

// Functions and such
static void cardinal_focus(Client *c, int dir);
static void close(void);
static void decorate_new_client(Client *c);
static void delete_client(Client *c);
static int euclidean_distance(Client *a, Client *b);
static Client* get_client_from_window(Window w);
static void grab_keys(Window w);
static void handle_configure_request(XEvent *e);
static void handle_keypress(XEvent *e);
static void handle_map_request(XEvent *e);
static void handle_unmap_notify(XEvent *e);
static void manage_client_focus(Client *c);
static void manage_new_window(Window w, XWindowAttributes *wa);
static void move_relative(Client *c, int x, int y);
static void move_absolute(Client *c, int x, int y);
static void raise_client(Client *c);
static void resize_relative(Client *c, int x, int y);
static void resize_absolute(Client *c, int x, int y);
static void run(void);
static void save_client(Client *c);
static void set_color(Client *c, unsigned long color);
static void set_input(Client *c);
static void setup(void);

static void (*event_handler[LASTEvent])(XEvent *e) = 
{
    [MapRequest]       = handle_map_request,
    [KeyPress]         = handle_keypress,
    [UnmapNotify]      = handle_unmap_notify,
    [ConfigureRequest] = handle_configure_request
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
                if (t->x > c->x && dist < min) { min = dist, focus_next = t;}
                break;
            case NORTH:
                if (t->y > c->y && dist < min) { min = dist, focus_next = t;}
                break;
            case WEST:
                if (t->x < c->x && dist < min) { min = dist, focus_next = t;}
                break;
            case SOUTH:
                if (t->y < c->y && dist < min) { min = dist, focus_next = t;}
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
    const Window dec = XCreateSimpleWindow(
            display,
            root,
            c->x - BORDER_WIDTH,
            c->y - TITLE_HEIGHT,
            c->w,
            c->h + TITLE_HEIGHT,
            BORDER_WIDTH,
            FOCUS_COLOR,
            FOCUS_COLOR);

    XMapWindow(display, dec);
    c->dec = dec;
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
    // monocle
    XGrabKey(display, XKeysymToKeycode(display, XK_M), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_T), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_Y), Mod4Mask,
            w, True, GrabModeAsync, GrabModeAsync);

    XGrabButton(display, Button3, Mod4Mask, w, True,
            ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);

    XGrabButton(display, Button1, Mod4Mask, w, True,
            ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
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
        move_relative(focused_client, RESIZE_STEP, 0);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_H)))
        move_relative(focused_client, -RESIZE_STEP, 0); 
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_J)))
        move_relative(focused_client, 0, RESIZE_STEP);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_K)))
        move_relative(focused_client, 0, -RESIZE_STEP);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Q)))
        XKillClient(display, focused_client->win);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_Tab)))
        cycle_focus(focused_client);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_U)))
        resize_relative(focused_client, -RESIZE_STEP, 0);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_I)))
        resize_relative(focused_client, 0, RESIZE_STEP);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_O)))
        resize_relative(focused_client, 0, -RESIZE_STEP);
    else if ((ev->state &Mod4Mask) && (ev->keycode == XKeysymToKeycode(display, XK_P)))
        resize_relative(focused_client, RESIZE_STEP, 0);
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
        XDestroyWindow(display, c->dec);
        delete_client(c);
        free(c);
    }

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
    if (focused_client) 
        set_color(focused_client, UNFOCUS_COLOR);

    if (c)
    {
        set_color(c, FOCUS_COLOR);
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
    c->y = wa->y + TITLE_HEIGHT;
    c->w = wa->width;
    c->h = wa->height;

    decorate_new_client(c);
    XMapWindow(display, c->win);
    manage_client_focus(c);
    move_absolute(c, 30, 30);
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
    XMoveWindow(display, c->win, x, y);
    XMoveWindow(display, c->dec, x - BORDER_WIDTH, y - TITLE_HEIGHT - BORDER_WIDTH);
    c->x = x;
    c->y = y;
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
    XResizeWindow(display, c->win, x, y);
    XResizeWindow(display, c->dec, x, y + TITLE_HEIGHT);
    c->w = x;
    c->h = y;
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
    XSetWindowBackground(display, c->dec, color);
    XSetWindowBorder(display, c->dec, color);
    XClearWindow(display, c->dec);
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

    display = XOpenDisplay(NULL);
    root = DefaultRootWindow(display);
    screen = DefaultScreen(display);
    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);
    grab_keys(root);
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









