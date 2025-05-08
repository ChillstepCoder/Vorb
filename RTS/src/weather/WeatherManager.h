#pragma once

class World;

class WeatherManager
{
public:
    WeatherManager(World& world);
    ~WeatherManager();

    f32 mSnowLevel = 0.0f;
private:
    World& mWorld;
};

