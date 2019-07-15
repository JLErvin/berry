/* Copyright (c) 2018 Joshua L Ervin. All rights reserved. */
/* Licensed under the MIT License. See the LICENSE file in the project root for full license information. */

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "globals.h"
#include "ipc.h"

static void fn_hex(long *, int, char **);
static void fn_int(long *, int, char **);
static void fn_str(long *, int, char **);
static void usage(void);
static void version(void);

struct Command
{
    char *name;
    enum IPCCommand cmd;
    int argc;
    void (*handler)(long *, int, char **);
};

static struct Command c[] = {
    { "window_move",            IPCWindowMoveRelative,      2, fn_int },
    { "window_move_absolute",   IPCWindowMoveAbsolute,      2, fn_int }, 
    { "window_resize",          IPCWindowResizeRelative,    2, fn_int },
    { "window_resize_absolute", IPCWindowResizeAbsolute,    2, fn_int },
    { "window_raise",           IPCWindowRaise,             0, NULL   },
    { "window_monocle",         IPCWindowMonocle,           0, NULL   },
    { "window_close",           IPCWindowClose,             0, NULL   },
    { "window_center",          IPCWindowCenter,            0, NULL   },
    { "focus_color",            IPCFocusColor,              1, fn_hex },
    { "unfocus_color",          IPCUnfocusColor,            1, fn_hex }, 
    { "inner_focus_color",      IPCInnerFocusColor,         1, fn_hex },
    { "inner_unfocus_color",    IPCInnerUnfocusColor,       1, fn_hex }, 
    { "text_focus_color",       IPCTitleFocusColor,         1, fn_hex }, 
    { "text_unfocus_color",     IPCTitleUnfocusColor,       1, fn_hex }, 
    { "border_width",           IPCBorderWidth,             1, fn_int },
    { "inner_border_width",     IPCInnerBorderWidth,        1, fn_int },
    { "title_height",           IPCTitleHeight,             1, fn_int },
    { "switch_workspace",       IPCSwitchWorkspace,         1, fn_int },
    { "send_to_workspace",      IPCSendWorkspace,           1, fn_int },
    { "fullscreen",             IPCFullscreen,              0, NULL   },
    { "fullscreen_state",       IPCFullscreenState,         0, NULL   },
    { "snap_left",              IPCSnapLeft,                0, NULL   },
    { "snap_right",             IPCSnapRight,               0, NULL   },
    { "cardinal_focus",         IPCCardinalFocus,           1, fn_int },
    { "toggle_decorations",     IPCWindowToggleDecorations, 0, NULL   },
    { "cycle_focus",            IPCCycleFocus,              0, NULL   },
    { "pointer_focus",          IPCPointerFocus,            0, NULL   },
    { "top_gap",                IPCTopGap,                  1, fn_int },
    { "save_monitor",           IPCSaveMonitor,             2, fn_int },
    { "smart_place",            IPCSmartPlace,              1, fn_int },
    { "draw_text",              IPCDrawText,                1, fn_str },
};

static void
fn_hex(long *data, int i, char **argv)
{
    data[i] = strtoul(argv[i - 1], NULL, 16);
}

static void
fn_int(long *data, int i, char **argv)
{
    data[i] = strtol(argv[i - 1], NULL, 10);
}

static void
fn_str(long *data, int i, char **argv)
{
    data[i] = strcmp(argv[i-1], "true") == 0 ? 1 : 0;
}

static void
usage(void)
{
    printf("Usage: berryc [-h|-v] <command> [args...]\n");
    exit(EXIT_SUCCESS);
}

static void
version(void)
{
    printf("berryc %s\n", __THIS_VERSION__);
    exit(EXIT_SUCCESS);
}

static void
send_command(struct Command *c, int argc, char **argv)
{
    Display *display;
    Window root;
    XEvent ev;

    display = XOpenDisplay(NULL);

    if (!display)
        return;

    root = DefaultRootWindow(display);

    memset(&ev, 0, sizeof ev);
    ev.xclient.type = ClientMessage;
    ev.xclient.window = root;
    ev.xclient.message_type = XInternAtom(display, BERRY_CLIENT_EVENT, False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = c->cmd;
    if (c->argc >= 1)
        (c->handler)(ev.xclient.data.l, 1, argv);
    if (c->argc == 2)
        (c->handler)(ev.xclient.data.l, 2, argv);

    XSendEvent(display, root, false, SubstructureRedirectMask, &ev);
    XSync(display, false);
    XCloseDisplay(display);
}

int
main(int argc, char **argv)
{
    int c_argc;
    char **c_argv;

    c_argc = argc - 2;
    c_argv = argv + 2;

    if (c_argc == -1) 
        return 1;
    else if (strcmp(argv[1], "-h") == 0)
        usage();
    else if (strcmp(argv[1], "-v") == 0)
        version();

    for (int i = 0; i < sizeof c / sizeof c[0]; i++) {
        if (strcmp(argv[1], c[i].name) == 0) {
            if (c[i].argc != c_argc) {
                printf("Wrong number of arguments\n");
                printf("%d expected for command %s\n", c[i].argc, c[i].name);
                return 1;
            }
            send_command(&c[i], c_argc, c_argv);
            return 0;
        }
    }

    printf("Command not found %s, exiting\n", argv[1]);
}
