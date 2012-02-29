// erlv8.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "erlv8.h"


// 这是导出变量的一个示例
ERLV8_API int nerlv8=0;

// 这是导出函数的一个示例。
ERLV8_API int fnerlv8(void)
{
	return 42;
}

// 这是已导出类的构造函数。
// 有关类定义的信息，请参阅 erlv8.h
Cerlv8::Cerlv8()
{
	return;
}
