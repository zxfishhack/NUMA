
#include "NUMAExecutorGroup.h"
#include <string>

struct CalcContext {
	int start;
	int end;
	double result;
	Task::sys::Semaphore* sem;
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

void test_routine(void *ctx) {
	NUMAExecutorGroup* eg(reinterpret_cast<NUMAExecutorGroup *>(ctx));
	auto& taskPool = eg->taskPool();
	auto& memPool = *eg->memPool();
	Task::sys::Semaphore sem;
	std::vector<CalcContext*> ccs;
	for(int i=1; i<=100; i++) {
		CalcContext* cc = (CalcContext*)memPool.alloc(sizeof(CalcContext));
		ccs.push_back(cc);
		cc->start = i * 20000;
		cc->end = (i + 1) * 20000;
		cc->result = 0;
		cc->sem = &sem;
		taskPool.addTask(calc, cc);
	}
	for(int i=0; i<100; i++) {
		sem.down();
	}
	double result = 0;
	for(auto i = ccs.begin(); i != ccs.end(); ++i) {
		result += (*i)->result;
	}
	std::cout << result << std::endl;
}

int main() {
	NUMAExecutorGroup eg(0, 0xff & 0x55555);
	eg.Run(test_routine, &eg);

	std::string line;
	std::cin >> line;
	return 0;
}
