/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Threads.h>

#ifdef HAVE_PTHREAD

/* Just an arbitrary limit for the number of idle threads */
static const size_t MAX_IDLE_THREADS = 3;

/* Just an arbitrary limit for the number of running threads */
static const size_t MAX_TOTAL_THREADS = 20;

CThreadPool& CThreadPool::Get() {
	// Beware! The following is not thread-safe! This function must
	// be called once any thread is started.
	static CThreadPool pool;
	return pool;
}

CThreadPool::~CThreadPool() {
	/* Anyone has an idea how this can be done less ugly? */
	CMutexLocker guard(m_mutex);
	m_done = true;

	while (m_num_threads > 0) {
		m_cond.broadcast();
		guard.unlock();
		usleep(100);
		guard.lock();
	}
}

bool CThreadPool::threadNeeded() const {
	if (m_num_idle > MAX_IDLE_THREADS)
		return false;
	return !m_done;
}

void CThreadPool::threadFunc() {
	CMutexLocker guard(m_mutex);
	m_num_threads++;
	m_num_idle++;

	while (true) {
		while (m_jobs.empty()) {
			if (!threadNeeded())
				break;
			m_cond.wait(m_mutex);
		}
		if (!threadNeeded())
			break;

		// Figure out a job to do
		CJob *job = m_jobs.front();
		m_jobs.pop_front();

		// Now do the actual job
		m_num_idle--;
		guard.unlock();

		job->run();

		guard.lock();
		m_num_idle++;
	}
	assert(m_num_threads > 0 && m_num_idle > 0);
	m_num_threads--;
	m_num_idle--;
}

void CThreadPool::addJob(CJob *job) {
	CMutexLocker guard(m_mutex);
	m_jobs.push_back(job);

	// Do we already have a thread which can handle this job?
	if (m_num_idle > 0) {
		m_cond.signal();
		return;
	}

	if (m_num_threads >= MAX_TOTAL_THREADS)
		// We can't start a new thread. The job will be handled once
		// some thread finishes its current job.
		return;

	// Start a new thread for our pool
	CThread::startThread(threadPoolFunc, this);
}

#endif // HAVE_PTHREAD
