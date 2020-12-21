#include "stdafx.h"
#include "Services.h"

static bool sIsInit = false;

void Services::init()
{
    assert(!sIsInit);
    sIsInit = true;

    std::cout << "Initializing services:\n";

    const int threadCount = vmath::max<int>(std::thread::hardware_concurrency() - 1, 1);
    std::cout << "  Initializing threadpool with " << threadCount << " threads.\n";
    Threadpool::set(threadCount);
}

void Services::destroy()
{
    assert(sIsInit);
    sIsInit = false;

    Threadpool::reset();
}