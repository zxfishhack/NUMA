#ifndef _FIXED_SIZE_POOL_H_
#define _FIXED_SIZE_POOL_H_

#include <cstdlib>
#include "noncopyable.h"
#include <new>
#include <list>
#include <mutex>
#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#else
#include <numa.h>
#endif

#define MEM_ALIGN(size, boundary) \
    (((size) + ((boundary) - 1)) & ~((boundary) - 1))

// ���ڴ�ز��ᶯ̬�����ռ�
// boundary��ȡ��ֵΪ8 16 32
template<
	 unsigned int poolSize = 128*1024*1024 // �ڴ�ص�Ԥ�����С
	,unsigned int minBlob  = 16            // ��С�ɸ��õ������С
	,unsigned int boundary = 16            // �ڴ����Ķ���ֵ
>
class FixedSizePool : public noncopyable {
	// WARNING
	// overheadSize��Ҫ����Ϊÿ���ڵ��������Ϣ��MemNode��С�Լ���������
	// Ŀǰ��������Ϊ32�ֽڶ��룬MemNode�ڲ�Ϊ4��ָ�룬����64λϵͳ�����
	// overheadSize����Ϊ32
	static const int overheadSize = 32;
	struct MemNode {
		MemNode * prev;
		MemNode * next;
		char * start;
		char * end;
		size_t size() const {
			return end - start;
		}
	};
public:
	FixedSizePool(int NUMANode = 0)
		:m_allocBlock(NULL)
		,m_size(poolSize)
		,m_NUMANode(NUMANode)
	{
		m_freeList = newBlock(NULL);
		if (m_freeList == NULL) {
			throw std::bad_alloc();
		}
	}
	~FixedSizePool() {
#ifdef _WIN32
		::VirtualFree(m_allocBlock, 0, MEM_RELEASE);
#else
		::numa_free(m_allocBlock, m_size);
#endif
	}
	void* alloc(size_t c) {
		size_t size = MEM_ALIGN(c, boundary);
		MemNode* cur = m_freeList;
		m_reqSize += c;
		while(cur && size > cur->size()) {
			cur = cur->next;
		}
		if (!cur) {
			return NULL;
		}
		// С���趨����С������򲻶�������
		if (cur->size() - size < minBlob + overheadSize) {
			if (cur->next) {
				cur->next->prev = cur->prev;
			}
			if (cur->prev) {
				cur->prev->next = cur->next;
			} else {
				m_freeList = cur->next;
			}
			m_allocedSize += cur->size();
			return cur->start;
		}
		m_allocedSize += size;
		// �����㣬��δʹ�������Ϸ����������м�¼
		MemNode * newNode = reinterpret_cast<MemNode*>(cur->start + size);
		newNode->prev = cur->prev;
		newNode->next = cur->next;
		newNode->start = cur->start + size + overheadSize;
		newNode->end = cur->end;
		cur->end = cur->start + size;
		if (cur->next) {
			cur->next->prev = newNode;
		}
		if (cur->prev) {
			cur->prev->next = newNode;
		} else {
			m_freeList = newNode;
		}
		return cur->start;
	}
	void free(void* m) {
		if (!isInPool(m)) {
			return;
		}
		MemNode * node = reinterpret_cast<MemNode*>(static_cast<char*>(m) - overheadSize);
		m_allocedSize -= node->size();
		// �ڴ��û��ʣ��ռ�
		if (m_freeList == NULL) {
			m_freeList = node;
			m_freeList->prev = m_freeList->next = NULL;
			return ;
		}
		MemNode * cur = m_freeList;
		// ���Ҵ����ս��Ӧ�ô��ڵ�λ��
		while(cur->next && cur->next < node) {
			cur = cur->next;
		}
		// �������ս���������λ�õĽ��պ���β���ʱ�����ֱ�Ӻϲ�
		if (cur->end == reinterpret_cast<char*>(node)) {
			cur->end = node->end;
		} else {
			// �������ս��Ӧ�ò�������ǰ���ǰ���Ǻ�
			if (node < cur) {
				node->prev = cur->prev;
				node->next = cur;
				cur->prev = node;
				m_freeList = node;
				cur = node;
			} else {
				if (cur->next) {
					cur->next->prev = node;
				}
				node->next = cur->next;
				cur->next = node;
				node->prev = cur;
				cur = cur->next;
			}
		}
		// ������Ľ�����һ������Ƿ���β��ӣ�����ӣ���ϲ�
		auto next = cur->next;
		if (cur->end == reinterpret_cast<char*>(next)) {
			cur->end = next->end;
			cur->next = next->next;
			if (next->next) {
				next->next->prev = cur;
			}
		}
	}
	inline bool isInPool(void *m) const {
		if (!m) {
			return false;
		}
		return m >= static_cast<char *>(m_allocBlock) && 
			m < static_cast<char *>(m_allocBlock) + m_size;
	}
private:
#ifdef WIN32
	MemNode* newBlock(MemNode* prev) {
		// Reserve the virtual memory.
		auto lpMemReserved = VirtualAllocExNuma(GetCurrentProcess(), NULL, m_size, MEM_COMMIT, PAGE_READWRITE, m_NUMANode);
		if (!lpMemReserved) {
			printf("alloc memory on node#%d failed.\n", m_NUMANode);
			lpMemReserved = VirtualAllocEx(GetCurrentProcess(), NULL, m_size, MEM_COMMIT, PAGE_READWRITE);
		}

		MemNode * node = static_cast<MemNode*>(lpMemReserved);
		if (!node) {
			return NULL;
		}
		m_allocBlock = node;
		node->next = NULL;
		node->prev = prev;
		node->start = reinterpret_cast<char*>(node) + overheadSize;
		node->end = reinterpret_cast<char*>(node) + m_size;
		return node;
	}
#else
	MemNode* newBlock(MemNode* prev) {
		auto mem = numa_alloc_onnode(m_size, m_NUMANode);
		if (!mem) {
			mem = numa_alloc_interleaved(m_size);
		}
		MemNode * node = static_cast<MemNode*>(::malloc(m_size));
		if (!node) {
			return NULL;
		}
		m_allocBlock = node;
		node->next = NULL;
		node->prev = prev;
		node->start = reinterpret_cast<char*>(node) + overheadSize;
		node->end = reinterpret_cast<char*>(node) + m_size;
		return node;
	}
#endif
	MemNode *m_freeList;
	void * m_allocBlock;
	size_t m_size;
	int m_NUMANode;
};

// PoolType���ṩ3���ӿ�
// void* alloc(size_t)������һ���ڴ�
// void free(void*)���ͷ�һ���ڴ�
// bool isInPool(void*)�����һ���ڴ��Ƿ�����Ӧ�ڴ����
template<class PoolType = FixedSizePool<>>
class VariableSizePool : public noncopyable {
	typedef std::list<PoolType*> PoolListType;
public:
	void* alloc(size_t c) {
		void * res;
		for(typename PoolListType::iterator i = m_poolList.begin(); i != m_poolList.end(); ++i) {
			res = (*i)->alloc(c);
			if (res) {
				return res;
			}
		}
		try {
			m_poolList.push_front(new PoolType(m_NUMANode));
			return m_poolList.front()->alloc(c);
		} catch(std::bad_alloc&) {
			return NULL;
		}
	}
	void free(void* m) {
		for(typename PoolListType::iterator i = m_poolList.begin(); i != m_poolList.end(); ++i) {
			if ((*i)->isInPool(m)) {
				return (*i)->free(m);
			}
		}
	}
	bool isInPool(void *m) {
		for(typename PoolListType::iterator i = m_poolList.begin(); i != m_poolList.end(); ++i) {
			if ((*i)->isInPool(m)) {
				return true;
			}
		}
		return false;
	}
	VariableSizePool(int NUMANode = 0) 
		: m_NUMANode(NUMANode)
	{
		m_poolList.push_back(new PoolType(m_NUMANode));
	}
	~VariableSizePool() {
		for(typename PoolListType::iterator i = m_poolList.begin(); i != m_poolList.end(); ++i) {
			delete *i;
		}
	}
private:
	PoolListType m_poolList;
	int m_NUMANode;
};


// PoolType���ṩ3���ӿ�
// void* alloc(size_t)������һ���ڴ�
// void free(void*)���ͷ�һ���ڴ�
// bool isInPool(void*)�����һ���ڴ��Ƿ�����Ӧ�ڴ����
// LockType���ṩ2���ӿ�
// Lock()������
// Unlock()������
template<
	 class PoolType = FixedSizePool<>
	,class LockType = std::mutex
>
class ThreadSafePool : public noncopyable {
public:
	ThreadSafePool(int NUMANode = 0) 
		: m_pool(NUMANode)
		, m_overhead(0)
	{}
	void* alloc(size_t c) {
		void * m;
		m_lock.lock();
		m = m_pool.alloc(c);
		m_lock.unlock();
		return m;
	}
	void free(void * m) {
		m_lock.lock();
		m_pool.free(m);
		m_lock.unlock();
	}
	bool isInPool(void *m) {
		bool res;
		m_lock.lock();
		res = m_pool.isInPool(m);
		m_lock.unlock();
		return res;
	}
private:
	PoolType m_pool;
	LockType m_lock;

	volatile LONG m_overhead;
};

#endif


