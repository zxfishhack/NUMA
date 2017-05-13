#ifndef _TASK_POOL_H_
#define _TASK_POOL_H_
#include "mempool.h"
#include "coroutine.h"
#include <deque>
#include <map>
#include <cassert>
#include <queue>
#include <ostream>
#include <iostream>
#include "thread.h"
#include "sync.h"
#include <atomic>

namespace Task {
	typedef std::deque<coroutine*> coroutineListType;

class Pool;

extern ThreadLocal<Pool*> curPool;
extern ThreadLocal<coroutine_schedule*> curSchedule;
extern ThreadLocal<size_t> curThreadId;

class Pool {
public:
	typedef void(*thread_init_t)(void*, int);
	typedef lock_guard<sys::Mutex> scoped_lock;
	Pool(int maxThread = 4, KAFFINITY affinityMask = 0xf, thread_init_t init_func = 0, void * ctx = 0)
		: m_Exit(false)
		, m_threadCount(maxThread)
		, m_index(0)
		, m_curIdx(0)
		, m_threadInit(init_func)
		, m_ctx(ctx)
	{
		assert(maxThread > 0);
		m_tasks.resize(maxThread);
		int CPUIdx = 0;
		for(int i=0; i<maxThread; i++) {
			m_lock.push_back(new sys::Mutex);
			m_sem.push_back(new sys::Semaphore);
		}
		for(int i=0; i<maxThread; i++) {
			KAFFINITY mask = 1;
			while (((mask << (CPUIdx % 64)) & affinityMask) == 0) {
				CPUIdx++;
			}
			mask <<= CPUIdx;
			CPUIdx++;
			m_threads.push_back(new Thread(s_routine, this, coroutine_schedule::STACK_SIZE, mask));
		}
	}
	~Pool() {
		join();
		for(size_t i=0; i<m_threads.size(); i++) {
			delete m_threads[i];
		}
	}
	static void checkSemp(coroutineListType& list, sys::Semaphore& sem) {
		if (list.empty()) {
			sem.up();
		}
	}
	bool addTask(coroutine_func_t func, void * ud, int targetIdx = -1) {
		coroutine* task = getCoroutine(func, ud);
		try {
			unsigned int idx;
			if (targetIdx != -1)
				idx = targetIdx;
			else
				idx = m_curIdx.fetch_add(1);
			scoped_lock lock(*m_lock[idx % m_threadCount]);
			checkSemp(m_tasks[idx % m_threadCount], *m_sem[idx % m_threadCount]);
			m_tasks[idx % m_threadCount].push_back(task);
			return true;
		} catch(...) {
			return false;
		}
	}
	bool addTask(coroutine* co, int targetIdx = -1) {
		try {
			unsigned int idx;
			if (targetIdx != -1)
				idx = targetIdx;
			else
				idx = m_curIdx.fetch_add(1);
			scoped_lock lock(*m_lock[idx % m_threadCount]);
			checkSemp(m_tasks[idx % m_threadCount], *m_sem[idx % m_threadCount]);
			m_tasks[idx % m_threadCount].push_back(co);
			return true;
		} catch(...) {
			return false;
		}
	}
	bool addImmediatelyTask(coroutine_func_t func, void * ud, int targetIdx = -1) {
		coroutine* task = getCoroutine(func, ud);
		try {
			unsigned int idx;
			if (targetIdx != -1)
				idx = targetIdx;
			else
				idx = m_curIdx.fetch_add(1);
			scoped_lock lock(*m_lock[idx % m_threadCount]);
			checkSemp(m_tasks[idx % m_threadCount], *m_sem[idx % m_threadCount]);
			m_tasks[idx % m_threadCount].push_front(task);
			return true;
		} catch(...) {
			return false;
		}
	}
	bool addImmediatelyTask(coroutine* co, int targetIdx = -1) {
		try {
			unsigned int idx;
			if (targetIdx != -1)
				idx = targetIdx;
			else
				idx = m_curIdx.fetch_add(1);
			scoped_lock lock(*m_lock[idx % m_threadCount]);
			checkSemp(m_tasks[idx % m_threadCount], *m_sem[idx % m_threadCount]);
			m_tasks[idx % m_threadCount].push_front(co);
			return true;
		} catch(...) {
			return false;
		}
	}
	void join() {
		m_Exit = true;
		for(int i=0; i<m_threadCount; i++) {
			m_sem[i]->up(m_threadCount);
		}
		for(int i=0; i<m_threadCount; i++) {
			m_threads[i]->join();
		}
	}
	static coroutine* getRunningTask() {
		return curSchedule.get()->running();
	}
private:
	std::vector<sys::Mutex*> m_lock;
	std::vector<coroutineListType> m_tasks;
	std::vector<sys::Semaphore*> m_sem;
	coroutineListType m_freeRoutines;
	sys::Mutex m_freeLock;
	bool m_Exit;
	int m_threadCount;
	std::vector<Thread*> m_threads;
	std::atomic<int> m_index;
	std::atomic<unsigned int> m_curIdx;
	thread_init_t m_threadInit;
	void * m_ctx;

	void routine() {
		auto idx = m_index.fetch_add(1) + 1;
		curThreadId.set(idx);
		if (m_threadInit) {
			m_threadInit(m_ctx, idx);
		}
		coroutine_schedule cs;
		curSchedule.set(&cs);
		idx--;
		while(!m_Exit) {
			coroutine* task = NULL;
			// 从当前任务队列中找出
			{
				scoped_lock _(*m_lock[idx]);
				if (!m_tasks[idx].empty()) {
					task = m_tasks[idx].front();
					m_tasks[idx].pop_front();
				}
			}
			for(int i=0; i < m_threadCount && task == NULL; i++) {
				scoped_lock _(*m_lock[i]);
				if (!m_tasks[i].empty()) {
					task = m_tasks[i].front();
					m_tasks[i].pop_front();
					break;
				}
			}
			if (!task) {
				m_sem[idx]->down();
			} else {
				cs.resume(task);
				switch(task->status()) {
				case coroutine::DEAD:
					delete task;
					break;
				case coroutine::WAITING:
					break;
				case coroutine::READY:
					{
						scoped_lock _(m_freeLock);
						m_freeRoutines.push_back(task);
					}
					break;
				default:
					{
						scoped_lock _(*m_lock[idx]);
						m_tasks[idx].push_back(task);
					}
					break;
				}
			}
		}
	}
	static void s_routine(void *p) {
		reinterpret_cast<Pool*>(p)->routine();
	}
	coroutine* getCoroutine(coroutine_func_t func, void * ud) {
		coroutine* co = NULL;
		{
			scoped_lock _(m_freeLock);
			if (!m_freeRoutines.empty()) {
				co = m_freeRoutines.front();
				m_freeRoutines.pop_front();
				co->reset(func, ud);
				return co;
			}
		}
		return new coroutine(func, ud);
	}
};

}

#endif
