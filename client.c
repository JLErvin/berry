#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

void
send_command(void)
{
    Display *display;
    Window root;
    XEvent ev;

    display = XOpenDisplay(NULL);

    if (!display)
        return;

    memset(&ev, 0, sizeof ev);
    ev.xclient.type = ClientMessage;
    ev.xclient.window = root;

    ev.xclient.message_type = XInternAtom(display, "bye", False);
    ev.xclient.format = 32;
    ev.xclient.data.b[0] = XInternAtom(display, "WM_SHIFT_LEFT", False);
    ev.xclient.data.b[1] = CurrentTime;

    XSendEvent(display, root, false, SubstructureRedirectMask, &ev);
    XSync(display, false);
}

int
main(int argc, char **argv)
{
    send_command();
}
