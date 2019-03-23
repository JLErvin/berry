#ifndef _BERRY_IPC_H_
#define _BERRY_IPC_H_

#define BERRY_CLIENT_EVENT "BERRY_CLIENT_EVENT"

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
    IPCBorderWidth,
    IPCInnerBorderWidth,
    IPCTitleHeight,
    IPCSwitchWorkspace,
    IPCSendWorkspace,
    IPCFullscreen,
    IPCSnapLeft,
    IPCSnapRight,
    IPCSaveMonitor,
    IPCCardinalFocus,
    IPCCycleFocus,
    IPCWindowClose,
    IPCWindowCenter,
    IPCPointerMove,
    IPCTopGap,
    IPCLast
};

#endif
