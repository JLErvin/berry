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
    IPCLast
};

#endif
