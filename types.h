#ifndef _TYPES_H
#define _TYPES_H

#define NORTH 2
#define SOUTH 4
#define EAST 1
#define WEST 3

typedef struct Client Client;
typedef struct Config Config;

struct Client 
{
    int x, y, w, h, ws;
    Window win;
    Window dec;
    Client *next;
};

struct Config
{
    int8_t border_width, title_height;
    uint32_t focus_color, unfocus_color;
    int32_t resize_step, move_step;
    bool focus_new;
};

#endif

