#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
char * nowShowing() 
{
 return strdup("This is a sample text This is rendered with new Driver installed This is a sample text his is rendered with new Driver installed"); 
}

int main()
{
    XftFont *font;
    XftDraw *draw;
    XRenderColor render_color;
    XftColor xft_color;
    char *str;
    str = nowShowing();
    int x = 70;
    int y = 150;
    Display *dis = XOpenDisplay (0);
    int screen = DefaultScreen (dis);
    Window  w = XCreateSimpleWindow (dis, RootWindow (dis, screen),
                                     0, 0, 1200, 300, 1,
                                     BlackPixel (dis, screen),
                                     WhitePixel (dis, screen));
    XEvent ev;

    render_color.red = 0;
    render_color.green =0;
    render_color.blue = 0;
    render_color.alpha = 0xffff;

    XftColorAllocValue (dis,
                        DefaultVisual(dis, screen),
                        DefaultColormap(dis, screen),
                        &render_color,
                        &xft_color);

    //font = XftFontOpen(dis, screen,
      //                 XFT_FAMILY, XftTypeString, "charter",
      //                 XFT_SIZE, XftTypeDouble, 20.0,
       //                 NULL);
    font = XftFontOpenName(dis,screen,"URW Palladio L:style=Bold Italic"); //it takes a Fontconfig pattern string

    draw = XftDrawCreate(dis, w,
                         DefaultVisual(dis, screen),
                         DefaultColormap(dis, screen));

    XSelectInput (dis, w, ExposureMask);
    XMapWindow (dis, w);
    for (;;)
    {
        XNextEvent (dis, &ev);
        if (ev.type == Expose)
            XftDrawString8(draw, &xft_color, font, x, y, (XftChar8 *) str,
                           strlen(str));
    }
    return 0;
}
