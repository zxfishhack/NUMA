#include "coroutine.h"
#include <cassert>

void coroutine::resume(coroutine_schedule* schedule) {
	m_schedule->resume(this);
}

void coroutine::reset(coroutine_func_t func, void* ud) {
	m_func = func;
	m_ud = ud;
	m_status = READY;
	m_initTime++;
}

void coroutine::yield() {
	m_schedule->yield(this);
}

void coroutine::fiber_routine() {
	while (!m_Exit) {
		m_func(m_ud);
		m_status = coroutine::READY;
		yield();
	}
	m_status = coroutine::DEAD;
	yield();
}

#ifdef _WIN32

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

#else

coroutine::coroutine(coroutine_func_t func, void* ud)
	: m_func(func)
	, m_ud(ud)
	, m_Exit(false)
	, m_initTime(1)
{
	stack = new char[coroutine_schedule::STACK_SIZE];
	getcontext(&m_ctx);
	m_ctx.uc_stack.ss_sp = stack;
	m_ctx.uc_stack.ss_size = coroutine_schedule::STACK_SIZE;
	m_ctx.uc_link = NULL;
	uintptr_t ptr = (uintptr_t)this;
	makecontext(&m_ctx, (void(*)(void))coroutine::s_fiber_routine, 2, (uint32_t)ptr, (uint32_t)(ptr >> 32));
}

coroutine::~coroutine() {
	delete []stack;
}

void coroutine::s_fiber_routine(uint32_t low32, uint32_t hi32) {
	uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
	reinterpret_cast<coroutine*>(ptr)->fiber_routine();
}

coroutine_schedule::coroutine_schedule()
	: m_running(NULL) {
	stack = new char[STACK_SIZE];
}

coroutine_schedule::~coroutine_schedule() {
	delete []stack;
}

void coroutine_schedule::yield(coroutine* co) const {
	switch (co->m_status) {
	case coroutine::DEAD:
	case coroutine::WAITING:
	case coroutine::READY:
		break;
	default:
		co->m_status = coroutine::SUSPEND;
	}
	swapcontext(&co->m_ctx, &main);
}

void coroutine_schedule::resume(coroutine* co) {
	co->m_status = coroutine::RUNNING;
	co->m_schedule = this;
	m_running = co;
	
	swapcontext(&main, &co->m_ctx);
}

#endif

