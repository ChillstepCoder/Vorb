#include "stdafx.h"



#include "network/srv/GameServer.h"

// TODO: Set this with a command arg?
ServerType SERVER_TYPE = ServerType::ONLINE;

bool isDisplayConnected() {
    static_assert(_WIN32 == 1);
    DISPLAY_DEVICE device;
    device.cb = sizeof(DISPLAY_DEVICE);
    return EnumDisplayDevices(nullptr, 0, &device, 0);
}

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {

    // Allocate a console if we are not headless
    if (isDisplayConnected()) {
        if (AllocConsole()) {
            // TODO: Maybe this is bad https://stackoverflow.com/questions/15543571/allocconsole-not-displaying-cout
            freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
            freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
        }
    }

    InitializeYojimbo();
    
    logSrv(L"Starting server...\n");
    GameServer gameServer(SERVER_TYPE);
    gameServer.start();

    logSrv(L"Shutting down\n");
    return 0;
}
