#pragma once

class IWorld;

class WorldFactory
{
public:
    static std::unique_ptr<IWorld> makeClientWorld();
    static std::unique_ptr<IWorld> makeHostWorld();
    // TODO:
    //static std::unique_ptr<IWorld> makeServerWorld();
};

