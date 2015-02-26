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

#include <znc/Threads.h>

#ifdef HAVE_PTHREAD

#include <znc/ZNCDebug.h>
#include <algorithm>

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

CThreadPool::CThreadPool() : m_done(false), m_num_threads(0), m_num_idle(0) {
	if (pipe(m_iJobPipe)) {
		DEBUG("Ouch, can't open pipe for thread pool: " << strerror(errno));
		exit(1);
	}
}

void CThreadPool::jobDone(CJob* job) {
	// This must be called with the mutex locked!

	enum CJob::EJobState oldState = job->m_eState;
	job->m_eState = CJob::DONE;

	if (oldState == CJob::CANCELLED) {
		// Signal the main thread that cancellation is done
		m_cancellationCond.signal();
		return;
	}

	// This write() must succeed because POSIX guarantees that writes of
	// less than PIPE_BUF are atomic (and PIPE_BUF is at least 512).
	// (Yes, this really wants to write a pointer(!) to the pipe.
	size_t w = write(m_iJobPipe[1], &job, sizeof(job));
	if (w != sizeof(job)) {
		DEBUG("Something bad happened during write() to a pipe for thread pool, wrote " << w << " bytes: " << strerror(errno));
		exit(1);
	}
}

void CThreadPool::handlePipeReadable() const {
	finishJob(getJobFromPipe());
}

CJob *CThreadPool::getJobFromPipe() const {
	CJob* a = nullptr;
	ssize_t need = sizeof(a);
	ssize_t r = read(m_iJobPipe[0], &a, need);
	if (r != need) {
		DEBUG("Something bad happened during read() from a pipe for thread pool: " << strerror(errno));
		exit(1);
	}
	return a;
}

void CThreadPool::finishJob(CJob *job) const {
	job->runMain();
	delete job;
}

CThreadPool::~CThreadPool() {
	CMutexLocker guard(m_mutex);
	m_done = true;

	while (m_num_threads > 0) {
		m_cond.broadcast();
		m_exit_cond.wait(m_mutex);
	}
}

bool CThreadPool::threadNeeded() const {
	if (m_num_idle > MAX_IDLE_THREADS)
		return false;
	return !m_done;
}

void CThreadPool::threadFunc() {
	CMutexLocker guard(m_mutex);
	// m_num_threads was already increased
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
		job->m_eState = CJob::RUNNING;
		guard.unlock();

		job->runThread();

		guard.lock();
		jobDone(job);
		m_num_idle++;
	}
	assert(m_num_threads > 0 && m_num_idle > 0);
	m_num_threads--;
	m_num_idle--;

	if (m_num_threads == 0 && m_done)
		m_exit_cond.signal();
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
	m_num_threads++;
	CThread::startThread(threadPoolFunc, this);
}

void CThreadPool::cancelJob(CJob *job) {
	std::set<CJob *> jobs;
	jobs.insert(job);
	cancelJobs(jobs);
}

void CThreadPool::cancelJobs(const std::set<CJob *> &jobs) {
	// Thanks to the mutex, jobs cannot change state anymore. There are
	// three different states which can occur:
	//
	// READY: The job is still in our list of pending jobs and no threads
	// got it yet. Just clean up.
	//
	// DONE: The job finished running and was already written to the pipe
	// that is used for waking up finished jobs. We can just read from the
	// pipe until we see this job.
	//
	// RUNNING: This is the complicated case. The job is currently being
	// executed. We change its state to CANCELLED so that wasCancelled()
	// returns true. Afterwards we wait on a CV for the job to have finished
	// running. This CV is signaled by jobDone() which checks the job's
	// status and sees that the job was cancelled. It signals to us that
	// cancellation is done by changing the job's status to DONE.

	CMutexLocker guard(m_mutex);
	std::set<CJob *> wait, finished, deleteLater;
	std::set<CJob *>::const_iterator it;

	// Start cancelling all jobs
	for (it = jobs.begin(); it != jobs.end(); ++it) {
		switch ((*it)->m_eState) {
		case CJob::READY: {
			(*it)->m_eState = CJob::CANCELLED;

			// Job wasn't started yet, must be in the queue
			std::list<CJob *>::iterator it2 = std::find(m_jobs.begin(), m_jobs.end(), *it);
			assert(it2 != m_jobs.end());
			m_jobs.erase(it2);
			deleteLater.insert(*it);
			continue;
		}

		case CJob::RUNNING:
			(*it)->m_eState = CJob::CANCELLED;
			wait.insert(*it);
			continue;

		case CJob::DONE:
			(*it)->m_eState = CJob::CANCELLED;
			finished.insert(*it);
			continue;

		case CJob::CANCELLED:
		default:
			assert(0);
		}
	}

	// Now wait for cancellation to be done

	// Collect jobs that really were cancelled. Finished cancellation is
	// signaled by changing their state to DONE.
	while (!wait.empty()) {
		it = wait.begin();
		while (it != wait.end()) {
			if ((*it)->m_eState != CJob::CANCELLED) {
				assert((*it)->m_eState == CJob::DONE);
				// Re-set state for the destructor
				(*it)->m_eState = CJob::CANCELLED;;
				deleteLater.insert(*it);
				wait.erase(it++);
			} else
				it++;
		}

		if (wait.empty())
			break;

		// Then wait for more to be done
		m_cancellationCond.wait(m_mutex);
	}

	// We must call destructors with m_mutex unlocked so that they can call wasCancelled()
	guard.unlock();

	// Handle finished jobs. They must already be in the pipe.
	while (!finished.empty()) {
		CJob *job = getJobFromPipe();
		if (finished.erase(job) > 0) {
			assert(job->m_eState == CJob::CANCELLED);
			delete job;
		} else
			finishJob(job);
	}

	// Delete things that still need to be deleted
	while (!deleteLater.empty()) {
		delete *deleteLater.begin();
		deleteLater.erase(deleteLater.begin());
	}
}

bool CJob::wasCancelled() const {
	CMutexLocker guard(CThreadPool::Get().m_mutex);
	return m_eState == CANCELLED;
}

#endif // HAVE_PTHREAD
