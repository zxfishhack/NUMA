#pragma once
#include "mempool.h"
#include "taskpool.h"

typedef ThreadSafePool<VariableSizePool<FixedSizePool<256*1024*1024>>, Task::sys::Mutex> memPoolType;

class NUMAExecutorGroup
{
public:
	typedef void (*func_t)(void*);
	NUMAExecutorGroup(int NUMANode, KAFFINITY affinity);
	~NUMAExecutorGroup(void);
	void Run(func_t func, void* ud) {
		m_thread = new Task::Thread(func, ud, 0, m_affinity);
	}
	void Stop() {
		if (m_thread) {
			m_thread->join();
			delete m_thread;
			m_thread = NULL;
		}
	}
	Task::Pool& taskPool() const {
		return *m_taskPool;
	}

	static memPoolType* memPool() {
		return curMemPool.get();
	}
	int m_thrCount;
	int m_NUMANode;
private:
	KAFFINITY m_affinity;
	Task::Thread *m_thread;
	memPoolType * m_memPool;
	Task::Pool *m_taskPool;

	static ThreadLocal<memPoolType*> curMemPool;

	static void s_thread_init(void * ctx, int);
};

