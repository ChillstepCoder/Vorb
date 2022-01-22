#include "stdafx.h"
#include "Services.h"

#include "pathfinding/NavThread.h"
#include "ResourceManager.h"

static bool sIsInit = false;

void Services::init()
{
    assert(!sIsInit);
    sIsInit = true;

    std::cout << "Initializing services:\n";

    // - 2 threads for main thread + nav thread
    const int threadCount = vmath::max<int>(std::thread::hardware_concurrency() - 2, 1);
    std::cout << "  Initializing threadpool with " << threadCount << " threads.\n";
    Threadpool::set(threadCount);
    NavThread::set();
    ResourceManager::set();
}

void Services::destroy()
{
    assert(sIsInit);
    sIsInit = false;

    Threadpool::reset();
    NavThread::reset();
    ResourceManager::reset();
}