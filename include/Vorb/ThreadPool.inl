
#include <Vorb/logging/Logger.h>

template<typename T>
vorb::core::ThreadPool<T>::ThreadPool(ui32 size) {
    /// Allocate all threads
    m_workers.resize(size);
    for (ui32 i = 0; i < size; i++) {
        m_workers[i] = new WorkerThread(&ThreadPool::workerThreadFunc, this);
    }
}


template<typename T>
vcore::ThreadPool<T>::~ThreadPool() {
    mStop.store(true);

    // Clear out the queue
    clearTasks();

    // Tell all threads to wake up and close, as they are currently hanging on a semaphore
    for (size_t i = 0; i < m_workers.size(); i++) {
        addTask([](T*) {
            return;
        }, nullptr);
    }

    // Join all threads
    for (size_t i = 0; i < m_workers.size(); i++) {
        m_workers[i]->join();
        delete m_workers[i];
    }
}

template<typename T>
void vcore::ThreadPool<T>::clearTasks() {
    // Dequeue all tasks
    ThreadPoolTaskProcs<T> task;
    while (mTasks.try_dequeue(task));
}

template<typename T>
void vorb::core::ThreadPool<T>::mainThreadUpdate() {
    std::function<void()> proc;
    // TODO: bulk dequeue?
    constexpr unsigned MAX_MS = 6;
    PreciseTimer timer;
    // TODO: Use optik for profiling
    while (mMainThreadProcs.try_dequeue(proc)) {
        proc();
        if (timer.stop() > MAX_MS) {
            break;
        }
    }
    //std::cout << "Main thread processed " << i << " tasks in " << timer.stop() << " ms\n";
    if (timer.stop() > 20.0f) {
        LOG_WARN("{} ms ***THREADPOOL SPIKE WARNING***", timer.stop());
    }
}

template<typename T>
void vcore::ThreadPool<T>::workerThreadFunc(T* data) {
    ThreadPoolTaskProcs<T> task;

    while (!mStop.load()) {
        // Note that threads will be stuck waiting here until the process ends
        mTasks.wait_dequeue(task);
        task.first(data);
        if (task.second) {
            mMainThreadProcs.enqueue(std::move(task.second));
        }
    }
}
