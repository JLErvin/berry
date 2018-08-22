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

#include "ipc.h"

static void fn_hex(long *, int, char **);

struct Command
{
    char *name;
    enum IPCCommand cmd;
    int argc;
    void (*handler)(long *, int, char **);
};

static struct Command c[] = {
    { "window_move",            IPCWindowMoveRelative,      2, NULL   },
    { "window_move_absolute",   IPCWindowMoveAbsolute,      2, NULL   }, 
    { "window_resize",          IPCWindowResizeRelative,    2, NULL   },
    { "window_resize_absolute", IPCWindowResizeAbsolute,    2, NULL   },
    { "window_raise",           IPCWindowRaise,             0, NULL   },
    { "window_monocle",         IPCWindowMonocle,           0, NULL   },
    { "window_close",           IPCWindowClose,             0, NULL   },
    { "focus_color",            IPCFocusColor,              1, fn_hex },
    { "unfocus_color",          IPCUnfocusColor,            1, fn_hex }, 
    { "inner_focus_color",      IPCInnerFocusColor,         1, fn_hex },
    { "inner_unfocus_color",    IPCInnerUnfocusColor,       1, fn_hex }, 
    { "border_width",           IPCBorderWidth,             1, NULL   },
    { "inner_border_width",     IPCInnerBorderWidth,        1, NULL   },
    { "title_height",           IPCTitleHeight,             1, NULL   },
    { "switch_workspace",       IPCSwitchWorkspace,         1, NULL   },
    { "send_to_workspace",      IPCSendWorkspace,           1, NULL   },
    { "fullscreen",             IPCFullscreen,              0, NULL   },
    { "snap_left",              IPCSnapLeft,                0, NULL   },
    { "snap_right",             IPCSnapRight,               0, NULL   },
    { "cardinal_focus",         IPCCardinalFocus,           1, NULL   },
    { "toggle_decorations",     IPCWindowToggleDecorations, 0, NULL   },
    { "cycle_focus",            IPCCycleFocus,              0, NULL   },
};

static void
fn_hex(long *data, int argc, char **argv)
{
    for (int i = 0; i < argc; i++) {
        data[i] = strtoul(argv[i], NULL, 16);
    }
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
    {
        if (c->cmd == 7)
            ev.xclient.data.l[1] = strtoul(argv[0], NULL, 16);
        else
            ev.xclient.data.l[1] = strtol(argv[0], NULL, 10);
    }
    if (c->argc == 2)
        ev.xclient.data.l[2] = strtol(argv[1], NULL, 10);

    XSendEvent(display, root, false, SubstructureRedirectMask, &ev);
    XSync(display, false);
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

    for (int i = 0; i < sizeof c / sizeof c[0]; i++)
    {
        if (strcmp(argv[1], c[i].name) == 0)
        {
            if (c[i].argc != c_argc)
            {
                if (c->argc < c_argc)
                    printf("Too many arguments\n");
                else
                    printf("Too few arguments\n");

                printf("%d expected for command %s\n", c[i].argc, c[i].name);
                return 1;
            }

            send_command(&c[i], c_argc, c_argv);
            return 0;
        }
    }

    printf("Command not found %s, exiting\n", argv[1]);
}
