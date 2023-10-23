#pragma once

struct EditorSettings {
    //---------- Content Browser ------------
    int contentBrowserThumbnailSize = 96;

    inline static EditorSettings& get() {
        static EditorSettings sInstance;
        return sInstance;
    }
};