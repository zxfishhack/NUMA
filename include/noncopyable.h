#ifndef _NON_COPYABLE_H_
#define _NON_COPYABLE_H_

class noncopyable {
protected:
	noncopyable(){}
	~noncopyable(){}
private:
	// �������������븳ֵ����
	noncopyable(const noncopyable&);
	void operator=(const noncopyable&);
};

#endif