#include "NUMAExecutorGroup.h"


NUMAExecutorGroup::NUMAExecutorGroup(int NUMANode, KAFFINITY affinity)
	: m_NUMANode(NUMANode)
	, m_affinity(affinity)
{
	int cnt = 0;
	KAFFINITY _1 = 1;
	for(int i=0; i<64; i++) {
		if (affinity & (_1<<i)) {
			cnt ++;
		}
	}
	m_thrCount = cnt;
	m_memPool = new memPoolType(NUMANode);
	m_taskPool = new Task::Pool(cnt, affinity, s_thread_init, this);
}

NUMAExecutorGroup::~NUMAExecutorGroup(void)
{
	Stop();
	delete m_taskPool;
	delete m_memPool;
}

void NUMAExecutorGroup::s_thread_init(void *ctx, int) {
	NUMAExecutorGroup* self = reinterpret_cast<NUMAExecutorGroup *>(ctx);
	curExecutorGroup.set(self);
}


ThreadLocal<NUMAExecutorGroup*> curExecutorGroup;
