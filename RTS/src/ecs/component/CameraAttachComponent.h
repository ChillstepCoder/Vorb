#pragma once

class World;
#include "camera/Camera3D.h"

// Simply an identifier : Represents attaching to an entity in the world
//  TODO: Add dynamic camera settings here instead of driven by render thread?
//        Camera can be driven completely by the game thread, and all values can be interpolated/predicted
//        on the render thread. This allows us to query the physics engine for camera collision, while retaining smooth
//        camera motion.
//        For this to work the input system needs to send events directly to the game thread in a queue. This gives us at most about
//        game thread hz in ping, less if we drain input queue multiple times per frame so good tick rate means less input delay.
// ALTERNATIVE: If the neraby physics data is copied over to the game thread and used only for camera queries, we can have a fully render
//      thread controlled camera that also interacts with its environment. Raycast impacts can apply real time effects with ease such as looking
//      through objects
// TODO: CameraComponent rename?

// Currently going with the copy physics data to render thread idea since it is cooler and probably
//     the most responsive, and opens up the idea for the render thread to interpolate all physics. Downside
//     is if we use full physics it will impact render thread performance so sticking to simple queries for now is probably
//     best, or keeping the world very small
struct CameraAttachComponent {
    //Camera3D mCamera;
};

class CameraAttachSystem {
public:
    //void update(IWorld& world, entt::registry& registry);
};