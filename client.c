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
    { "window_move", IPCWindowMoveRelative, 2 } ,
    { "window_move_absolute", IPCWindowMoveAbsolute, 2 }, 
    { "window_resize", IPCWindowResizeRelative, 2 },
    { "window_resize_absolute", IPCWindowResizeAbsolute, 2 },
    { "window_raise", IPCWindowRaise, 2 },
    { "window_monocle", IPCWindowMonocle, 2 }
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
    ev.xclient.data.b[0] = c->cmd;
    /* Kill me for writing this I just want to see if it
     * works ok fam */
    if (c->cmd == 0 || c->cmd == 4) {
        printf("Found data\n");
        ev.xclient.data.b[1] = strtol(argv[0], NULL, 10);
        ev.xclient.data.b[2] = strtol(argv[1], NULL, 10);
    }

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
        printf("Made it this far!\n");
        /* for now keep this check very simple, but
         * in the future we will want to make sure all the
         * arguments line up 
         */
        if (strcmp(argv[1], c[i].name) == 0)
        {
            send_command(&c[i], c_argc, c_argv);
            printf("Command matches\n");
            return 0;
        }
    }
}



























