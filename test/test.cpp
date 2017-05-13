
#include "NUMAExecutorGroup.h"
#include <string>
#include <vector>

struct CalcContext {
	int start;
	int end;
	double result;
	Task::Semaphore* sem;
};

void calc(void * ctx) {
	CalcContext * cc(reinterpret_cast<CalcContext *>(ctx));
	cc->result = 0;
	for(int i=cc->start; i<cc->end; ++i) {
		for (int j = cc->start; j <= i; j++) {
			cc->result += j;
		}
	}
	cc->sem->up();
}

struct CalcTask {
	Task::sys::Semaphore* succ;
};

template<class Ty>
struct memblock {
public:
	memblock(size_t c) {
		auto& memPool = *curExecutorGroup.get()->memPool();
		_ptr = (Ty*)memPool.alloc(sizeof(Ty) * c);
	}
	~memblock() {
		auto& memPool = *curExecutorGroup.get()->memPool();
		memPool.free(_ptr);
	}
	Ty* get() const {
		return _ptr;
	}
private:
	Ty * _ptr;
};

void calc_task(void* ctx) {
	auto& taskPool = curExecutorGroup.get()->taskPool();
	const int taskNum = 100;
	memblock<CalcContext> ccs(taskNum);
	CalcContext *cp = ccs.get();
	Task::Semaphore sem;
	for(int i=0; i<100; i++) {
		CalcContext* cc = &cp[i];
		cc->start = i * 2000;
		cc->end = (i + 1) * 2000;
		cc->result = 0;
		cc->sem = &sem;
		taskPool.addTask(calc, cc);
	}
	sem.down(taskNum);
	reinterpret_cast<CalcTask*>(ctx)->succ->up();
}

void test_routine(void *ctx) {
	NUMAExecutorGroup* eg(reinterpret_cast<NUMAExecutorGroup *>(ctx));
	auto& taskPool = eg->taskPool();
	auto& memPool = *eg->memPool();
	Task::sys::Semaphore sem;
	for(int i=1; i<=100; i++) {
		CalcTask* ct = (CalcTask*)memPool.alloc(sizeof(CalcTask));
		ct->succ = &sem;
		taskPool.addTask(calc_task, ct);
	}
	for(int i=0; i<100; i++) {
		sem.down();
		std::cout << "job done count " << i + 1 << "\r";
	}
	std::cout << std::endl;
	std::cout << "all job done." << std::endl;
}

int main() {
	NUMAExecutorGroup eg(0, 0xf & 0x55555);
	eg.Run(test_routine, &eg);

	eg.Stop();
	return 0;
}
