#ifndef _NUMA_LOCAL_STORAGE_H_
#define _NUMA_LOCAL_STORAGE_H_

#include <exception>

#ifdef _WIN32

#include <Windows.h>
		
template<typename Ty, int size = sizeof(Ty)>
class ThreadLocal {
public:
	ThreadLocal() {
		m_tlsId = ::TlsAlloc();
		if (TLS_OUT_OF_INDEXES == m_tlsId) {
			throw std::bad_alloc();
		}
	}
	~ThreadLocal() {
		::TlsFree(m_tlsId);
	}
	void set(Ty p) const {
		::TlsSetValue(m_tlsId, LPVOID(p));
	}
	Ty get()  const {
		return Ty(::TlsGetValue(m_tlsId));
	}
private:
	DWORD m_tlsId;
};

#else

template<typename Ty>
class ThreadLocal {
public:
	ThreadLocal() {
		if (pthread_key_create(&m_key, NULL) != 0) {
			throw std::bad_alloc();
		}
	}
	~ThreadLocal() {
		pthread_key_delete(m_key);
	}
	void set(Ty p) const {
		pthread_setspecific(m_key, LPVOID(p));
	}
	Ty get() const {
		return Ty(pthread_getspecific(m_key));
	}
private:
	pthread_key_t m_key;
};

#endif

#endif
