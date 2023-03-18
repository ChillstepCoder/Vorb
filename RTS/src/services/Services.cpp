#include "stdafx.h"
#include "Services.h"

#include "pathfinding/NavThread.h"
#include "resources/ResourceManager.h"
#include "city/contracts/ContractManager.h"

static bool sIsInit = false;
bool Services::sUsingNav = false;

void Services::initResources() {
    ResourceManager::set();
}

void Services::initHost()
{
    assert(!sIsInit);
    sIsInit = true;

    LOG_INFO("Initializing host services:");

    sUsingNav = true;
    initThreads();
    ContractManager::set();
}

void Services::initCli()
{
    assert(!sIsInit);
    sIsInit = true;

    LOG_INFO("Initializing client services:");

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
        ContractManager::reset();
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
    LOG_INFO("  Initializing threadpool with {} threads. ", threadCount);
    Threadpool::set(threadCount);
    if (sUsingNav) {
        NavThread::set();
    }
}
