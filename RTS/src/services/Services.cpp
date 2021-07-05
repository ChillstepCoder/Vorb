#include "stdafx.h"
#include "Services.h"

#include "pathfinding/PathFinder.h"
#include "ResourceManager.h"

static bool sIsInit = false;

void Services::init()
{
    assert(!sIsInit);
    sIsInit = true;

    std::cout << "Initializing services:\n";

    const int threadCount = vmath::max<int>(std::thread::hardware_concurrency() - 1, 1);
    std::cout << "  Initializing threadpool with " << threadCount << " threads.\n";
    Threadpool::set(threadCount);
    PathFinder::set();
    ResourceManager::set();
}

void Services::destroy()
{
    assert(sIsInit);
    sIsInit = false;

    Threadpool::reset();
    PathFinder::reset();
    ResourceManager::reset();
}