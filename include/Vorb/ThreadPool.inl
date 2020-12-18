template<typename T>
vcore::ThreadPool<T>::~ThreadPool() {
    destroy();
}

template<typename T>
void vcore::ThreadPool<T>::clearTasks() {
    // Dequeue all tasks
    ThreadPoolTaskProcs<T> task;
    // TODO: Bulk dequeue? Why are we double? lol
    while (mTasks.try_dequeue(task));
    while (mTasks.try_dequeue(task));
}

template<typename T>
void vcore::ThreadPool<T>::init(ui32 size) {
    // Check if its already initialized
    assert(!m_isInitialized);
    m_isInitialized = true;

    /// Allocate all threads
    m_workers.resize(size);
    for (ui32 i = 0; i < size; i++) {
        m_workers[i] = new WorkerThread(&ThreadPool::workerThreadFunc, this);
    }
}

template<typename T>
void vorb::core::ThreadPool<T>::mainThreadUpdate() {
    assert(m_isInitialized);

    std::function<void()> proc;
    // TODO: limit + bulk dequeue
    while (mMainThreadProcs.try_dequeue(proc)) {
        proc();
    }
}

template<typename T>
void vcore::ThreadPool<T>::destroy() {
    if (!m_isInitialized) return;

    mStop.store(true);

    // Join all threads
    for (size_t i = 0; i < m_workers.size(); i++) {
        m_workers[i]->join();
        delete m_workers[i];
    }

    clearTasks();

    // Free memory
    std::vector<WorkerThread*>().swap(m_workers);

    // We are no longer initialized
    m_isInitialized = false;
}

template<typename T>
void vcore::ThreadPool<T>::workerThreadFunc(T* data) {
    ThreadPoolTaskProcs<T> task;

    while (true) {
        // Check for exit
        if (mStop.load()) return;

        if (mTasks.try_dequeue(task)) {
            task.first(data);
            if (task.second) {
                mMainThreadProcs.enqueue(task.second);
            }
        }
        else {
            // Don't just spin
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}
