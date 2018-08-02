#ifndef _IPC_H_
#define _IPC_H

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
    IPCBorderWidth,
    IPCTitleHeight,
    IPCLast
};

#endif
