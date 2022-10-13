#pragma once

// Static data for inter-screen sharing

#include "network/WorldType.h"
#include "network/NetworkConst.h"

class MainMenuScreenGlobalState {
public:
    static void initDefaults();
    static void setJoin(const nString& targetHostIp);
    static void setHostLan();
    static void setHostOnline();
    static void setHostDev();

    static bool isSinglePlayer() { return hostIP.empty() && serverType == ServerType::NONE; }
    static bool isHosting() { return hostIP.empty() && serverType != ServerType::NONE; }
    static bool isClient() { return !hostIP.empty(); }

    static ServerType serverType;
    static nString hostIP;
};

class GameplayScreenGlobalState {
public:
    static void initDefaults();

    static bool isQuittingToMenu;
    static bool isQuittingToDesktop;
};