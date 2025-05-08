#pragma once

// Static data for inter-screen sharing

#include "network/WorldNetMode.h"
#include "network/NetworkConst.h"

#include "world/host/HostWorldData.h"

#include "filesystem/FileSystem.h"

enum class RegisteredScreens {
    MainMenu = 0,
    WorldGen = 1,
    Gameplay = 2,
    EditorOnly = 3,
    COUNT
};

enum class StartGameType {
    NewWorld,
    NewWorldFromTemplate,
    LoadWorld,
    EditorOnly,
    COUNT
};

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

    inline static ServerType serverType = ServerType::NONE;
    inline static nString hostIP;
    inline static StartGameType startGameType = StartGameType::NewWorld;
    inline static fs::path loadWorldPath;
};

class GameplayScreenGlobalState {
public:
    static void initDefaults();

    static bool isQuittingToMenu;
    static bool isQuittingToDesktop;
};

class WorldGenScreenGlobalState {
public:
};