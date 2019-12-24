#include "stdafx.h"

//#ifdef VORB_OS_WINDOWS
//#include <SDL/SDL_syswm.h>
//#endif

#include <iostream>
#include <Vorb/Vorb.h>
#include <Vorb/VorbLibs.h>

#include "App.h"

using namespace std;

int main(int argc, char **argv) {
    // Initialize Vorb modules
    vorb::init(vorb::InitParam::ALL);

    App app;

    app.run();

#ifdef VORB_OS_WINDOWS
    // Tell windows that our priority class should be above normal
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
#endif

    return 0;
}