#include "NUMAExecutorGroup.h"


NUMAExecutorGroup::NUMAExecutorGroup(int NUMANode, KAFFINITY affinity)
	: m_NUMANode(NUMANode)
	, m_affinity(affinity)
{
	int cnt = 0;
	DWORD_PTR _1 = 1;
	for(int i=0; i<64; i++) {
		if (affinity & (_1<<i)) {
			cnt ++;
		}
	}
	m_thrCount = cnt;
	m_memPool = new memPoolType(NUMANode);
	m_taskPool = new Task::Pool(cnt / 2, affinity, s_thread_init, this);
}

NUMAExecutorGroup::~NUMAExecutorGroup(void)
{
	Stop();
	delete m_taskPool;
	delete m_memPool;
}

void NUMAExecutorGroup::s_thread_init(void *ctx, int) {
	NUMAExecutorGroup* self = reinterpret_cast<NUMAExecutorGroup *>(ctx);
	curMemPool.set(self->m_memPool);
}


ThreadLocal<memPoolType*> NUMAExecutorGroup::curMemPool;