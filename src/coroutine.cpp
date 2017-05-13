#include "coroutine.h"
#include <cassert>

#ifdef WIN32

void coroutine::resume(coroutine_schedule* schedule) {
	m_schedule->resume(this);
}

void coroutine::reset(coroutine_func_t func, void* ud) {
	m_func = func;
	m_ud = ud;
	m_status = READY;
	m_initTime ++;
}

void coroutine::yield() {
	m_schedule->yield(this);
}

void coroutine::fiber_routine() {
	while(!m_Exit) {
		m_func(m_ud);
		m_status = coroutine::READY;
		yield();
	}
	m_status = coroutine::DEAD;
	yield();
}

coroutine::coroutine(coroutine_func_t func, void* ud)
	: m_func(func)
	, m_ud(ud)
	, m_status(READY)
	, m_Exit(false)
	, m_initTime(1)
{
	// 默认4K的栈空间，最大为1M的栈空间
	m_fiber = ::CreateFiberEx(64 * 1024, coroutine_schedule::STACK_SIZE, FIBER_FLAG_FLOAT_SWITCH, 
		s_fiber_routine, this);
	assert(m_fiber != NULL);
}

coroutine_schedule::coroutine_schedule()
	: m_running(NULL){
	m_fiber = ::ConvertThreadToFiber(NULL);
}

coroutine_schedule::~coroutine_schedule() {
	::ConvertFiberToThread();
}

coroutine::~coroutine() {
	::DeleteFiber(m_fiber);
}

void coroutine_schedule::yield(coroutine* co) const {
	switch(co->m_status) {
	case coroutine::DEAD:
	case coroutine::WAITING:
	case coroutine::READY:
		break;
	default:
		co->m_status = coroutine::SUSPEND;
	}
	::SwitchToFiber(m_fiber);
}

void coroutine_schedule::resume(coroutine* co) {
	co->m_status = coroutine::RUNNING;
	co->m_schedule = this;
	m_running = co;
	::SwitchToFiber(co->m_fiber);
}

#endif

