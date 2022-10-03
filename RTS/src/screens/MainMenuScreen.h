#pragma once

#include <Vorb/ui/IGameScreen.h>

class App;

enum class MainMenuState {
    MAIN,
    MULTIPLAYER,
    JOIN,
    LAN_JOIN,
    ONLINE_JOIN,
    HOST,
    OPTIONS,
    COUNT
};

class MainMenuScreen : public vui::IAppScreen<App>
{
public:
    MainMenuScreen(App* const app);
    ~MainMenuScreen();

    virtual i32 getNextScreen() const override;
    virtual i32 getPreviousScreen() const override;

    virtual void build() override;
    virtual void destroy(const vui::GameTime& gameTime) override;

    virtual void onEntry(const vui::GameTime& gameTime) override;
    virtual void onExit(const vui::GameTime& gameTime) override;

    virtual void update(const vui::GameTime& gameTime) override;

    virtual void draw(const vui::GameTime& gameTime) override;

private:
    void drawMainState();
    void drawMultiplayerState();
    void drawJoinState();
    void drawLanJoinState();
    void drawOnlineJoinState();
    void drawHostState();

    MainMenuState mState = MainMenuState::MAIN;
};

