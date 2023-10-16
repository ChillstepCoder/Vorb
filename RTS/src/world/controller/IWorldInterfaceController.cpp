#include "stdafx.h"
#include "IWorldInterfaceController.h"

IWorldInterfaceController::IWorldInterfaceController(vui::GameWindow& window, World& world, CameraController& cameraController) : mWindow(&window), mWorld(&world), mCameraController(&cameraController) {

}
