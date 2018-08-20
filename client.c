#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "ipc.h"

struct Command
{
    char *name;
    enum IPCCommand cmd;
    int argc;
    int (*handler)(char *argv, char payload);
};

static struct Command c[] = {
    { "window_move",            IPCWindowMoveRelative,    2 } ,
    { "window_move_absolute",   IPCWindowMoveAbsolute,    2 }, 
    { "window_resize",          IPCWindowResizeRelative,  2 },
    { "window_resize_absolute", IPCWindowResizeAbsolute,  2 },
    { "window_raise",           IPCWindowRaise,           0 },
    { "window_monocle",         IPCWindowMonocle,         0 }
};

static void
send_command(struct Command *c, int argc, char **argv)
{
    Display *display;
    Window root;
    XEvent ev;
    printf("IPC Command = %d\n", c->cmd);

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
    if (c->argc == 1)
        ev.xclient.data.l[1] = strtol(argv[0], NULL, 10);
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

    /* s/o Vain for this loop :) */
    for (int i = 0; i < sizeof c / sizeof c[0]; i++)
    {
        /* for now keep this check very simple, but
         * in the future we will want to make sure all the
         * arguments line up 
         */
        if (strcmp(argv[1], c[i].name) == 0)
        {
            if (c->argc > c_argc)
            {
                printf("Error, too many arguments\n");
                return 1;
            }
            else if (c->argc < c_argc)
            {
                printf("Error, too few arguments\n");
                return 1;
            }
            send_command(&c[i], c_argc, c_argv);
            printf("Command %s found\n", argv[1]);
            return 0;
        }
    }
    printf("Command not found %s, exiting\n", argv[1]);
}



























