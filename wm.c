/* Copyright (c) 2018 Joshua L Ervin. All rights reserved. */
/* Licensed under the MIT License. See the LICENSE file in the project root for full license information. */

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>
#include <X11/cursorfont.h>
#include <X11/Xft/Xft.h>

#include "config.h"
#include "globals.h"
#include "ipc.h"
#include "types.h"
#include "utils.h"

static struct client *f_client = NULL; /* focused client */
static struct client *c_list[WORKSPACE_NUMBER]; /* 'stack' of managed clients in drawing order */
static struct client *f_list[WORKSPACE_NUMBER]; /* ordered lists for clients to be focused */
static struct monitor *m_list = NULL; /* All saved monitors */
static struct config conf; /* gloabl config */
static int ws_m_list[WORKSPACE_NUMBER]; /* Mapping from workspaces to associated monitors */
static int curr_ws = 0;
static int m_count = 0;
static Cursor move_cursor, normal_cursor;
static Display *display;
static Atom net_atom[NetLast], wm_atom[WMLast], net_berry[BerryLast];
static Window root, check, nofocus;
static bool running = true;
static bool debug = false;
static int screen, display_width, display_height;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static XftColor xft_focus_color, xft_unfocus_color;
static XftFont *font;
static char global_font[MAXLEN] = DEFAULT_FONT;
static XRenderColor r_color;
static GC gc;
static Atom utf8string;

/* All functions */

/* Client management functions */
static void client_cardinal_focus(struct client *c, int dir);
static void client_center(struct client *c);
static void client_center_in_rect(struct client *c, int x, int y, int w, int h);
static void client_close(struct client *c);
static void client_decorations_create(struct client *c);
static void client_decorations_destroy(struct client *c);
static void client_delete(struct client *c);
static void client_fullscreen(struct client *c, bool max);
static void client_hide(struct client *c);
static void client_manage_focus(struct client *c);
static void client_move_absolute(struct client *c, int x, int y);
static void client_move_relative(struct client *c, int x, int y);
static void client_move_to_front(struct client *c);
static void client_monocle(struct client *c);
static void client_place(struct client *c);
static void client_raise(struct client *c);
static void client_refresh(struct client *c);
static void client_resize_absolute(struct client *c, int w, int h);
static void client_resize_relative(struct client *c, int w, int h);
static void client_save(struct client *c, int ws);
static void client_send_to_ws(struct client *c, int ws);
static void client_set_color(struct client *c, unsigned long i_color, unsigned long b_color);
static void client_set_input(struct client *c);
static void client_set_title(struct client *c);
static void client_show(struct client *c);
static void client_snap_left(struct client *c);
static void client_snap_right(struct client *c);
static void client_toggle_decorations(struct client *c);
static void client_set_status(struct client *c);

/* EWMH functions */
static void ewmh_set_fullscreen(struct client *c, bool fullscreen);
static void ewmh_set_viewport(void);
static void ewmh_set_focus(struct client *c);
static void ewmh_set_desktop(struct client *c, int ws);
static void ewmh_set_frame_extents(struct client *c);
static void ewmh_set_client_list(void);
static void ewmh_set_desktop_names(void);
static void ewmh_set_active_desktop(int ws);

/* Event handlers */
static void handle_client_message(XEvent *e);
static void handle_configure_notify(XEvent *e);
static void handle_configure_request(XEvent *e);
static void handle_focus(XEvent *e);
static void handle_map_request(XEvent *e);
static void handle_unmap_notify(XEvent *e);
static void handle_button_press(XEvent *e);
static void handle_expose(XEvent *e);
static void handle_property_notify(XEvent *e);

/* IPC client functions */
static void ipc_move_absolute(long *d);
static void ipc_move_relative(long *d);
static void ipc_monocle(long *d);
static void ipc_raise(long *d);
static void ipc_resize_absolute(long *d);
static void ipc_resize_relative(long *d);
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
static void ipc_fullscreen_state(long *d);
static void ipc_snap_left(long *d);
static void ipc_snap_right(long *d);
static void ipc_cardinal_focus(long *d);
static void ipc_cycle_focus(long *d);
static void ipc_pointer_focus(long *d);
static void ipc_top_gap(long *d);
static void ipc_smart_place(long *d);
static void ipc_save_monitor(long *d);
static void ipc_tf_color(long *d);
static void ipc_tu_color(long *d);
static void ipc_draw_text(long *d);
static void ipc_edge_lock(long *d);
static void ipc_set_font(long *d);
static void ipc_json_status(long *d);
static void ipc_quit(long *d);

static void monitors_free(void);
static void monitors_setup(void);

static void close_wm(void);
static void draw_text(struct client *c, bool focused);
static struct client* get_client_from_window(Window w);
static void load_color(XftColor *dest_color, unsigned long raw_color);
static void load_config(char *conf_path);
static void manage_new_window(Window w, XWindowAttributes *wa);
static int manage_xsend_icccm(struct client *c, Atom atom);
static void refresh_config(void);
static void run(void);
static bool safe_to_focus(int ws);
static void setup(void);
static void switch_ws(int ws);
static void usage(void);
static void version(void);
static int xerror(Display *display, XErrorEvent *e);

/* Native X11 Event handler */
static void (*event_handler[LASTEvent])(XEvent *e) = {
    [MapRequest]       = handle_map_request,
    [UnmapNotify]      = handle_unmap_notify,
    [ConfigureNotify]  = handle_configure_notify,
    [ConfigureRequest] = handle_configure_request,
    [ClientMessage]    = handle_client_message,
    [ButtonPress]      = handle_button_press,
    [PropertyNotify]   = handle_property_notify,
    [Expose]           = handle_expose,
    [FocusIn]          = handle_focus
};

static void (*ipc_handler[IPCLast])(long *) = {
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
    [IPCFullscreenState]          = ipc_fullscreen_state,
    [IPCSnapLeft]                 = ipc_snap_left,
    [IPCSnapRight]                = ipc_snap_right,
    [IPCCardinalFocus]            = ipc_cardinal_focus,
    [IPCCycleFocus]               = ipc_cycle_focus,
    [IPCPointerFocus]             = ipc_pointer_focus,
    [IPCSaveMonitor]              = ipc_save_monitor,
    [IPCTitleFocusColor]          = ipc_tf_color,
    [IPCTitleUnfocusColor]        = ipc_tu_color,
    [IPCTopGap]                   = ipc_top_gap,
    [IPCSmartPlace]               = ipc_smart_place,
    [IPCDrawText]                 = ipc_draw_text,
    [IPCEdgeLock]                 = ipc_edge_lock,
    [IPCSetFont]                  = ipc_set_font,
    [IPCJSONStatus]               = ipc_json_status,
    [IPCQuit]                     = ipc_quit
};

/* Give focus to the given client in the given direction */
static void
client_cardinal_focus(struct client *c, int dir)
{
    struct client *tmp, *focus_next;
    int min;

    tmp = c_list[curr_ws];
    focus_next = NULL;
    min = INT_MAX;

    while (tmp != NULL) {
        int dist = euclidean_distance(c, tmp);
        switch (dir) {
            case EAST:
                D fprintf(stderr, WINDOW_MANAGER_NAME": Focusing EAST\n");
                if (tmp->geom.x > c->geom.x && dist < min) {
                    min = dist;
                    focus_next = tmp;
                }
                break;
            case SOUTH:
                D fprintf(stderr, WINDOW_MANAGER_NAME": Focusing SOUTH\n");
                if (tmp->geom.y > c->geom.y && dist < min) {
                    min = dist;
                    focus_next = tmp;
                }
                break;
            case WEST:
                D fprintf(stderr, WINDOW_MANAGER_NAME": Focusing WEST\n");
                if (tmp->geom.x < c->geom.x && dist < min) {
                    min = dist;
                    focus_next = tmp;
                }
                break;
            case NORTH:
                D fprintf(stderr, WINDOW_MANAGER_NAME": Focusing NORTH\n");
                if (tmp->geom.y < c->geom.y && dist < min) {
                    min = dist;
                    focus_next = tmp;
                }
                break;
        }
        tmp = tmp->next;
    }

    if (focus_next == NULL) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Cannot cardinal focus, no valid windows found\n");
        return;
    } else {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Valid window found in direction %d, focusing\n", dir);
        client_manage_focus(focus_next);
    }
}

/* Move a client to the center of the screen, centered vertically and horizontally
 * by the middle of the Client
 */
static void
client_center(struct client *c)
{
    int mon;
    D fprintf(stderr, WINDOW_MANAGER_NAME": Centering Client\n");
    mon = ws_m_list[c->ws];
    client_center_in_rect(c, m_list[mon].x, m_list[mon].y, m_list[mon].width, m_list[mon].height);
}

static void
client_center_in_rect(struct client *c, int x, int y, int w, int h) 
{
    client_move_absolute(c,
                         round_k(x + w / 2 - c->geom.width / 2),
                         round_k(y + (h + conf.top_gap) / 2 - c->geom.height / 2));
    client_refresh(c); // in case we went over the top gap
}

/* Close connection to the current display */
static void
close_wm(void)
{
    D fprintf(stderr, WINDOW_MANAGER_NAME": Shutting down window manager\n");

    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        while (c_list[i] != NULL)
            client_delete(c_list[i]);
    }


    XDeleteProperty(display, root, net_berry[BerryWindowStatus]);
    XDeleteProperty(display, root, net_berry[BerryFontProperty]);
    XDeleteProperty(display, root, net_atom[NetSupported]);

    D fprintf(stderr, WINDOW_MANAGER_NAME": Closing display...\n");
    XCloseDisplay(display);
}

static void
draw_text(struct client *c, bool focused)
{
    XftDraw *draw;
    XftColor *xft_render_color;
    XGlyphInfo extents;
    int x, y, len;

    if (!conf.draw_text) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": drawing text disabled\n");
        return;
    }
    
    if (!c->decorated) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Client not decorated, not drawing text\n");
        return;
    }

    XftTextExtentsUtf8(display, font, (XftChar8 *)c->title, strlen(c->title), &extents);
    y = (conf.t_height / 2) + ((extents.y) / 2);
    x = !conf.t_center ? TITLE_X_OFFSET : (c->geom.width - extents.width) / 2;

    for (len = strlen(c->title); len >= 0; len--) {
        XftTextExtentsUtf8(display, font, (XftChar8 *)c->title, len, &extents);
        if (extents.xOff < c->geom.width)
            break;
    }

    D fprintf(stderr, WINDOW_MANAGER_NAME": Text height is %u\n", extents.height);

    if (extents.y > conf.t_height) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Text is taller than title bar height, not drawing text\n");
        return;
    }

    D fprintf(stderr, WINDOW_MANAGER_NAME": Drawing text on client\n");
    XClearWindow(display, c->dec);
    draw = XftDrawCreate(display, c->dec, DefaultVisual(display, screen), DefaultColormap(display, screen));
    xft_render_color = focused ? &xft_focus_color : &xft_unfocus_color;
    XftDrawStringUtf8(draw, xft_render_color, font, x, y, (XftChar8 *) c->title, strlen(c->title));
    XftDrawDestroy(draw);
}

/* Communicate with the given Client, kindly telling it to close itself
 * and terminate any associated processes using the WM_DELETE_WINDOW protocol
 */
static void
client_close(struct client *c)
{
    XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = c->window;
    ev.xclient.message_type = wm_atom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = wm_atom[WMDeleteWindow];
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(display, c->window, False, NoEventMask, &ev);
    D fprintf(stderr, WINDOW_MANAGER_NAME": Closing window...\n");
}

/* Create new "dummy" windows to be used as decorations for the given client */
static void
client_decorations_create(struct client *c)
{
    D fprintf(stderr, WINDOW_MANAGER_NAME": Decorating new client\n");
    int w = c->geom.width + 2 * conf.i_width;
    int h = c->geom.height + 2 * conf.i_width + conf.t_height;
    int x = c->geom.x - conf.i_width - conf.b_width;
    int y = c->geom.y - conf.i_width - conf.b_width - conf.t_height; 
    Window dec = XCreateSimpleWindow(display, root, x, y, w, h, conf.b_width,
            conf.bu_color, conf.bf_color);

    D fprintf(stderr, WINDOW_MANAGER_NAME": Mapping new decorations\n");
    c->dec = dec;
    c->decorated = true;
    XSelectInput (display, c->dec, ExposureMask);
    XGrabButton(display, 1, AnyModifier, c->dec, True, ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XMapWindow (display, c->dec);
    draw_text(c, true);
    ewmh_set_frame_extents(c);
    client_set_status(c);
}

/* Destroy any "dummy" windows associated with the given Client as decorations */
static void 
client_decorations_destroy(struct client *c)
{
    D fprintf(stderr, WINDOW_MANAGER_NAME": Removing decorations\n");
    c->decorated = false;
    XUnmapWindow(display, c->dec);
    XDestroyWindow(display, c->dec);
    ewmh_set_frame_extents(c);
    client_set_status(c);
}

/* Remove the given Client from the list of currently managed clients 
 * Does not free the given client from memory. 
 * */
static void
client_delete(struct client *c)
{
    int ws;
    ws = c->ws;

    if (ws == -1) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Cannot delete client, not found\n"); 
        return;
    } else {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Deleting client on workspace %d\n", ws); 
    }

    /* Delete in the stack */
    if (c_list[ws] == c) {
        c_list[ws] = c_list[ws]->next;
    } else {
        struct client *tmp = c_list[ws];
        while (tmp != NULL && tmp->next != c)
            tmp = tmp->next;

        if (tmp)
            tmp->next = tmp->next->next;
    }

    /* Delete in the focus list */
    /* I'll factor this out later */
    /* Or actually it might not be so easy... */
    if (f_list[ws] == c) {
        f_list[ws] = f_list[ws]->f_next;
    } else {
        struct client *tmp = f_list[ws];
        while (tmp != NULL && tmp->f_next != c)
            tmp = tmp->f_next;

        if (tmp)
            tmp->f_next = tmp->f_next->f_next;
    }

    if (c_list[ws] == NULL)
        f_client = NULL;

    ewmh_set_client_list();
}

static void
monitors_free(void)
{
    free(m_list);
    m_list = NULL;
}

/* Set the given Client to be fullscreen. Moves the window to fill the dimensions
 * of the given display. 
 * Updates the value of _NET_WM_STATE_FULLSCREEN to reflect fullscreen changes
 */
static void
client_fullscreen(struct client *c, bool max)
{
    int mon;
    mon = ws_m_list[c->ws];
    // save the old geometry values so that we can toggle between fulscreen mode

    if (!c->fullscreen) {
        // if the client is not currently fullscreen, maximize it to fill the current screen
        ewmh_set_fullscreen(c, true);
        if (max) {
            c->prev.x = c->geom.x;
            c->prev.y = c->geom.y;
            c->prev.width = c->geom.width;
            c->prev.height = c->geom.height;
            client_move_absolute(c, m_list[mon].x, m_list[mon].y);
            client_resize_absolute(c, m_list[mon].width, m_list[mon].height);
        }
    } else {
        // if the client is currently fulscreen, revert it's state to its original size
        ewmh_set_fullscreen(c, false);
        if (max) {
            client_move_absolute(c, c->prev.x, c->prev.y);
            client_resize_absolute(c, c->prev.width, c->prev.height);
        }
    }

    c->fullscreen = !c->fullscreen;
}

/* Focus the next window in the list. Windows are sorted by the order in which they are 
 * created (mapped to the window manager)
 */
static void
focus_next(struct client *c)
{
    if (c == NULL)
        return;

    int ws;
    ws = c->ws;

    if (f_list[ws] == c && f_list[ws]->f_next == NULL) {
        client_manage_focus(f_list[ws]);
        return;
    }

    struct client *tmp;
    tmp = c->f_next == NULL ? f_list[ws] : c->f_next;
    client_manage_focus(tmp);
}

/* Returns the struct client associated with the given struct Window */
static struct client*
get_client_from_window(Window w)
{
    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        for (struct client *tmp = c_list[i]; tmp != NULL; tmp = tmp->next) {
            if (tmp->window == w)
                return tmp;
            else if (tmp->decorated && tmp->dec == w)
                return tmp;
        }
    }

    return NULL;
}

/* Redirect an XEvent from berry's client program, berryc */
static void
handle_client_message(XEvent *e)
{
    XClientMessageEvent *cme = &e->xclient;
    long cmd, *data;

    if (cme->message_type == net_berry[BerryClientEvent]) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Recieved event from berryc\n");
        if (cme->format != 32) {
			D fprintf(stderr, WINDOW_MANAGER_NAME": Wrong format, ignoring event\n");
			return;
		}
        cmd = cme->data.l[0];
        data = cme->data.l;
        ipc_handler[cmd](data);
    }
}

static void
handle_button_press(XEvent *e)
{
    /* Much credit to the authors of dwm for
     * this function. 
     */
    XButtonPressedEvent *bev = &e->xbutton;
    XEvent ev; 
    struct client *c;
    int x, y, ocx, ocy, nx, ny, di;
    unsigned int dui;
    Window dummy;

    XQueryPointer(display, root, &dummy, &dummy, &x, &y, &di, &di, &dui);
    D fprintf(stderr, WINDOW_MANAGER_NAME": Handling button press event\n");
    c = get_client_from_window(bev->window);
    if (c == NULL)
        return;
    if (c != f_client)
        client_manage_focus(c);
    ocx = c->geom.x;
    ocy = c->geom.y;
    if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, move_cursor, CurrentTime) != GrabSuccess)
        return;
    do {
        XMaskEvent(display, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
        switch (ev.type) {
            case ConfigureRequest:
            case Expose:
            case MapRequest:
                draw_text(c, true);
                break;
            case MotionNotify:
                D fprintf(stderr, WINDOW_MANAGER_NAME": Handling motion notify event\n");
                nx = ocx + (ev.xmotion.x - x);
                ny = ocy + (ev.xmotion.y - y);
                if (conf.edge_lock)
                    client_move_relative(c, nx - c->geom.x, ny - c->geom.y);
                else
                    client_move_absolute(c, nx, ny);
                break;
        }
    } while (ev.type != ButtonRelease);
    XUngrabPointer(display, CurrentTime);
}

static void
handle_expose(XEvent *e)
{
    XExposeEvent *ev = &e->xexpose;
    struct client *c;
    bool focused;

    D fprintf(stderr, WINDOW_MANAGER_NAME": Handling expose event\n");
    c = get_client_from_window(ev->window);
    if (c == NULL) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Expose event client not found, exiting\n");
        return;
    }
    focused = c == f_client;
    draw_text(c, focused);
}

static void
handle_focus(XEvent *e)
{
    XFocusChangeEvent *ev = &e->xfocus;
    UNUSED(ev);
    return;
    if (ev->window != f_client->window)
        client_manage_focus(f_client);
}

static void
handle_property_notify(XEvent *e)
{
    XPropertyEvent *ev = &e->xproperty;
    struct client *c;

    D fprintf(stderr, WINDOW_MANAGER_NAME": Handling property notify event\n");
    c = get_client_from_window(ev->window);

    if (c == NULL)
        return;

    if (ev->state == PropertyDelete)
        return;

    if (ev->atom == net_atom[NetWMName]) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Updating client title\n");
        client_set_title(c);
        draw_text(c, c == f_client);
    }
}

static void
handle_configure_notify(XEvent *e)
{
    XConfigureEvent *ev = &e->xconfigure;

    if (ev->window == root)
        return;

    D fprintf(stderr, WINDOW_MANAGER_NAME": Handling configure notify event\n");

    monitors_free();
    monitors_setup();
}

static void
handle_configure_request(XEvent *e) 
{
    struct client *c;
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;

    D fprintf(stderr, WINDOW_MANAGER_NAME": Handling configure request event\n");

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
        client_refresh(c);
}

static void
handle_map_request(XEvent *e)
{
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;

    D fprintf(stderr, WINDOW_MANAGER_NAME": Handling map request event\n");

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
    struct client *c;
    c = get_client_from_window(ev->window);

    if (c != NULL) {
        focus_next(c);
        if (c->decorated)
            XDestroyWindow(display, c->dec);
        client_delete(c);
        free(c);
        client_raise(f_client);
    }
}

/* Hides the given Client by moving it outside of the visible display */
static void
client_hide(struct client *c)
{
    if (!c->hidden) {
        c->x_hide = c->geom.x;
        D fprintf(stderr, WINDOW_MANAGER_NAME": Hiding client\n");
        client_move_absolute(c, display_width + conf.b_width, c->geom.y);
        c->hidden = true;
    }
}

static void
ipc_move_absolute(long *d)
{
    int x, y;

    if (f_client == NULL)
        return;

    x = d[1];
    y = d[2];

    client_move_absolute(f_client, x, y);
}

static void
ipc_move_relative(long *d)
{
    int x, y;

    if (f_client == NULL)
        return;

    x = d[1];
    y = d[2];

    client_move_relative(f_client, x, y);
}

static void
ipc_monocle(long *d)
{
    UNUSED(d);
    if (f_client == NULL)
        return;

    client_monocle(f_client);
}

static void
ipc_raise(long *d) 
{
    UNUSED(d);
    if (f_client == NULL)
        return;

    client_raise(f_client);
}

static void 
ipc_resize_absolute(long *d)
{
    int w, h;

    if (f_client == NULL)
        return;

    w = d[1];
    h = d[2];

    client_resize_absolute(f_client, w, h);
}

static void 
ipc_resize_relative(long *d)
{
    int w, h;

    if (f_client == NULL)
        return;

    w = d[1];
    h = d[2];

    client_resize_relative(f_client, w, h);
}

static void 
ipc_toggle_decorations(long *d)
{
    UNUSED(d);
    if (f_client == NULL)
        return ;

    client_toggle_decorations(f_client);
}

static void
ipc_window_close(long *d)
{
    UNUSED(d);
    if (f_client == NULL)
        return;

    client_close(f_client);
}

static void
ipc_window_center(long *d)
{
    UNUSED(d);
    if (f_client == NULL)
        return;
    client_center(f_client);
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

    refresh_config();
    client_raise(f_client);
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
    switch_ws(ws);
}

static void
ipc_send_to_ws(long *d)
{
    if (f_client == NULL)
        return;

    int ws = d[1];
    client_send_to_ws(f_client, ws - 1);
}

static void
ipc_fullscreen(long *d)
{
    UNUSED(d);
    if (f_client == NULL)
        return;

    client_fullscreen(f_client, true);
}

static void
ipc_fullscreen_state(long *d)
{
    UNUSED(d);

    if (f_client == NULL)
        return;

    client_fullscreen(f_client, false);
}

static void
ipc_snap_left(long *d)
{
    UNUSED(d);
    if (f_client == NULL)
        return;

    client_snap_left(f_client);
}

static void
ipc_snap_right(long *d)
{
    UNUSED(d);
    if (f_client == NULL)
        return;

    client_snap_right(f_client);
}

static void
ipc_cardinal_focus(long *d)
{
    int dir = d[1];
    client_cardinal_focus(f_client, dir);
}

static void
ipc_cycle_focus(long *d)
{
    UNUSED(d);
    focus_next(f_client);
}

static void
ipc_pointer_focus(long *d)
{
    UNUSED(d);
    /* Shoutout to vain for this methodology */
    int x, y, di;
    unsigned int dui;
    Window child, dummy;
    struct client *c;

    XQueryPointer(display, root, &dummy, &child, &x, &y, &di, &di, &dui);
    c = get_client_from_window(child);

    if (c != NULL)
    {
        /* Focus the client for either type of event 
         * However, don't change focus if the client is already focused
         * otherwise menu's will be hidden behind the parent window
         */
        if (c != f_client)
            client_manage_focus(c);
    }
}

static void
ipc_top_gap(long *d)
{
    int data = d[1];
    conf.top_gap = data;

    refresh_config();
}

static void
ipc_smart_place(long *d) {
    int data = d[1];
    conf.smart_place = data;

    refresh_config();
}

static void
ipc_save_monitor(long *d)
{
    int ws, mon;
    ws = d[1];
    mon = d[2];

    if (mon >= m_count) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Cannot save monitor, number is too high\n");
        return;
    }

    D fprintf(stderr, WINDOW_MANAGER_NAME": Saving ws %d to monitor %d\n", ws, mon);

    /* Associate the given workspace to the given monitor */
    ws_m_list[ws] = mon;
    ewmh_set_viewport();
}

static void
ipc_tf_color(long *d) 
{
    unsigned long nc;
    nc = d[1];

    load_color(&xft_focus_color, nc);
    refresh_config();
}

static void
ipc_tu_color(long *d) 
{
    unsigned long nc;
    nc = d[1];

    load_color(&xft_unfocus_color, nc);
    refresh_config();
}

static void
ipc_draw_text(long *d)
{
    unsigned long draw;
    draw = d[1];

    if (draw == 0)
        conf.draw_text = false;
    else
        conf.draw_text = true;

    refresh_config();
}

static void
ipc_edge_lock(long *d)
{
    unsigned long draw;
    draw = d[1];

    if (draw == 0)
        conf.edge_lock = false;
    else
        conf.edge_lock = true;

    refresh_config();
}

static void
ipc_set_font(long *d)
{
    UNUSED(d);
    XTextProperty font_prop;
    char** font_list;
    int err, n;
    D fprintf(stderr, WINDOW_MANAGER_NAME": Handling new set_font request\n");

    font_list = NULL;
    D fprintf(stderr, WINDOW_MANAGER_NAME": Getting text property\n");
    XGetTextProperty(display, root, &font_prop, net_berry[BerryFontProperty]);
    D fprintf(stderr, WINDOW_MANAGER_NAME": Converting to text list\n");
    err = XmbTextPropertyToTextList(display, &font_prop, &font_list, &n);
    strcpy(global_font, font_list[0]);
    D fprintf(stderr, WINDOW_MANAGER_NAME": Opening font by name\n");
    font = XftFontOpenName(display, screen, global_font);
    if (font == NULL) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Error, could not open font name\n");
        return;
    }
    refresh_config();
    if (err >= Success && n > 0 && *font_list)
        XFreeStringList(font_list);
    XFree(font_prop.value);
}

static void
ipc_json_status(long *d)
{
    unsigned long json;
    json = d[1];

    if (json == 0)
        conf.json_status = false;
    else
        conf.json_status = true;

    refresh_config();
}

static void
ipc_quit(long *d)
{
    UNUSED(d);
    running = false;
}

static void
load_color(XftColor *dest_color, unsigned long raw_color)
{
    XColor x_color;
    x_color.pixel = raw_color;
    XQueryColor(display, DefaultColormap(display, screen), &x_color);
    r_color.blue = x_color.blue;
    r_color.green = x_color.green;
    r_color.red = x_color.red;
    r_color.alpha = DEFAULT_ALPHA;

    XftColorFree(display, DefaultVisual(display, screen), DefaultColormap(display, screen), dest_color);
    XftColorAllocValue(display, DefaultVisual(display, screen), DefaultColormap(display, screen),
            &r_color, dest_color);
}


static void
load_config(char *conf_path)
{
    if (fork() == 0) {
        setsid();
        execl(conf_path, conf_path, NULL);
        D fprintf(stderr, WINDOW_MANAGER_NAME": CONFIG PATH: %s\n", conf_path);
    }
}

static void
client_manage_focus(struct client *c)
{
    if (c != NULL && f_client != NULL) {
        client_set_color(f_client, conf.iu_color, conf.bu_color);
        draw_text(f_client, false);
        manage_xsend_icccm(c, wm_atom[WMTakeFocus]);
    }

    if (c != NULL) {
        client_move_to_front(c);
        client_set_color(c, conf.if_color, conf.bf_color);
        draw_text(c, true);
        client_raise(c);
        client_set_input(c);
        ewmh_set_focus(c);
        manage_xsend_icccm(c, wm_atom[WMTakeFocus]);
    } else { //client is null, might happen when switching to a new workspace
             // without any active clients
        D fprintf(stderr, WINDOW_MANAGER_NAME": Giving focus to dummy window\n");
        f_client = NULL;
        XSetInputFocus(display, nofocus, RevertToPointerRoot, CurrentTime);
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
                &prop_ret) == Success) {
        if (prop_ret) {
            prop = ((Atom *)prop_ret)[0];
            if (prop == net_atom[NetWMWindowTypeDock] ||
                prop == net_atom[NetWMWindowTypeToolbar] ||
                prop == net_atom[NetWMWindowTypeUtility] ||
                prop == net_atom[NetWMWindowTypeDialog] ||
                prop == net_atom[NetWMWindowTypeMenu]) {
                D fprintf(stderr, WINDOW_MANAGER_NAME": Window is of type dock, toolbar, utility, menu, or splash: not managing\n");
                D fprintf(stderr, WINDOW_MANAGER_NAME": Mapping new window, not managed\n");
                XMapWindow(display, w);
                return;
            }
        }
    }

    // Make sure we aren't trying to map the same window twice
    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        for (struct client *tmp = c_list[i]; tmp; tmp = tmp->next) {
            if (tmp->window == w) {
                D fprintf(stderr, WINDOW_MANAGER_NAME": Error, window already mapped. Not mapping.\n");
                return;
            }
        }
    }

    // Get class information for the current window
    XClassHint ch;
    if (XGetClassHint(display, w, &ch) > Success) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": client has class %s\n", ch.res_class);
        D fprintf(stderr, WINDOW_MANAGER_NAME": client has name %s\n", ch.res_name);
        if (ch.res_class)
            XFree(ch.res_class);
        if (ch.res_name)
            XFree(ch.res_name);
    } else {
        D fprintf(stderr, WINDOW_MANAGER_NAME": could not retrieve client class name\n");
    }

    struct client *c;
    c = malloc(sizeof(struct client));
    if (c == NULL) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Error, malloc could not allocated new window\n");
        return;
    }
    c->window = w;
    c->ws = curr_ws;
    c->geom.x = wa->x;
    c->geom.y = wa->y;
    c->geom.width = wa->width;
    c->geom.height = wa->height;
    c->hidden = false;
    c->fullscreen = false;
    c->mono = false;

    client_decorations_create(c);
    client_set_title(c);
    XMapWindow(display, c->window);
    client_refresh(c); /* using our current factoring, w/h are set incorrectly */
    client_save(c, curr_ws);
    client_manage_focus(c);
    client_place(c);
    ewmh_set_desktop(c, c->ws);
    ewmh_set_client_list();
    XSelectInput(display, c->window, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
}

static int
manage_xsend_icccm(struct client *c, Atom atom)
{
    /* This is from a dwm patch by Brendan MacDonell:
     * http://lists.suckless.org/dev/1104/7548.html */

    int n;
    Atom *protocols;
    int exists = 0;
    XEvent ev;

    if (XGetWMProtocols(display, c->window, &protocols, &n)) {
        while (!exists && n--)
            exists = protocols[n] == atom;
        XFree(protocols);
    }

    if (exists) {
        ev.type = ClientMessage;
        ev.xclient.window = c->window;
        ev.xclient.message_type = wm_atom[WMProtocols];
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = atom;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(display, c->window, False, NoEventMask, &ev);
    }

    return exists;
}

static void
client_move_absolute(struct client *c, int x, int y)
{
    int dest_x = x;
    int dest_y = y;

    if (c->decorated) {
        dest_x = x + conf.i_width + conf.b_width;
        dest_y = y + conf.i_width + conf.b_width + conf.t_height;
    }

    /* move relative to where decorations should go */
    XMoveWindow(display, c->window, dest_x, dest_y);
    if (c->decorated){
        XMoveWindow(display, c->dec, x, y);
    }

    c->geom.x = x;
    c->geom.y = y;

    if (c->mono)
        c->mono = false;

    client_set_status(c);
}

static void
client_move_relative(struct client *c, int x, int y) 
{
    /* Constrain the current client to the w/h of display */
    if (conf.edge_lock) {
        int dx, dy, mon;
        mon = ws_m_list[c->ws];

        /* Lock on the right side of the screen */
        if (c->geom.x + c->geom.width + x > m_list[mon].width + m_list[mon].x)
            dx = m_list[mon].width + m_list[mon].x - c->geom.width;
        /* Lock on the left side of the screen */
        else if (c->geom.x + x < m_list[mon].x)
            dx = m_list[mon].x; 
        else
            dx = c->geom.x + x;

        /* Lock on the bottom of the screen */
        if (c->geom.y + c->geom.height + y > m_list[mon].height + m_list[mon].y)
            dy = m_list[mon].height + m_list[mon].y - c->geom.height;
        /* Lock on the top of the screen */
        else if (c->geom.y + y < m_list[mon].y + conf.top_gap)
            dy = m_list[mon].y + conf.top_gap;
        else
            dy = c->geom.y + y;

        client_move_absolute(c, dx, dy);
    } else {
        client_move_absolute(c, c->geom.x + x, c->geom.y + y);
    }
}

static void
client_move_to_front(struct client *c)
{
    int ws;
    ws = c->ws;

    /* If we didn't find the client */
    if (ws == -1)
        return;

    /* If the Client is at the front of the list, ignore command */
    if (c_list[ws] == c || c_list[ws]->next == NULL)
        return;

    struct client *tmp;
    for (tmp = c_list[ws]; tmp->next != NULL; tmp = tmp->next)
        if (tmp->next == c)
            break;

    if (tmp && tmp->next)
        tmp->next = tmp->next->next; /* remove the Client from the list */
    c->next = c_list[ws]; /* add the client to the front of the list */
    c_list[ws] = c;
}

static void
client_monocle(struct client *c)
{
    int mon;
    mon = ws_m_list[c->ws];
    if (c->mono) {
        client_move_absolute(c, c->prev.x, c->prev.y);
        client_resize_absolute(c, c->prev.width, c->prev.height);
    } else {
        c->prev.x = c->geom.x;
        c->prev.y = c->geom.y;
        c->prev.width = c->geom.width;
        c->prev.height = c->geom.height;
        client_move_absolute(c, m_list[mon].x, m_list[mon].y + conf.top_gap); 
        client_resize_absolute(c, m_list[mon].width, m_list[mon].height - conf.top_gap);
        c->mono = true;
    }

    //c->mono = !c->mono;
}

static void
client_place(struct client *c) 
{
    int width, height, mon, count, max_height;

    mon = ws_m_list[c->ws];
    width = m_list[mon].width / PLACE_RES;
    height = m_list[mon].height / PLACE_RES;

    // If this is the first window in the workspace, we can simply center
    // it. Also center it if the user wants to disable smart placeement.
    if (f_list[curr_ws]->next == NULL || !conf.smart_place) {
        client_center(c);
        return;
    }

    uint16_t opt[height][width];

    // Initialize array to all 1's
    //memset(opt, 1, sizeof(opt[0][0]) * width * height);
    //memset(opt, 1, sizeof(uint16_t) * width * height);
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            opt[i][j] = 1;
        }
    }

    // Fill in the top gap
    for (int i = 0; i < conf.top_gap / PLACE_RES + 1; i++) {
        for (int j = 0; j < width / PLACE_RES; j++) {
            opt[i][j] = 0;
        }
    }

    for (struct client *tmp = f_list[curr_ws]; tmp != NULL; tmp = tmp->next) {
        if (tmp != c) {
            struct client_geom *geom = &tmp->geom;
            for (int i = geom->y / PLACE_RES; 
                 i < (geom->y / PLACE_RES) + (geom->height / PLACE_RES) && i < width;
                 i++) {
                for (int j = geom->x / PLACE_RES; 
                     j < (geom->x / PLACE_RES) + (geom->width / PLACE_RES) && j < width;
                     j++) {
                    opt[i][j] = 0;
                }
            }
        }
    }

    // fill in the OPT matrix
    for (int i = 1; i < height; i++) {
        for (int j = 0; j < width; j++) {
            opt[i][j] = opt[i][j] == 0 ? 0 : opt[i-1][j] + 1;
        }
    }

    count = 0;
    max_height = INT_MAX;
    for (int i = height - 1; i >= 0; i--) {
        for (int j = 0; j < width; j++) {
            while (j < width && opt[i][j] >= c->geom.height / PLACE_RES) {
                max_height = MIN(max_height, opt[i][j]);
                count++;
                j++;
            }
            // the window WILL fit here
            if (count >= c->geom.width / PLACE_RES) {
                client_move_absolute(c, 
                                    (j - count) * PLACE_RES + (count * PLACE_RES - c->geom.width) / 2,
                                    (i - max_height) * PLACE_RES + (max_height * PLACE_RES - c->geom.height) / 2);
                return;
            }
            count = 0;
        }
    }
}

static void
client_raise(struct client *c)
{
    if (c != NULL) {
        if (!c->decorated) {
            XRaiseWindow(display, c->window);
        } else {
            // how may active clients are there on our workspace
            int count, i;
            count = 0;
            for (struct client *tmp = c_list[c->ws]; tmp != NULL; tmp = tmp->next) {
                count++;
            }

            if (count == 0)
                return;

            Window wins[count*2];

            i = 0;
            for (struct client *tmp = c_list[c->ws]; tmp != NULL; tmp = tmp->next) {
                wins[i] = tmp->window;
                wins[i+1] = tmp->dec;
                i += 2;
            }
            XRestackWindows(display, wins, count*2);
        }
    }
}

static void monitors_setup(void)
{
    XineramaScreenInfo *m_info;
    int n;

    if (!XineramaIsActive(display)) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Xinerama not active, cannot read monitors\n");
        return;
    }

    m_info = XineramaQueryScreens(display, &n);
    if (m_info == NULL) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Xinerama could not query screens\n");
        return;
    }
    D fprintf(stderr, WINDOW_MANAGER_NAME": Found %d screens active\n", n);
    m_count = n;

    /* First, we need to decide which monitors are unique.
     * Non-unique monitors can become a problem when displays
     * are mirrored. They will share the same information (which something
     * like xrandr will handle for us) but will have the exact same information.
     * We want to avoid creating duplicate structs for the same monitor if we dont
     * need to 
     */

    // TODO: Add support for repeated displays

    m_list = malloc(sizeof(struct monitor) * n);

    for (int i = 0; i < n; i++) {
        m_list[i].screen = m_info[i].screen_number;
        m_list[i].width = m_info[i].width;
        m_list[i].height = m_info[i].height;
        m_list[i].x = m_info[i].x_org;
        m_list[i].y = m_info[i].y_org;
        D fprintf(stderr, WINDOW_MANAGER_NAME": Screen #%d with dim: x=%d y=%d w=%d h=%d\n",
                m_list[i].screen, m_list[i].x, m_list[i].y, m_list[i].width, m_list[i].height);
    }

    ewmh_set_viewport();
}

static void
client_refresh(struct client *c)
{
    D fprintf(stderr, WINDOW_MANAGER_NAME": Refreshing client\n");
    for (int i = 0; i < 2; i++) {
        client_move_relative(c, 0, 0);
        client_resize_relative(c, 0, 0);
    }
}

static void
refresh_config(void)
{
    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        for (struct client *tmp = c_list[i]; tmp != NULL; tmp = tmp->next) {
            /* We run into this annoying issue when where we have to 
             * re-create these windows since the border_width has changed.
             * We end up destroying and recreating this windows, but this
             * causes them to be redrawn on the wrong screen, regardless of
             * their current desktop. The easiest way around this is to move
             * them all to the current desktop and then back agian */
            if (tmp->decorated) {
                client_decorations_destroy(tmp);
                client_decorations_create(tmp);
            }

            client_refresh(tmp);
            client_show(tmp);

            if (f_client != tmp) 
                client_set_color(tmp, conf.iu_color, conf.bu_color);
            else
                client_set_color(tmp, conf.if_color, conf.bf_color);

            if (i != curr_ws) {
                client_hide(tmp);
            } else {
                client_show(tmp);
                client_raise(tmp);
            }
        }
    }
}

static void
client_resize_absolute(struct client *c, int w, int h) 
{
    int dw = w;
    int dh = h;
    int dec_w = w;
    int dec_h = h;

    if (c->decorated) {
        dw = w - (2 * conf.i_width) - (2 * conf.b_width);
        dh = h - (2 * conf.i_width) - (2 * conf.b_width) - conf.t_height;

        dec_w = w - (2 * conf.b_width);
        dec_h = h - (2 * conf.b_width);
    }

    D fprintf(stderr, WINDOW_MANAGER_NAME": Resizing client main window\n");
    XResizeWindow(display, c->window, MAX(dw, MINIMUM_DIM), MAX(dh, MINIMUM_DIM));
    if (c->decorated) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Resizing client decoration\n");
        XResizeWindow(display, c->dec, MAX(dec_w, MINIMUM_DIM), MAX(dec_h, MINIMUM_DIM));
    }

    c->geom.width = MAX(w, MINIMUM_DIM);
    c->geom.height = MAX(h, MINIMUM_DIM);
    if (c->mono)
        c->mono = false;
    client_set_status(c);
}

static void
client_resize_relative(struct client *c, int w, int h) 
{
    if (conf.edge_lock) {
        int dw, dh, mon;
        mon = ws_m_list[c->ws];

        /* First, check if the resize will exceed the dimensions set by
         * the right side of the given monitor. If they do, cap the resize
         * amount to move only to the edge of the monitor.
         */
        if (c->geom.x + c->geom.width + w > m_list[mon].x + m_list[mon].width)
            dw = m_list[mon].x + m_list[mon].width - c->geom.x;
        else
            dw = c->geom.width + w;

        /* Next, check if the resize will exceed the dimensions set by
         * the bottom side of the given monitor. If they do, cap the resize
         * amount to move only to the edge of the monitor.
         */
        if (c->geom.y + c->geom.height + conf.t_height + h > m_list[mon].y + m_list[mon].height)
            dh = m_list[mon].height + m_list[mon].y - c->geom.y;
        else
            dh = c->geom.height + h;

        client_resize_absolute(c, dw, dh);
    } else {
        client_resize_absolute(c, c->geom.width + w, c->geom.height + h);
    }
}

static void
run(void)
{
    XEvent e;
    XSync(display, false);
    while (running) {
        XNextEvent(display, &e);
        D fprintf(stderr, WINDOW_MANAGER_NAME": Receieved new %d event\n", e.type);
        if (event_handler[e.type]) {
            D fprintf(stderr, WINDOW_MANAGER_NAME": Handling %d event\n", e.type);
            event_handler[e.type](&e);
        }
    }
}

static void
client_save(struct client *c, int ws)
{
    /* Save the client to the "stack" of managed clients */
    c->next = c_list[ws];
    c_list[ws] = c;

    /* Save the client o the list of focusing order */
    c->f_next = f_list[ws];
    f_list[ws] = c;
}

/* This method will return true if it is safe to show a client on the given workspace
 * based on the currently focused workspaces on each monitor.
 */
static bool
safe_to_focus(int ws)
{
    int mon = ws_m_list[ws];

    if (m_count == 1)
        return false;
    
    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        if (i != ws && ws_m_list[i] == mon && c_list[i] != NULL && c_list[i]->hidden == false)
            return false;

    return true;
}

static void
client_send_to_ws(struct client *c, int ws)
{
    int prev;
    client_delete(c);

    // DEBUG
    int count = 0;
    for (struct client *tmp = c_list[ws]; tmp != NULL; tmp = tmp->next) {
        count++;
    }
    D fprintf(stderr, WINDOW_MANAGER_NAME": Found %d clients on previous workspace\n", count);
    // END DEBUG

    prev = c->ws;
    c->ws = ws;
    client_save(c, ws);
    client_hide(c);
    focus_next(f_list[prev]);

    if (safe_to_focus(ws))
        client_show(c);

    ewmh_set_desktop(c, ws);
}

static void
client_set_color(struct client *c, unsigned long i_color, unsigned long b_color)
{
    if (c->decorated) {
        XSetWindowBackground(display, c->dec, i_color);
        XSetWindowBorder(display, c->dec, b_color);
        XClearWindow(display, c->dec);
        draw_text(c, c == f_client);
    }
}


static void
client_set_input(struct client *c)
{
    XSetInputFocus(display, c->window, RevertToPointerRoot, CurrentTime);
}

static void
client_set_title(struct client *c)
{
    XTextProperty tp;
    char **slist = NULL;
    int count;

    c->title[0] = 0;
    if (!XGetTextProperty(display, c->window, &tp, net_atom[NetWMName])) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Could not read client title, not updating\n");
        return;
    }

    if (tp.encoding == XA_STRING) {
        strncpy(c->title, (char *)tp.value, sizeof(c->title) - 1);
    } else {
        if (XmbTextPropertyToTextList(display, &tp, &slist, &count) >= Success && count > 0 && *slist) {
            strncpy(c->title, slist[0], sizeof(c->title) - 1);
            XFreeStringList(slist);
        }
    }

    c->title[sizeof c->title - 1] = 0;
    XFree(tp.value);
}


static void
setup(void)
{
    unsigned long data[1], data2[1];
    int mon;
    XSetWindowAttributes wa = { .override_redirect = true };
    // Setup our conf initially
    conf.b_width     = BORDER_WIDTH;
    conf.t_height    = TITLE_HEIGHT;
    conf.i_width     = INTERNAL_BORDER_WIDTH;
    conf.bf_color    = BORDER_FOCUS_COLOR;
    conf.bu_color    = BORDER_UNFOCUS_COLOR;
    conf.if_color    = INNER_FOCUS_COLOR;
    conf.iu_color    = INNER_UNFOCUS_COLOR; 
    conf.m_step      = MOVE_STEP;
    conf.r_step      = RESIZE_STEP;
    conf.focus_new   = FOCUS_NEW;
    conf.edge_lock   = EDGE_LOCK;
    conf.t_center    = TITLE_CENTER;
    conf.top_gap     = TOP_GAP;
    conf.smart_place = SMART_PLACE;
    conf.draw_text   = DRAW_TEXT;
    conf.json_status = JSON_STATUS;

    root = DefaultRootWindow(display);
    screen = DefaultScreen(display);
    display_height = DisplayHeight(display, screen); /* Display height/width still needed for hiding clients */ 
    display_width = DisplayWidth(display, screen);
    move_cursor = XCreateFontCursor(display, XC_crosshair); 
    normal_cursor = XCreateFontCursor(display, XC_left_ptr);
    XDefineCursor(display, root, normal_cursor);

    XSelectInput(display, root,
            SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|Button1Mask);
    xerrorxlib = XSetErrorHandler(xerror);

    check = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
    nofocus = XCreateSimpleWindow(display, root, -10, -10, 1, 1, 0, 0, 0);
    XChangeWindowAttributes(display, nofocus, CWOverrideRedirect, &wa);
    XMapWindow(display, nofocus);
    client_manage_focus(NULL);

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
    net_atom[NetWMWindowTypeDialog]  = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    net_atom[NetWMWindowTypeUtility] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    net_atom[NetWMDesktop]           = XInternAtom(display, "_NET_WM_DESKTOP", False);
    net_atom[NetWMFrameExtents]      = XInternAtom(display, "_NET_FRAME_EXTENTS", False);
    net_atom[NetDesktopNames]        = XInternAtom(display, "_NET_DESKTOP_NAMES", False);
    net_atom[NetDesktopViewport]     = XInternAtom(display, "_NET_DESKTOP_VIEWPORT", False);

    /* Some icccm atoms */
    wm_atom[WMDeleteWindow]          = XInternAtom(display, "WM_DELETE_WINDOW", False);
    wm_atom[WMTakeFocus]             = XInternAtom(display, "WM_TAKE_FOCUS", False);
    wm_atom[WMProtocols]             = XInternAtom(display, "WM_PROTOCOLS", False);

    /* Internal berry atoms */
    net_berry[BerryWindowStatus]     = XInternAtom(display, "BERRY_WINDOW_STATUS", False);
    net_berry[BerryClientEvent]      = XInternAtom(display, "BERRY_CLIENT_EVENT", False);
    net_berry[BerryFontProperty]     = XInternAtom(display, "BERRY_FONT_PROPERTY", False);

    D fprintf(stderr, WINDOW_MANAGER_NAME": Successfully assigned atoms\n");

    XChangeProperty(display , check , net_atom[NetWMCheck]   , XA_WINDOW  , 32 , PropModeReplace , (unsigned char *) &check              , 1);
    XChangeProperty(display , check , net_atom[NetWMName]    , utf8string , 8  , PropModeReplace , (unsigned char *) WINDOW_MANAGER_NAME , 5);
    XChangeProperty(display , root  , net_atom[NetWMCheck]   , XA_WINDOW  , 32 , PropModeReplace , (unsigned char *) &check              , 1);
    XChangeProperty(display , root  , net_atom[NetSupported] , XA_ATOM    , 32 , PropModeReplace , (unsigned char *) net_atom            , NetLast);

    D fprintf(stderr, WINDOW_MANAGER_NAME": Successfully set initial properties\n");

    /* Set the total number of desktops */
    data[0] = WORKSPACE_NUMBER;
    XChangeProperty(display, root, net_atom[NetNumberOfDesktops], XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 1);

    /* Set the intial "current desktop" to 0 */
    data2[0] = curr_ws;
    XChangeProperty(display, root, net_atom[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data2, 1);
    D fprintf(stderr, WINDOW_MANAGER_NAME": Setting up monitors\n");
    monitors_setup();
    D fprintf(stderr, WINDOW_MANAGER_NAME": Successfully setup monitors\n");
    mon = ws_m_list[curr_ws];
    XWarpPointer(display, None, root, 0, 0, 0, 0,
        m_list[mon].x + m_list[mon].width / 2,
        m_list[mon].y + m_list[mon].height / 2);
    
    gc = XCreateGC(display, root, 0, 0); 

    D fprintf(stderr, WINDOW_MANAGER_NAME": Allocating color values\n");
    XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen),
            TEXT_FOCUS_COLOR, &xft_focus_color);
    XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen),
            TEXT_UNFOCUS_COLOR, &xft_unfocus_color);

    font = XftFontOpenName(display, screen, global_font);
    ewmh_set_desktop_names();
}

static void
client_show(struct client *c)
{
    if (c->hidden) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": Showing client");
        client_move_absolute(c, c->x_hide, c->geom.y);
        client_raise(c);
        c->hidden = false;
    }
}

static void
client_snap_left(struct client *c)
{
    int mon;
    mon = ws_m_list[c->ws];
    client_move_absolute(c, m_list[mon].x, m_list[mon].y + conf.top_gap); 
    client_resize_absolute(c, m_list[mon].width / 2, m_list[mon].height - conf.top_gap);
}

static void
client_snap_right(struct client *c)
{
    int mon;
    mon = ws_m_list[c->ws];
    client_move_absolute(c, m_list[mon].x + m_list[mon].width / 2, m_list[mon].y + conf.top_gap); 
    client_resize_absolute(c, m_list[mon].width / 2, m_list[mon].height - conf.top_gap);
}

static void
switch_ws(int ws)
{
    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        /*if (i != ws && ws_m_list[i] == ws_m_list[ws]) {*/
        if (i != ws) {
            for (struct client *tmp = c_list[i]; tmp != NULL; tmp = tmp->next) {
                client_hide(tmp);
            }
        } else if (i == ws) {
            int count, j;
            count = 0;

            // how many active clients are on the current workspace
            for (struct client *tmp = c_list[i]; tmp != NULL; tmp = tmp->next) {
                count++;
                client_show(tmp);
            }

            Window wins[count*2];
            j = 0;

            for (struct client *tmp = c_list[i]; tmp != NULL; tmp = tmp->next) {
                wins[j] = tmp->window;
                wins[j+1] = tmp->dec;
                j += 2;
            }

            XRestackWindows(display, wins, count * 2);
        }
    }

    curr_ws = ws;
    int mon = ws_m_list[ws];
    D fprintf(stderr, WINDOW_MANAGER_NAME": Setting Screen #%d with active workspace %d\n", m_list[mon].screen, ws);
    client_manage_focus(c_list[curr_ws]);
    ewmh_set_active_desktop(ws);
}

static void
client_toggle_decorations(struct client *c)
{
    if (c->decorated)
        client_decorations_destroy(c);
    else
        client_decorations_create(c);

    client_refresh(c);
    client_raise(c);
    client_manage_focus(c);
    ewmh_set_frame_extents(c);
}

/*
 * Credit to tudurom and windowchef
 * as inspiration for this functionality
 */
static void
client_set_status(struct client *c)
{
    if (c == NULL)
        return;
    int size = 0;
    char *str = NULL;
    char *state, *decorated;

    D fprintf(stderr, WINDOW_MANAGER_NAME": Updating client status...\n");

    if (c->fullscreen)
        state = "fullscreen";
    else if (c->mono)
        state = "mono";
    else if (c->hidden)
        state = "hidden";
    else
        state = "normal";

    if (c->decorated)
        decorated = "true";
    else
        decorated = "false";


    if (conf.json_status) {
        size = asprintf(&str,
        "{"
            "\"window\":\"0x%08x\","
            "\"geom\":{"
                "\"x\":%d,"
                "\"y\":%d,"
                "\"width\":%d,"
                "\"height\":%d"
            "},"
            "\"state\":\"%s\","
            "\"decorated\":\"%s\""
        "}", (unsigned int)c->window, c->geom.x, c->geom.y, c->geom.width, c->geom.height, state, decorated);
    } else {
        size = asprintf(&str,
                "0x%08x, " // window id
                "%d, " // x
                "%d, " // y
                "%d, " // width 
                "%d, " // height
                "%s, " // state
                "%s",  // decorated
                (unsigned int)c->window, c->geom.x, c->geom.y, c->geom.width, c->geom.height, state, decorated);
    }

    if (size == -1) {
        D fprintf(stderr, WINDOW_MANAGER_NAME": asprintf returned -1, could not report window status\n");
        return;
    }

    XChangeProperty(display, c->window, net_berry[BerryWindowStatus], utf8string, 8, PropModeReplace,
            (unsigned char *) str, strlen(str));
    free(str);
}

static void
ewmh_set_fullscreen(struct client *c, bool fullscreen)
{
    XChangeProperty(display, c->window, net_atom[NetWMState], XA_ATOM, 32, 
            PropModeReplace, (unsigned char *)&net_atom[NetWMStateFullscreen], fullscreen ? 0 : 1 );
}

static void
ewmh_set_viewport(void)
{
    unsigned long data[2] = { 0, 0 };
    XChangeProperty(display, root, net_atom[NetDesktopViewport], XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&data, 2);
}

static void
ewmh_set_focus(struct client *c)
{
        XDeleteProperty(display, root, net_atom[NetActiveWindow]);
        f_client = c;
        /* Tell EWMH about our new window */
        XChangeProperty(display, root, net_atom[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &(c->window), 1);
}

static void
ewmh_set_desktop(struct client *c, int ws)
{
    unsigned long data[1];
    data[0] = ws;
    XChangeProperty(display, c->window, net_atom[NetWMDesktop], 
            XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 1);
}

static void ewmh_set_frame_extents(struct client *c)
{
    D fprintf(stderr, WINDOW_MANAGER_NAME": Setting client frame extents\n");
    unsigned long data[4];
    int left, right, top, bottom;

    if (c->decorated) {
        left = right = bottom = conf.b_width + conf.i_width;
        top = conf.b_width + conf.i_width + conf.t_height;
    } else {
        left = right = top = bottom = 0;
    }

    data[0] = left;
    data[1] = right;
    data[2] = top;
    data[3] = bottom;
    XChangeProperty(display, c->window, net_atom[NetWMFrameExtents],
            XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 4);
}

static void ewmh_set_client_list(void)
{
    /* Remove all current clients */
    XDeleteProperty(display, root, net_atom[NetClientList]);
    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        for (struct client *tmp = c_list[i]; tmp != NULL; tmp = tmp->next)
            XChangeProperty(display, root, net_atom[NetClientList], XA_WINDOW, 32, PropModeAppend,
                    (unsigned char *) &(tmp->window), 1);
}

/*
* Create and populate the values for _NET_DESKTOP_NAMES,
* used by applications such as polybar for named workspaces.
* By default, set the name of each workspaces to simply be the
* index of that workspace.
*/
static void ewmh_set_desktop_names(void)
{
    char **list;
    XTextProperty text_prop;
    list = malloc(sizeof(char*) * WORKSPACE_NUMBER);
    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        char *tmp = malloc(sizeof(char) * 2 + 1);
        sprintf(tmp, "%d", i);
        list[i] = tmp;
    }
    Xutf8TextListToTextProperty(display, list, WORKSPACE_NUMBER, XUTF8StringStyle, &text_prop);
    XSetTextProperty(display, root, &text_prop, XInternAtom(display, "_NET_DESKTOP_NAMES", False));
    XFree(text_prop.value);
    free(list);
}

static void
ewmh_set_active_desktop(int ws)
{
    unsigned long data[1];
    data[0] = ws;
    XChangeProperty(display, root, net_atom[NetCurrentDesktop], XA_CARDINAL, 32,
            PropModeReplace, (unsigned char *) data, 1);
}

static void
usage(void)
{
    D fprintf(stderr, "Usage: berry [-h|-v|-c CONFIG_PATH]\n");
    exit(EXIT_SUCCESS);
}

static void
version(void)
{
    D fprintf(stderr, "%s %s\n", WINDOW_MANAGER_NAME, __THIS_VERSION__);
    D fprintf(stderr, "Copyright (c) 2018 Joshua L Ervin\n");
    D fprintf(stderr, "Released under the MIT License\n");
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

    D fprintf(stderr, "Fatal request. request code=%d, error code=%d\n",
            e->request_code, e->error_code);
    return xerrorxlib(display, e);
}

int
main(int argc, char *argv[])
{
    int opt;
    char *conf_path = malloc(MAXLEN * sizeof(char));
    char *font_name = malloc(MAXLEN * sizeof(char));
    bool conf_found = true;
    conf_path[0] = '\0';
    font_name[0] = '\0';

    while ((opt = getopt(argc, argv, "dhf:vc:")) != -1) {
        switch (opt) {
            case 'h':
                usage();
                break;
            case 'f':
                snprintf(font_name, MAXLEN * sizeof(char), "%s", optarg);
                break;
            case 'c':
                snprintf(conf_path, MAXLEN * sizeof(char), "%s", optarg);
                break;
            case 'v':
                version();
                break;
            case 'd':
                debug = true;
                break;
        }
    }

    if (conf_path[0] == '\0') {
        char *xdg_home = getenv("XDG_CONFIG_HOME");
        if (xdg_home != NULL) {
            snprintf(conf_path, MAXLEN * sizeof(char), "%s/%s", xdg_home, BERRY_AUTOSTART);
        } else {
            char *home = getenv("HOME");
            if (home == NULL) {
                D fprintf(stderr, WINDOW_MANAGER_NAME": Warning $XDG_CONFIG_HOME and $HOME not found"
                        "autostart will not be loaded.\n");
                conf_found = false;
            }
            snprintf(conf_path, MAXLEN * sizeof(char), "%s/%s/%s", home, ".config", BERRY_AUTOSTART);
        }
    }
    
    if (font_name[0] == '\0') { // font not loaded
        D fprintf(stderr, WINDOW_MANAGER_NAME": font not specified, loading default font\n");
    } else {
        D fprintf(stderr, WINDOW_MANAGER_NAME": font specified, loading... %s\n", font_name);
        strcpy(global_font, font_name);
    }

    display = XOpenDisplay(NULL);
    if (!display)
        exit(EXIT_FAILURE);

    D fprintf(stderr, WINDOW_MANAGER_NAME": Successfully opened display\n");

    setup();
    if (conf_found)
        load_config(conf_path);
    run();
    close_wm();
    free(font_name);
    free(conf_path);
}
