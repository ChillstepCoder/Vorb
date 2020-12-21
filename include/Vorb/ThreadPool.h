//
// ThreadPool.h
// Vorb Engine
//
// Created by Benjamin Arnold on 13 Nov 2014
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file ThreadPool.h
 * @brief Provides a general threadpool implementation for distributing work.
 */

#pragma once

#ifndef Vorb_ThreadPool_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_ThreadPool_h__
//! @endcond

#ifndef VORB_USING_PCH
#include <vector>

#include "Vorb/types.h"
#endif // !VORB_USING_PCH

#include <functional>
#include <thread>
#include <condition_variable>

#include <Vorb/concurrentqueue.h>
#include <Vorb/blockingconcurrentqueue.h>

#include "Vorb/IThreadPoolTask.h"


template<typename T>
using ThreadPoolTaskProcs = std::pair<std::function<void(T*)>, std::function<void()>>;

namespace vorb {
    namespace core {

        template<typename T>
        class ThreadPool {
        public:
            ThreadPool(ui32 size);
            ~ThreadPool();

            void mainThreadUpdate();

            /// Clears all unprocessed tasks from the task queue
            void clearTasks();

            /// Adds a task to the task queue
            /// @param task: The task to add
            void addTask(std::function<void(T*)> workerProc, std::function<void()> mainProc) {
                mTasks.enqueue(std::make_pair(workerProc, mainProc));
            }

            /// Add an array of tasks to the task queue
            /// @param tasks: The array of tasks to add
            /// @param size: The size of the array
            /*void addTasks(IThreadPoolTask<T>* tasks[], size_t size) {
                mTasks.enqueue_bulk(tasks, size);
            }*/

            /// Getters
            i32 getNumWorkers() const { return m_workers.size(); }
            size_t getTasksSizeApprox() const { return mTasks.size_approx(); }
        private:
            VORB_NON_COPYABLE(ThreadPool);
            // Typedef for func ptr
            typedef void (ThreadPool<T>::*workerFunc)(T*);

            /// Class definition for worker thread
            class WorkerThread {
            public:
                /// Creates the thread
                /// @param func: The function the thread should execute
                WorkerThread(workerFunc func, ThreadPool<T>* threadPool) {
                    thread = std::make_unique<std::thread>(func, threadPool, &data);
                }

                ~WorkerThread() {

                }
                /// Blocks until the worker thread completes
                void join() {
                    thread->join();
                }

                std::unique_ptr<std::thread> thread; ///< The thread handle
                T data; ///< Worker specific data
            };

            /// Thread function that processes tasks
            /// @param data: The worker specific data
            void workerThreadFunc(T* data);

            /// Lock free task queues
            moodycamel::BlockingConcurrentQueue<ThreadPoolTaskProcs<T>> mTasks; ///< Holds tasks to execute
            moodycamel::ConcurrentQueue<std::function<void()>> mMainThreadProcs; ///< Contains functions to run on main thread after complete
            std::atomic_bool mStop = false;
           
            std::vector<WorkerThread*> m_workers; ///< All the worker threads
        };

    }
}
namespace vcore = vorb::core;

#include "ThreadPool.inl"

#endif // !Vorb_ThreadPool_h__
