#include "taskpool.h"

namespace Task {

Semaphore::Semaphore(int initVal) 
	: m_cnt(initVal)
{}

void Semaphore::down(int count) {
	coroutine* co = curPool.get()->getRunningTask();
	{
		lock_guard<sys::Mutex> _(m_lock);
		if (m_cnt >= count) {
			m_cnt -= count;
			return;
		}
		waitItem item;
		item.need = count;
		item.co = co;
		m_waitQueue.push_back(item);
	}
	co->setWaiting();
	co->yield();
}

void Semaphore::up() {
	coroutine * nco = 0;
	{
		lock_guard<sys::Mutex> _(m_lock);
		m_cnt ++;
		if (!m_waitQueue.empty()) {
			waitItem& front = m_waitQueue.front();
			if (m_cnt >= front.need) {
				nco = front.co;
				m_cnt -= front.need;
			}
		}
	}
	if (nco) {
		curPool.get()->addImmediatelyTask(nco);
	}
}

Event::Event(bool isTrigger) 
	: m_status(isTrigger)
{}

void Event::signal() {
	coroutine * nco = 0;
	{
		lock_guard<sys::Mutex> _(m_lock);
		if (m_waitQueue.empty()) {
			m_status = true;
		} else {
			nco = m_waitQueue.front();
			m_waitQueue.pop_front();
		}
	}
	if (nco) {
		curPool.get()->addImmediatelyTask(nco);
	}
}

void Event::wait() {
	auto co = curPool.get()->getRunningTask();
	{
		lock_guard<sys::Mutex> _(m_lock);
		if (m_status) {
			m_status = false;
			return;
		}
		m_waitQueue.push_back(co);
	}
	co->setWaiting();
	co->yield();
}

Barrier::Barrier(int waitCount) 
	: m_cnt(0)
	, m_trigger(waitCount)
{}

void Barrier::sync() {
	auto co = curPool.get()->getRunningTask();
	{
		lock_guard<sys::Mutex> _(m_lock);
		m_cnt++;
		if (m_cnt >= m_trigger) {
			m_cnt -= m_trigger;
			for(int i=1; i<m_trigger && m_waitQueue.size() > 0; i++) {
				coroutine* nco = m_waitQueue.front();
				curPool.get()->addImmediatelyTask(nco);
				m_waitQueue.pop_front();
			}
			return;
		}
		m_waitQueue.push_back(co);
	}
	co->setWaiting();
	co->yield();
}

ThreadLocal<Pool*> curPool;
ThreadLocal<coroutine_schedule*> curSchedule;
ThreadLocal<size_t> curThreadId;

}
