#pragma once

// Static data for inter-screen sharing

#include "network/WorldType.h"

class MainMenuScreenState {
public:
    static void initDefaults();
    static void setJoin(const nString& hostIp);
    static void setHostLan();
    static void setHostOnline();

    static bool isSinglePlayer;
    static bool isHost;
    static bool isLan;
    static nString hostIP;
};