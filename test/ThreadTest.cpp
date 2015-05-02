/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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

#include <gtest/gtest.h>
#include <znc/Threads.h>
#include <sys/select.h>

class CWaitingJob : public CJob {
public:
	CWaitingJob(bool& destroyed)
		: m_bDestroyed(destroyed), m_Mutex(), m_CV(), m_bThreadReady(false), m_bThreadDone(false) {
	};


	~CWaitingJob() {
		EXPECT_TRUE(m_bThreadReady);
		EXPECT_TRUE(m_bThreadDone);
		EXPECT_FALSE(wasCancelled());
		m_bDestroyed = true;
	}

	void signal() {
		CMutexLocker locker(m_Mutex);
		// Wait for the thread to run
		while (!m_bThreadReady)
			m_CV.wait(m_Mutex);

		// and signal it to exit
		m_bThreadDone = true;
		m_CV.broadcast();
	}

	virtual void runThread() {
		CMutexLocker locker(m_Mutex);
		// We are running
		m_bThreadReady = true;
		m_CV.broadcast();

		// wait for our exit signal
		while (!m_bThreadDone)
			m_CV.wait(m_Mutex);
	}

	virtual void runMain() {}

private:
	bool& m_bDestroyed;
	CMutex m_Mutex;
	CConditionVariable m_CV;
	bool m_bThreadReady;
	bool m_bThreadDone;
};

TEST(Thread, RunJob) {
	bool destroyed = false;
	CWaitingJob *pJob = new CWaitingJob(destroyed);

	CThreadPool::Get().addJob(pJob);
	pJob->signal();

	while (!destroyed)
		CThreadPool::Get().handlePipeReadable();
}

class CCancelJob : public CJob {
public:
	CCancelJob(bool& destroyed)
		: m_bDestroyed(destroyed), m_Mutex(), m_CVThreadReady(), m_bThreadReady(false) {
	}

	~CCancelJob() {
		EXPECT_TRUE(wasCancelled());
		m_bDestroyed = true;
	}

	void wait() {
		CMutexLocker locker(m_Mutex);
		// Wait for the thread to run
		while (!m_bThreadReady)
			m_CVThreadReady.wait(m_Mutex);
	}

	virtual void runThread() {
		m_Mutex.lock();
		// We are running, tell the main thread
		m_bThreadReady = true;
		m_CVThreadReady.broadcast();
		// Have to unlock here so that wait() can get the mutex
		m_Mutex.unlock();

		while (!wasCancelled()) {
			// We can't do much besides busy-looping here. If the
			// job really gets cancelled while it is already
			// running, the main thread is stuck in cancelJob(), so
			// it cannot signal us in any way. And signaling us
			// before calling cancelJob() effictively is the same
			// thing as busy looping anyway. So busy looping it is.
			// (Yes, CJob shouldn't be used for anything that
			// requires synchronisation between threads!)
		}
	}

	virtual void runMain() { }

private:
	bool& m_bDestroyed;
	CMutex m_Mutex;
	CConditionVariable m_CVThreadReady;
	bool m_bThreadReady;
};

TEST(Thread, CancelJobEarly) {
	bool destroyed = false;
	CCancelJob *pJob = new CCancelJob(destroyed);

	CThreadPool::Get().addJob(pJob);
	// Don't wait for the job to run. The idea here is that we are calling
	// cancelJob() before pJob->runThread() runs, but this is a race.
	CThreadPool::Get().cancelJob(pJob);

	// cancelJob() should only return after successful cancellation
	EXPECT_TRUE(destroyed);
}

TEST(Thread, CancelJobWhileRunning) {
	bool destroyed = false;
	CCancelJob *pJob = new CCancelJob(destroyed);

	CThreadPool::Get().addJob(pJob);
	// Wait for the job to run
	pJob->wait();
	CThreadPool::Get().cancelJob(pJob);

	// cancelJob() should only return after successful cancellation
	EXPECT_TRUE(destroyed);
}

class CEmptyJob : public CJob {
public:
	CEmptyJob(bool& destroyed)
		: m_bDestroyed(destroyed) {
	}

	~CEmptyJob() {
		EXPECT_TRUE(wasCancelled());
		m_bDestroyed = true;
	}

	virtual void runThread() { }
	virtual void runMain() { }

private:
	bool& m_bDestroyed;
};

TEST(Thread, CancelJobWhenDone) {
	bool destroyed = false;
	CEmptyJob *pJob = new CEmptyJob(destroyed);

	CThreadPool::Get().addJob(pJob);

	// Wait for the job to finish
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(CThreadPool::Get().getReadFD(), &fds);
	EXPECT_EQ(1, select(1 + CThreadPool::Get().getReadFD(), &fds, nullptr, nullptr, nullptr));

	// And only cancel it afterwards
	CThreadPool::Get().cancelJob(pJob);

	// cancelJob() should only return after successful cancellation
	EXPECT_TRUE(destroyed);
}
