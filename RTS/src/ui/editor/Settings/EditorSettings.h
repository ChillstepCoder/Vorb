#pragma once

struct EditorSettings {
    //---------- Content Browser ------------
    int contentBrowserThumbnailSize = 128;

    inline static EditorSettings& get() {
        static EditorSettings sInstance;
        return sInstance;
    }
};