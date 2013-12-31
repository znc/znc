/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
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

#ifndef _THREADS_H
#define _THREADS_H

#include <znc/zncconfig.h>

#ifdef HAVE_PTHREAD

#include <znc/Utils.h>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <list>
#include <pthread.h>

/**
 * This class represents a non-recursive mutex. Only a single thread may own the
 * mutex at any point in time.
 */
class CMutex {
public:
	friend class CConditionVariable;

	CMutex() {
		int i = pthread_mutex_init(&m_mutex, NULL);
		if (i) {
			CUtils::PrintError("Can't initialize mutex: " + CString(strerror(errno)));
			exit(1);
		}
	}

	~CMutex() {
		int i = pthread_mutex_destroy(&m_mutex);
		if (i) {
			CUtils::PrintError("Can't destroy mutex: " + CString(strerror(errno)));
			exit(1);
		}
	}

	void lock() {
		int i = pthread_mutex_lock(&m_mutex);
		if (i) {
			CUtils::PrintError("Can't lock mutex: " + CString(strerror(errno)));
			exit(1);
		}
	}

	void unlock() {
		int i = pthread_mutex_unlock(&m_mutex);
		if (i) {
			CUtils::PrintError("Can't unlock mutex: " + CString(strerror(errno)));
			exit(1);
		}
	}

private:
	pthread_mutex_t m_mutex;
};

/**
 * A mutex locker should always be used as an automatic variable. This
 * class makes sure that the mutex is unlocked when this class is destructed.
 * For example, this makes it easier to make code exception-safe.
 */
class CMutexLocker {
public:
	CMutexLocker(CMutex& mutex, bool initiallyLocked = true)
			: m_mutex(mutex), m_locked(false) {
		if (initiallyLocked)
			lock();
	}

	~CMutexLocker() {
		if (m_locked)
			unlock();
	}

	void lock() {
		assert(!m_locked);
		m_mutex.lock();
		m_locked = true;
	}

	void unlock() {
		assert(m_locked);
		m_locked = false;
		m_mutex.unlock();
	}

private:
	CMutex &m_mutex;
	bool m_locked;
};

/**
 * A condition variable makes it possible for threads to wait until some
 * condition is reached at which point the thread can wake up again.
 */
class CConditionVariable {
public:
	CConditionVariable() {
		int i = pthread_cond_init(&m_cond, NULL);
		if (i) {
			CUtils::PrintError("Can't initialize condition variable: "
					+ CString(strerror(errno)));
			exit(1);
		}
	}

	~CConditionVariable() {
		int i = pthread_cond_destroy(&m_cond);
		if (i) {
			CUtils::PrintError("Can't destroy condition variable: "
					+ CString(strerror(errno)));
			exit(1);
		}
	}

	void wait(CMutex& mutex) {
		int i = pthread_cond_wait(&m_cond, &mutex.m_mutex);
		if (i) {
			CUtils::PrintError("Can't wait on condition variable: "
					+ CString(strerror(errno)));
			exit(1);
		}
	}

	void signal() {
		int i = pthread_cond_signal(&m_cond);
		if (i) {
			CUtils::PrintError("Can't signal condition variable: "
					+ CString(strerror(errno)));
			exit(1);
		}
	}

	void broadcast() {
		int i = pthread_cond_broadcast(&m_cond);
		if (i) {
			CUtils::PrintError("Can't broadcast condition variable: "
					+ CString(strerror(errno)));
			exit(1);
		}
	}

private:
	pthread_cond_t m_cond;
};

class CThread {
public:
	typedef void *threadRoutine(void *);
	static void startThread(threadRoutine *func, void *arg) {
		pthread_t thr;
		sigset_t old_sigmask, sigmask;

		/* Block all signals. The thread will inherit our signal mask
		 * and thus won't ever try to handle signals.
		 */
		int i = sigfillset(&sigmask);
		i |= pthread_sigmask(SIG_SETMASK, &sigmask, &old_sigmask);
		i |= pthread_create(&thr, NULL, func, arg);
		i |= pthread_sigmask(SIG_SETMASK, &old_sigmask, NULL);
		i |= pthread_detach(thr);
		if (i) {
			CUtils::PrintError("Can't start new thread: "
					+ CString(strerror(errno)));
			exit(1);
		}
	}
};

class CJob {
public:
	virtual ~CJob() {}
	virtual void runThread() = 0;
	virtual void runMain() = 0;
};

class CThreadPool {
private:
	CThreadPool();
	~CThreadPool();

public:
	static CThreadPool& Get();

	void addJob(CJob *job);

	int getReadFD() const {
		return m_iJobPipe[0];
	}

	void handlePipeReadable() const;

private:
	void jobDone(const CJob* pJob) const;

	// Check if the calling thread is still needed, must be called with m_mutex held
	bool threadNeeded() const;

	void threadFunc();
	static void *threadPoolFunc(void *arg) {
		CThreadPool &pool = *reinterpret_cast<CThreadPool *>(arg);
		pool.threadFunc();
		return NULL;
	}

	// mutex protecting all of these members
	CMutex m_mutex;

	// condition variable for waiting idle threads
	CConditionVariable m_cond;

	// when this is true, all threads should exit
	bool m_done;

	// total number of running threads
	size_t m_num_threads;

	// number of idle threads waiting on the condition variable
	size_t m_num_idle;

	// pipe for waking up the main thread
	int m_iJobPipe[2];

	// list of pending jobs
	std::list<CJob *> m_jobs;
};

#endif // HAVE_PTHREAD
#endif // !_THREADS_H
