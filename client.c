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
    const char* usage;
    const char* description;
};

static const struct command command_table[] = {
    { "window_move",            IPCWindowMoveRelative,      false, 2, fn_int,     "x y",                                      "move the focused window by x and y pixels, relatively"},
    { "window_move_absolute",   IPCWindowMoveAbsolute,      false, 2, fn_int,     "x y",                                      "move the focused window to position x and y"},
    { "window_resize",          IPCWindowResizeRelative,    false, 2, fn_int,     "x y",                                      "resize the focused window by x and y pixels, relatively"},
    { "window_resize_absolute", IPCWindowResizeAbsolute,    false, 2, fn_int,     "x y",                                      "resize the focused window by x and y"},
    { "window_raise",           IPCWindowRaise,             false, 0, NULL,       "",                                         "raise the focused window"},
    { "window_monocle",         IPCWindowMonocle,           false, 0, NULL,       "",                                         "set the focused window the fill the screen, respecting top_gap, maintains decorations"},
    { "window_close",           IPCWindowClose,             false, 0, NULL,       "",                                         "close the focused window and its associated application"},
    { "window_center",          IPCWindowCenter,            false, 0, NULL,       "",                                         "center the focused window, maintains current size"},
    { "focus_color",            IPCFocusColor,              true,  1, fn_hex,     "#XXXXXX",                                  "Set the color of the outer border of the focused window"},
    { "unfocus_color",          IPCUnfocusColor,            true,  1, fn_hex,     "#XXXXXX",                                  "Set the color of the outer border for all unfocused windows"},
    { "inner_focus_color",      IPCInnerFocusColor,         true,  1, fn_hex,     "#XXXXXX",                                  "Set the color of the inner border and the titlebar of the focused window"},
    { "inner_unfocus_color",    IPCInnerUnfocusColor,       true,  1, fn_hex,     "#XXXXXX",                                  "Set the color of the inner border and the titlebar of the unfocused window"},
    { "text_focus_color",       IPCTitleFocusColor,         true,  1, fn_hex,     "#XXXXXX",                                  "Set the color of the title bar text for the focused window"},
    { "text_unfocus_color",     IPCTitleUnfocusColor,       true,  1, fn_hex,     "#XXXXXX",                                  "Set the color of the title bar text for all unfocused windows"},
    { "border_width",           IPCBorderWidth,             true,  1, fn_int,     "WIDTH",                                    "Set the border width, in pixels, of the outer border"},
    { "inner_border_width",     IPCInnerBorderWidth,        true,  1, fn_int,     "WIDTH",                                    "Set the border width, in pixels, of the inneer border"},
    { "title_height",           IPCTitleHeight,             true,  1, fn_int,     "HEIGHT",                                   "Set the height of the title bar, does not include border widths"},
    { "switch_workspace",       IPCSwitchWorkspace,         false, 1, fn_int,     "i",                                        "switch the active desktop"},
    { "send_to_workspace",      IPCSendWorkspace,           false, 1, fn_int,     "i",                                        "send the focused window to the given workspace"},
    { "fullscreen",             IPCFullscreen,              false, 0, NULL,       "",                                         "toggle fullscreen status of the focused window, fills the active screen"},
    { "fullscreen_state",       IPCFullscreenState,         false, 0, NULL,       "",                                         "toggle fullscreen status of the focused window, doesn’t resize the window"},
    { "fullscreen_remove_dec",  IPCFullscreenRemoveDec,     true,  1, fn_bool,    "true/false",                               ""},
    { "fullscreen_max",         IPCFullscreenMax,           true,  1, fn_bool,    "true/false",                               ""},
    { "snap_left",              IPCSnapLeft,                false, 0, NULL,       "",                                         "snap the focused window to fill the left half of the screen"},
    { "snap_right",             IPCSnapRight,               false, 0, NULL,       "",                                         "snap the focused window to fill the right half of the screen"},
    { "cardinal_focus",         IPCCardinalFocus,           false, 1, fn_int,     "1/2/3/4",                                  "shift focus to the nearest client in the specified direction"},
    { "toggle_decorations",     IPCWindowToggleDecorations, false, 0, NULL,       "",                                         "toggle decorations for the focused client"},
    { "cycle_focus",            IPCCycleFocus,              false, 0, NULL,       "",                                         "change focus to the next client in the stack"},
    { "pointer_focus",          IPCPointerFocus,            false, 0, NULL,       "",                                         "focus the window under the current pointer (used by sxhkd)"},
    { "quit",                   IPCQuit,                    true,  0, NULL,       "",                                         "close the window manager"},
    { "top_gap",                IPCTopGap,                  true,  1, fn_int,     "GAP",                                      "Set the offset at the top of the monitor (usually for system bars)"},
    { "edge_gap",               IPCEdgeGap,                 false, 4, fn_int,     "TOP BOTTOM LEFT RIGHT",                    "Set the edge gap around the monitor (must include all parameters)"},
    { "save_monitor",           IPCSaveMonitor,             false, 2, fn_int,     "i j",                                      "Associate the ith monitor to the jth workspace"},
    { "smart_place",            IPCSmartPlace,              true,  1, fn_bool,    "true/false",                               "Determine whether or not newly placed windows should be placed in the largest available space."},
    { "draw_text",              IPCDrawText,                true,  1, fn_bool,    "true/false",                               "Determine whether or not text should be draw in title bars"},
    { "edge_lock",              IPCEdgeLock,                true,  1, fn_bool,    "true/false",                               ""},
    { "set_font",               IPCSetFont,                 false, 1, fn_font,    "FONT_NAME",                                "Set the name of the font to use (e.g. set_font 'xft:Rubik:size=10:style=Bold')"},
    { "json_status",            IPCJSONStatus,              true,  1, fn_bool,    "true/false",                               "Determine whether or not BERRY_WINDOW_STATUS returns JSON formatted text."},
    { "manage",                 IPCManage,                  true,  1, fn_str,     "Dialog|Toolbar|Menu|Splash|Utility",       "Set berry to manage clients of the above type. Clients which are managed will be given decorations and are movable by the window manager. This is not retroactive for current clients. Only Toolbars and Splahes are not handled by default."},
    { "unmanage",               IPCUnmanage,                true,  1, fn_str,     "Dialog|Toolbar|Menu|Splash|Utility",       "Set berry to not manage clients of the above type."},
    { "decorate_new",           IPCDecorate,                true,  1, fn_bool,    "true/false",                               "Determine whether or not new windows are decorated by default"},
    { "name_desktop",           IPCNameDesktop,             false, 2, fn_int_str, "DESKTOP NAME",                             "Name the ith desktop name. Used with _NET_DESKTOP_NAMES."},
    { "move_mask",              IPCMoveMask,                true,  1, fn_mask,    "shift|lock|ctrl|mod1|mod2|mod3|mod4|mod5", ""},
    { "resize_mask",            IPCResizeMask,              true,  1, fn_mask,    "shift|lock|ctrl|mod1|mod2|mod3|mod4|mod5", ""},
    { "pointer_interval",       IPCPointerInterval,         true,  1, fn_int,     "INTERVAL",                                 "The minimum interval between two motion events generated by the pointer. Should help resolve issues related to resize lag on high refresh rate monitors. Default value of 0."},
    { "focus_follows_pointer",  IPCFocusFollowsPointer,     true,  1, fn_bool,    "true/false",                               ""},
    { "warp_pointer",           IPCWarpPointer,             true,  1, fn_bool,    "true/false",                               ""},
};

static void
fn_hex(long *data, bool b, int i, char **argv)
{
    UNUSED(b);
    if (*argv[i - 1] == '#') ++argv[i - 1];
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
    fputs("Available commands: \n", out);
    size_t command_count = sizeof(command_table) / sizeof(struct command);
    for (size_t i = 0; i < command_count; ++i) {
        struct command command = command_table[i];
        fprintf(out, "%s %s\n    %s\n\n", command.name, command.usage, command.description);
    }
    fputs("Please note that for the previous commands, #XXXXXX represents a hex color, accept hex color without leading #\n", out);
    fputs("All of these commands can be viewed on your system via `man berryc`\n", out);
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

    if (argc == 1) {
        usage(stderr);
    }

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
