/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ZNC_THREADS_H
#define ZNC_THREADS_H

#include <znc/zncconfig.h>

#ifdef HAVE_PTHREAD

#include <znc/Utils.h>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <list>
#include <pthread.h>
#include <mutex>
#include <condition_variable>

/**
 * This class represents a non-recursive mutex. Only a single thread may own the
 * mutex at any point in time.
 */
using CMutex = std::mutex;

/**
 * A mutex locker should always be used as an automatic variable. This
 * class makes sure that the mutex is unlocked when this class is destructed.
 * For example, this makes it easier to make code exception-safe.
 */
using CMutexLocker = std::unique_lock<std::mutex>;

/**
 * A condition variable makes it possible for threads to wait until some
 * condition is reached at which point the thread can wake up again.
 */
using CConditionVariable = std::condition_variable_any;

/**
 * A job is a task which should run without blocking the main thread. You do
 * this by inheriting from this class and implementing the pure virtual methods
 * runThread(), which gets executed in a separate thread and does not block the
 * main thread, and runMain() which gets automatically called from the main
 * thread after runThread() finishes.
 *
 * After you create a new instance of your class, you can pass it to
 * CThreadPool()::Get().addJob(job) to start it. The thread pool automatically
 * deletes your class after it finished.
 *
 * For modules you should use CModuleJob instead.
 */
class CJob {
  public:
    friend class CThreadPool;

    enum EJobState { READY, RUNNING, DONE, CANCELLED };

    CJob() : m_eState(READY) {}

    /// Destructor, always called from the main thread.
    virtual ~CJob() {}

    /// This function is called in a separate thread and can do heavy, blocking work.
    virtual void runThread() = 0;

    /// This function is called from the main thread after runThread()
    /// finishes. It can be used to handle the results from runThread()
    /// without needing synchronization primitives.
    virtual void runMain() = 0;

    /// This can be used to check if the job was cancelled. For example,
    /// runThread() can return early if this returns true.
    bool wasCancelled() const;

  private:
    // Undefined copy constructor and assignment operator
    CJob(const CJob&);
    CJob& operator=(const CJob&);

    // Synchronized via the thread pool's mutex! Do not access without that
    // mutex!
    EJobState m_eState;
};

class CThreadPool {
  private:
    friend class CJob;

    CThreadPool();
    ~CThreadPool();

  public:
    static CThreadPool& Get();

    /// Add a job to the thread pool and run it. The job will be deleted when done.
    void addJob(CJob* job);

    /// Cancel a job that was previously passed to addJob(). This *might*
    /// mean that runThread() and/or runMain() will not be called on the job.
    /// This function BLOCKS until the job finishes!
    void cancelJob(CJob* job);

    /// Cancel some jobs that were previously passed to addJob(). This *might*
    /// mean that runThread() and/or runMain() will not be called on some of
    /// the jobs. This function BLOCKS until all jobs finish!
    void cancelJobs(const std::set<CJob*>& jobs);

    int getReadFD() const { return m_iJobPipe[0]; }

    void handlePipeReadable() const;

  private:
    void jobDone(CJob* pJob);

    // Check if the calling thread is still needed, must be called with m_mutex
    // held
    bool threadNeeded() const;

    CJob* getJobFromPipe() const;
    void finishJob(CJob*) const;

    void threadFunc();

    // mutex protecting all of these members
    CMutex m_mutex;

    // condition variable for waiting idle threads
    CConditionVariable m_cond;

    // condition variable for reporting finished cancellation
    CConditionVariable m_cancellationCond;

    // condition variable for waiting running threads == 0
    CConditionVariable m_exit_cond;

    // when this is true, all threads should exit
    bool m_done;

    // total number of running threads
    size_t m_num_threads;

    // number of idle threads waiting on the condition variable
    size_t m_num_idle;

    // pipe for waking up the main thread
    int m_iJobPipe[2];

    // list of pending jobs
    std::list<CJob*> m_jobs;
};

#endif  // HAVE_PTHREAD
#endif  // !ZNC_THREADS_H
