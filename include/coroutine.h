#ifndef _COROUTINE_H_
#define _COROUTINE_H_

#ifdef _WIN32
#include <Windows.h>
#else
#include <ucontext.h>
#endif
#include "localstorage.h"

class coroutine_schedule;
class coroutine;
typedef void(*coroutine_func_t)(void* ud);

class coroutine {
public:
	enum status_t {
		DEAD = 0,
		READY = 1,
		RUNNING = 2,
		WAITING = 3,
		SUSPEND = 4
	};
	coroutine(coroutine_func_t func, void* ud);
	~coroutine();
	void resume(coroutine_schedule* schedule);
	void reset(coroutine_func_t func, void* ud);
	void setWaiting() {
		m_status = WAITING;
	}
	status_t status() const {
		return m_status;
	}

	void yield();
private:
	coroutine_schedule* m_schedule;
	volatile coroutine_func_t m_func;
	void * m_ud;
	status_t m_status;
	bool m_Exit;
#ifdef _WIN32
	HANDLE m_fiber;
	int m_initTime;

	static void WINAPI s_fiber_routine(LPVOID p) {
		reinterpret_cast<coroutine*>(p)->fiber_routine();
	}
	void fiber_routine();
#else
	ucontext_t m_ctx;
	char *stack;
#endif
	friend class coroutine_schedule;
};

class coroutine_schedule {
public:
	static const int STACK_SIZE = 1024 * 1024;
	coroutine_schedule();
	~coroutine_schedule();
	coroutine* running() const {
		return m_running;
	}
	void yield(coroutine* co) const;
	void resume(coroutine* co);
private:
	coroutine* m_running;
#ifdef _WIN32
	HANDLE m_fiber;
#else
	ucontext_t main;
	char stack[STACK_SIZE];
#endif
};


#endif
