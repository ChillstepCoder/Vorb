#pragma once
class RenderState {
    friend class CliWorldInterface;
public:
    const f32v2& getWorldLoadCenter() const { return mWorldLoadCenter; }
    const f32v3& getCameraOwningEntityPos() const { return mCameraOwningEntityPos; }
private:
    f32v2 mWorldLoadCenter;
    f32v3 mCameraOwningEntityPos;
};

