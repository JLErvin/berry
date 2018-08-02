#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "ipc.h"

void
send_command(void)
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
    ev.xclient.format = 8;
    ev.xclient.data.b[0] = 1;
    ev.xclient.data.b[1] = CurrentTime;
    ev.xclient.data.b[2] = 20;

    XSendEvent(display, root, false, SubstructureRedirectMask, &ev);
    XSync(display, false);
}

int
main(int argc, char **argv)
{
    send_command();
}
