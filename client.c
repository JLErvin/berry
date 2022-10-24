/* Copyright (c) 2018 Joshua L Ervin. All rights reserved. */
/* Licensed under the MIT License. See the LICENSE file in the project root for full license information. */

#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "globals.h"
#include "ipc.h"
#include "utils.h"

static void fn_hex(long *, bool, int, char **);
static void fn_int(long *, bool, int, char **);
static void fn_bool(long *, bool, int, char **);
static void fn_font(long *, bool, int, char **);
static void fn_str(long *, bool, int, char **);
static void fn_int_str(long *, bool, int, char **);
static void usage(FILE *);
static void fn_mask(long *, bool, int, char **);
static void version(void);

static Display* display = NULL;
static Window root = 0;

struct command {
    const char* name;
    enum IPCCommand cmd;
    bool config;
    int argc;
    void (*handler)(long *, bool, int, char **);
};

static const struct command command_table[] = {
    { "window_move",            IPCWindowMoveRelative,      false, 2, fn_int     },
    { "window_move_absolute",   IPCWindowMoveAbsolute,      false, 2, fn_int     },
    { "window_resize",          IPCWindowResizeRelative,    false, 2, fn_int     },
    { "window_resize_absolute", IPCWindowResizeAbsolute,    false, 2, fn_int     },
    { "window_raise",           IPCWindowRaise,             false, 0, NULL       },
    { "window_monocle",         IPCWindowMonocle,           false, 0, NULL       },
    { "window_close",           IPCWindowClose,             false, 0, NULL       },
    { "window_center",          IPCWindowCenter,            false, 0, NULL       },
    { "focus_color",            IPCFocusColor,              true,  1, fn_hex     },
    { "unfocus_color",          IPCUnfocusColor,            true,  1, fn_hex     },
    { "inner_focus_color",      IPCInnerFocusColor,         true,  1, fn_hex     },
    { "inner_unfocus_color",    IPCInnerUnfocusColor,       true,  1, fn_hex     },
    { "text_focus_color",       IPCTitleFocusColor,         true,  1, fn_hex     },
    { "text_unfocus_color",     IPCTitleUnfocusColor,       true,  1, fn_hex     },
    { "border_width",           IPCBorderWidth,             true,  1, fn_int     },
    { "inner_border_width",     IPCInnerBorderWidth,        true,  1, fn_int     },
    { "title_height",           IPCTitleHeight,             true,  1, fn_int     },
    { "switch_workspace",       IPCSwitchWorkspace,         false, 1, fn_int     },
    { "send_to_workspace",      IPCSendWorkspace,           false, 1, fn_int     },
    { "fullscreen",             IPCFullscreen,              false, 0, NULL       },
    { "fullscreen_state",       IPCFullscreenState,         false, 0, NULL       },
    { "fullscreen_remove_dec",  IPCFullscreenRemoveDec,     true,  1, fn_bool    },
    { "fullscreen_max",         IPCFullscreenMax,           true,  1, fn_bool    },
    { "snap_left",              IPCSnapLeft,                false, 0, NULL       },
    { "snap_right",             IPCSnapRight,               false, 0, NULL       },
    { "cardinal_focus",         IPCCardinalFocus,           false, 1, fn_int     },
    { "toggle_decorations",     IPCWindowToggleDecorations, false, 0, NULL       },
    { "cycle_focus",            IPCCycleFocus,              false, 0, NULL       },
    { "pointer_focus",          IPCPointerFocus,            false, 0, NULL       },
    { "quit",                   IPCQuit,                    true,  0, NULL       },
    { "top_gap",                IPCTopGap,                  true,  1, fn_int     },
    { "edge_gap",               IPCEdgeGap,                 false, 4, fn_int     },
    { "save_monitor",           IPCSaveMonitor,             false, 2, fn_int     },
    { "smart_place",            IPCSmartPlace,              true,  1, fn_bool    },
    { "draw_text",              IPCDrawText,                true,  1, fn_bool    },
    { "edge_lock",              IPCEdgeLock,                true,  1, fn_bool    },
    { "set_font",               IPCSetFont,                 false, 1, fn_font    },
    { "json_status",            IPCJSONStatus,              true,  1, fn_bool    },
    { "manage",                 IPCManage,                  true,  1, fn_str     },
    { "unmanage",               IPCUnmanage,                true,  1, fn_str     },
    { "decorate_new",           IPCDecorate,                true,  1, fn_bool    },
    { "name_desktop",           IPCNameDesktop,             false, 2, fn_int_str },
    { "move_button",            IPCMoveButton,              true,  1, fn_int     },
    { "move_mask",              IPCMoveMask,                true,  1, fn_mask    },
    { "resize_button",          IPCResizeButton,            true,  1, fn_int     },
    { "resize_mask",            IPCResizeMask,              true,  1, fn_mask    },
    { "pointer_interval",       IPCPointerInterval,         true,  1, fn_int     },
    { "focus_follows_pointer",  IPCFocusFollowsPointer,     true,  1, fn_bool    },
    { "warp_pointer",           IPCWarpPointer,             true,  1, fn_bool    },
};

static void
fn_hex(long *data, bool b, int i, char **argv)
{
    UNUSED(b);
    data[i+b] = strtoul(argv[i - 1], NULL, 16);
}

static void
fn_int(long *data, bool b, int i, char **argv)
{
    UNUSED(b);
    data[i+b] = strtol(argv[i - 1], NULL, 10);
}

static void
fn_bool(long *data, bool b, int i, char **argv)
{
    UNUSED(b);
    data[i+b] = strcmp(argv[i-1], "true") == 0 ? 1 : 0;
}

static void
fn_str(long *data, bool b, int i, char **argv)
{
    UNUSED(b);
    // lord forgive me for I have sinned
    if (strcmp(argv[i-1], "Dialog") == 0) data[i+b] = Dialog;
    else if (strcmp(argv[i-1], "Toolbar") == 0) data[i+b] = Toolbar;
    else if (strcmp(argv[i-1], "Menu") == 0) data[i+b] = Menu;
    else if (strcmp(argv[i-1], "Splash") == 0) data[i+b] = Splash;
    else if (strcmp(argv[i-1], "Utility") == 0) data[i+b] = Utility;
}

/* This function works by setting a new atom globally on the root
 * window called BERRY_FONT_PROPERTY which tells berry what font
 * to use for window decoration.
 * We set the font here in the client and then send a message to
 * berry, notifying the main program to read this value
 */
static void
fn_font(long *data, bool b, int i, char** argv)
{
    UNUSED(b);
    UNUSED(i);
    UNUSED(data);
    char** font_list;
    XTextProperty font_prop;

    font_list = malloc(sizeof(char*));
    font_list[0] = argv[0];
    Xutf8TextListToTextProperty(display, font_list, 1,
                                XUTF8StringStyle, &font_prop);
    XSetTextProperty(display, root, &font_prop, XInternAtom(display, BERRY_FONT_PROPERTY, False));
    XFree(font_prop.value);
    free(font_list);
}

/*
 * This function sets the _NET_DESKTOP_NAMES property
 * by assocating the first argument, an integer,
 * and the following string to the _NET_DESKTOP_NAMES property
 */
static void
fn_int_str(long *data, bool b, int i, char **argv)
{
    UNUSED(b);
    UNUSED(data);
    UNUSED(i);
    char *name;
    char **list;
    int idx, len;
    XTextProperty text_prop;

    name = argv[1];
    idx = atoi(argv[0]);
    XGetTextProperty(display, root, &text_prop, XInternAtom(display, "_NET_DESKTOP_NAMES", False));
    Xutf8TextPropertyToTextList(display, &text_prop, &list, &len);
    list[idx] = name;
    Xutf8TextListToTextProperty(display, list, WORKSPACE_NUMBER, XUTF8StringStyle, &text_prop);
    XSetTextProperty(display, root, &text_prop, XInternAtom(display, "_NET_DESKTOP_NAMES", False));

    XFree(text_prop.value);
    free(list);
}

static void
fn_mask(long *data, bool b, int i, char **argv)
{
    UNUSED(b);
    data[i+b] = 0;
    char * mask_str = strtok( argv[i-1] , "|");

    while( mask_str != NULL ) {
        if( ! strcmp(mask_str,"shift") ) data[i+b] = data[i+b]|ShiftMask;
        else if( !strcmp(mask_str,"lock") ) data[i+b] = data[i+b]|LockMask;
        else if( !strcmp(mask_str,"ctrl") ) data[i+b] = data[i+b]|ControlMask;
        else if( !strcmp(mask_str,"mod1") ) data[i+b] = data[i+b]|Mod1Mask;
        else if( !strcmp(mask_str,"mod2") ) data[i+b] = data[i+b]|Mod2Mask;
        else if( !strcmp(mask_str,"mod3") ) data[i+b] = data[i+b]|Mod3Mask;
        else if( !strcmp(mask_str,"mod4") ) data[i+b] = data[i+b]|Mod4Mask;
        else if( !strcmp(mask_str,"mod5") ) data[i+b] = data[i+b]|Mod5Mask;
        else {
            printf("%s is not a valid modifier", mask_str);
            data[i+b]=0;
            break;
        }
        mask_str = strtok(NULL, "|");
    }
}

static void
usage(FILE *out)
{
    int rc = out == stderr ? EXIT_FAILURE : EXIT_SUCCESS;
    fputs("Usage: berryc [-h|-v] <command> [args...]\n", out);
    exit(rc);
}

static void
version(void)
{
    printf("berryc %s\n", __THIS_VERSION__);
    exit(EXIT_SUCCESS);
}

static void
send_command(const struct command *c, int argc, char **argv)
{
    XEvent ev;
    UNUSED(argc);

    display = XOpenDisplay(NULL);

    if (!display)
        return;

    root = DefaultRootWindow(display);

    memset(&ev, 0, sizeof ev);
    ev.xclient.type = ClientMessage;
    ev.xclient.window = root;
    ev.xclient.message_type = XInternAtom(display, BERRY_CLIENT_EVENT, False);
    ev.xclient.format = 32;

    /* We use the following protocol:
     * If the given command is related to berry's config then assign it a value of
     * IPCConfig at d[0]. Then, assign the specific config element at d[1], shifting
     * all values up by one.
     * Otherwise, set the IPC command at d[0] and assign arguments from 1 upwards.
     */
    if (c->config) {
        ev.xclient.data.l[0] = IPCConfig;
        ev.xclient.data.l[1] = c->cmd;
    } else {
        ev.xclient.data.l[0] = c->cmd;
    }

    if (strcmp(c->name, "name_desktop") == 0) {
        fn_int_str(ev.xclient.data.l, false, 2, argv);
        XSync(display, false);
        return;
    }

    for (int i = 1; i <= argc; i++) {
        (c->handler)(ev.xclient.data.l, c->config, i, argv);
    }

    XSendEvent(display, root, false, SubstructureRedirectMask, &ev);
    XSync(display, false);
    XCloseDisplay(display);
}

int
main(int argc, char **argv)
{
    int c, c_argc;
    char **c_argv;

    c_argc = argc - 2;
    c_argv = argv + 2;

    while ((c = getopt(argc, argv, "hv")) != -1) {
        switch (c) {
        case 'h':
            usage(stdout);
            break;
        case 'v':
            version();
            break;
        default:
            usage(stderr);
            break;
        }
    }

    for (int i = 0; i < (int)(sizeof command_table / sizeof command_table[0]); i++) {
        if (strcmp(argv[1], command_table[i].name) == 0) {
            if (command_table[i].argc != c_argc) {
                printf("Wrong number of arguments\n");
                printf("%d expected for command %s\n", command_table[i].argc, command_table[i].name);
                return EXIT_FAILURE;
            }
            send_command(&command_table[i], c_argc, c_argv);
            return EXIT_SUCCESS;
        }
    }

    fprintf(stderr, "Command not found %s, exiting\n", argv[1]);
    return EXIT_FAILURE;
}
