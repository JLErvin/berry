#ifndef _BERRY_CONFIG_H_
#define _BERRY_CONFIG_H_

/* CHANGE THIS TO USE A DIFFERENT FONT */
#define DEFAULT_FONT "Monospace 10"

/* DO NOT CHANGE ANYTHING BELOW THIS COMMENT */
#define WORKSPACE_NUMBER 10

#define BORDER_WIDTH 3
#define INTERNAL_BORDER_WIDTH 3
#define TITLE_HEIGHT 30

#define MOVE_STEP 40
#define RESIZE_STEP 40
#define PLACE_RES 10 

#define TOP_GAP 30
#define BOT_GAP 0

#define BORDER_UNFOCUS_COLOR 0x20292d
#define BORDER_FOCUS_COLOR 0x20292d

#define INNER_UNFOCUS_COLOR 0x2c3539
#define INNER_FOCUS_COLOR 0xac8d6e

#define TEXT_FOCUS_COLOR "#ffffff"
#define TEXT_UNFOCUS_COLOR "#000000"

#define FOCUS_NEW true
#define FOCUS_MOTION true
#define EDGE_LOCK true
#define TITLE_CENTER true
#define SMART_PLACE true
#define DRAW_TEXT true
#define JSON_STATUS true
#define FULLSCREEN_REMOVE_DEC true
#define FULLSCREEN_MAX true

#define MANAGE_DOCK false
#define MANAGE_DIALOG true
#define MANAGE_TOOLBAR false
#define MANAGE_MENU true
#define MANAGE_SPLASH false
#define MANAGE_UTILITY true

#define DECORATE_NEW true
#define MOVE_MASK Mod4Mask
#define RESIZE_MASK Mod1Mask
#define POINTER_INTERVAL 0
#define FOCUS_FOLLOWS_POINTER false

#endif
