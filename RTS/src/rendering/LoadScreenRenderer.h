#pragma once

DECL_VUI(class GameWindow);
DECL_VG(class TextureCache);

class LoadScreenRenderer
{
protected:
    LoadScreenRenderer();
    ~LoadScreenRenderer();

public:
    LoadScreenRenderer(LoadScreenRenderer& other) = delete;
    void operator=(const LoadScreenRenderer&) = delete;

    static LoadScreenRenderer& getInstance();

    void render(OPT vui::GameWindow* windowToSync);
    
    void appendLoadingTexture(const vio::Path& path, vg::TextureCache& textureCache, bool setActive = false);
    void setLoadingTexture(int index);
    void setShowBar(bool showBar);
    void setTotalWork(f32 totalWork);
    void incWork(f32 workDone);
    void setWorkDone(f32 workDone);
    void setText(const nString& text);

private:
    bool mShowBar = false;
    f32 mTotalWork = 0.0f;
    f32 mWorkDone = 0.0f;
    int mCurrentBackgroundTexture = 0;
    std::vector<VGTexture> mBackgroundTextures;
    nString mText;


    static LoadScreenRenderer* sInstance;
};

