#ifndef __BERRY_GLOBALS_H_
#define __BERRY_GLOBALS_H_

#define BERRY_AUTOSTART "berry/autostart"

#ifndef __WINDOW_MANAGER_NAME__
#define __WINDOW_MANAGER_NAME__ "wm"
#endif

#define MOUSEMASK (PointerMotionMask|ButtonPressMask|ButtonReleaseMask)

// Taken from DWM. Many thanks. https://git.suckless.org/dwm
#define mod_clean(mask) (mask & ~(LockMask) & \
        (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

#ifndef __THIS_VERSION__
#define __THIS_VERSION__ ""
#endif

#define MAXLEN 256
#define MINIMUM_DIM 30
#define TITLE_X_OFFSET 5
#define DEFAULT_ALPHA 0xffff

#endif
