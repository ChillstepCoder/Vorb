#pragma once

struct GameDateTime {
    i32 day;
    f32 timeHours; // [0.0, 24.0)

    nString toString() const {
        return std::format("{} {:02}:{:02}",
            day,
            static_cast<int>(timeHours),
            static_cast<int>((timeHours - static_cast<int>(timeHours)) * 60.0f));
    }
};

class TimeOfDayManager
{
public:
    void setTimeOfDay(f32 timeHours);
    void updateTimeOfDay(f32 timePassedHours);

    // [-1.0, 1.0]
    // TODO: Render thread accesses all of this! Race conditions!
    float getSunHeight() const { return mSunHeight; }
    f32v3 getSunPosition() const { return mSunPosition; }
    float getTimeOfDayHours() const { return mTimeOfDayHours; }
    f32v3 getSunColor() const { return mSunColor; }
    const f32m4& getSkyRotMatrix() const { return mSkyRotMatrix; }

    // Return [0.0, 24.0)
    static GameDateTime convertTimestampMSToDateTime(TimestampMs ms);

private:
    // Sunlight and time of day
    float mSunHeight = 1.0f;
    f32v3 mSunPosition = f32v3(0.0f, 0.0f, 1.0f);
    float mTimeOfDayHours = 0.0f; // span of 24:00
    f32v3 mSunColor = f32v3(1.0f);
    f32m4 mSkyRotMatrix = f32m4(1.0f);
};

