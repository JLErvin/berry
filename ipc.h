#ifndef _BERRY_IPC_H_
#define _BERRY_IPC_H_

#define BERRY_CLIENT_EVENT "BERRY_CLIENT_EVENT"
#define BERRY_FONT_PROPERTY "BERRY_FONT_PROPERTY"
#define BERRY_WINDOW_STATUS "BERRY_WINDOW_STATUS"

enum IPCCommand
{
    IPCWindowMoveRelative,
    IPCWindowMoveAbsolute,
    IPCWindowMonocle,
    IPCWindowRaise,
    IPCWindowResizeRelative,
    IPCWindowResizeAbsolute,
    IPCWindowToggleDecorations,
    IPCFocusColor,
    IPCUnfocusColor,
    IPCInnerFocusColor,
    IPCInnerUnfocusColor,
    IPCTitleFocusColor,
    IPCTitleUnfocusColor,
    IPCTitleCenter,
    IPCBorderWidth,
    IPCInnerBorderWidth,
    IPCTitleHeight,
    IPCSwitchWorkspace,
    IPCSendWorkspace,
    IPCFullscreen,
    IPCFullscreenState,
    IPCToggleAbove,
    IPCSnapLeft,
    IPCSnapRight,
    IPCSaveMonitor,
    IPCCardinalFocus,
    IPCCycleFocus,
    IPCWindowClose,
    IPCWindowCenter,
    IPCPointerFocus,
    IPCTopGap,
    IPCSmartPlace,
    IPCDrawText,
    IPCEdgeLock,
    IPCSetFont,
    IPCJSONStatus,
    IPCNameDesktop,
    IPCQuit,
    IPCManage,
    IPCUnmanage,
    IPCLast
};

enum WindowType
{
    Dock,
    Dialog,
    Toolbar,
    Menu,
    Splash,
    Utility,
    WindowLast
};

#endif
