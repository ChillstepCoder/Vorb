#pragma once

// Static data for inter-screen sharing

#include "network/WorldType.h"
#include "network/NetworkConst.h"

class MainMenuScreenState {
public:
    static void initDefaults();
    static void setJoin(const nString& hostIp);
    static void setHostLan();
    static void setHostOnline();
    static void setHostDev();

    static ServerType serverType;
    static nString hostIP;
};

class GameplayScreenState {
public:
    static void initDefaults();

    static bool isQuittingToMenu;
    static bool isQuittingToDesktop;
};