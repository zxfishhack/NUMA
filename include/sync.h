#ifndef _NUMA_SYNC_H_
#define _NUMA_SYNC_H_
#include <mutex>
#include "coroutine.h"
#include <deque>
#ifdef _WIN32
#include <Windows.h>
#else
#include <pthread.h>
#include <semaphore.h>
#endif

namespace Task {

	namespace sys {
		class Mutex : public noncopyable {
		public:
			Mutex();
			~Mutex();
			void lock();
			void unlock();
		private:
#ifdef _WIN32
			HANDLE m_mutex;
#else
			pthread_mutex_t m_mutex;
#endif
		};
#ifdef _WIN32
		inline Mutex::Mutex() {
			m_mutex = ::CreateMutexA(NULL, FALSE, NULL);
		}
		inline Mutex::~Mutex() {
			::CloseHandle(m_mutex);
		}
		inline void Mutex::lock() {
			::WaitForSingleObject(m_mutex, INFINITE);
		}
		inline void Mutex::unlock() {
			::ReleaseMutex(m_mutex);
		}

#else
		inline Mutex::Mutex() {
			pthread_mutexattr_t attr;
			pthread_mutexattr_init(&attr);
			pthread_mutex_init(&m_mutex, &attr);
		}
		inline Mutex::~Mutex() {
			pthread_mutex_destroy(&m_mutex);
		}
		inline void Mutex::lock() {
			pthread_mutex_lock(&m_mutex);
		}
		inline void Mutex::unlock() {
			pthread_mutex_unlock(&m_mutex);
		}
#endif
		class Semaphore : public noncopyable {
		public:
			Semaphore(int initval = 0);
			void up(int val = 1);
			void down();
		private:
#ifdef _WIN32
			HANDLE m_semaphore;
#else
			sem_t m_semaphore;
#endif
		};

#ifdef _WIN32
		inline Semaphore::Semaphore(int initval) {
			m_semaphore = ::CreateSemaphoreA(NULL, initval, 0x7fffffff, NULL);
		}
		inline void Semaphore::up(int val) {
			::ReleaseSemaphore(m_semaphore, val, NULL);
		}
		inline void Semaphore::down() {
			::WaitForSingleObject(m_semaphore, INFINITE);
		}
#else
		inline Semaphore::Semaphore(int initval) {
			sem_init(&m_semaphore, 0, initval);
		}
		inline void Semaphore::up(int val) {
			for (int i = 0; i<val; i++) {
				sem_post(&m_semaphore);
			}
		}
		inline void Semaphore::down() {
			sem_wait(&m_semaphore);
		}
#endif
	}
	template<class lock>
	class lock_guard : public noncopyable {
	public:
		lock_guard(lock& mtx) : m_mtx(mtx) {
			m_mtx.lock();
		}
		~lock_guard() {
			m_mtx.unlock();
		}
	private:
		lock& m_mtx;
	};

	typedef std::deque<coroutine*> coroutineListType;
	class Semaphore : public noncopyable {
	public:
		Semaphore(int initVal = 0);
		void down(int count);
		void up();
	private:
		sys::Mutex m_lock;
		volatile int m_cnt;
		struct waitItem {
			int need;
			coroutine* co;
		};
		std::deque<waitItem> m_waitQueue;
	};

	class Event : public noncopyable {
	public:
		Event(bool isTrigger = false);
		void signal();
		void wait();
	private:
		bool m_status;
		sys::Mutex m_lock;
		coroutineListType m_waitQueue;
	};

	class Barrier : public noncopyable {
	public:
		Barrier(int waitCount = 1);
		void sync();
	private:
		int m_cnt;
		int m_trigger;
		sys::Mutex m_lock;
		coroutineListType m_waitQueue;
	};
}

#endif
