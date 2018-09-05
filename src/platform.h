#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <stdio.h> // printf
#include <stdlib.h> // exit

#ifndef NULL
#define NULL 0
#endif

//typedef char int8_t;
//typedef int int32_t;

typedef int fd_type_t;

#define FATAL_ERROR(...) \
do{ \
	fprintf(stderr, __VA_ARGS__); \
	exit(-1); \
}while(0) \

/*
 * 描述符最大数目
 * - ev : 定义事件循环系统可处理的最大fd数目.
 */
#define MAX_FD_NUMS 32

#endif

