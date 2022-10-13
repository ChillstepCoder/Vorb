#include "stdafx.h"
#include "Services.h"

#include "pathfinding/NavThread.h"
#include "resources/ResourceManager.h"

static bool sIsInit = false;
bool Services::sUsingNav = false;

void Services::initResources() {
    ResourceManager::set();
}

void Services::initHost()
{
    assert(!sIsInit);
    sIsInit = true;

    std::cout << "Initializing services:\n";

    sUsingNav = true;
    initThreads();
}

void Services::initCli()
{
    assert(!sIsInit);
    sIsInit = true;

    std::cout << "Initializing services:\n";

    sUsingNav = false;
    initThreads();
}

void Services::destroy()
{
    if (sIsInit) {
        sIsInit = false;
        sUsingNav = false;

        Threadpool::reset();
        NavThread::reset();
    }
}

void Services::destroyResources()
{
    ResourceManager::reset();
}

void Services::resetThreads()
{
    Threadpool::reset();
    NavThread::reset();
    initThreads();
}

void Services::initThreads()
{
    // - 2 threads for main thread + nav thread
    const int threadCount = vmath::max<int>(std::thread::hardware_concurrency() - 2, 1);
    std::cout << "  Initializing threadpool with " << threadCount << " threads.\n";
    Threadpool::set(threadCount);
    if (sUsingNav) {
        NavThread::set();
    }
}
