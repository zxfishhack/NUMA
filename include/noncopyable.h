#ifndef _NON_COPYABLE_H_
#define _NON_COPYABLE_H_

class noncopyable {
protected:
	noncopyable(){}
	~noncopyable(){}
private:
	// 不允许拷贝构造与赋值操作
	noncopyable(const noncopyable&);
	void operator=(const noncopyable&);
};

#endif