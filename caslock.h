#ifndef PTL_H
#define PTL_H

#ifdef __cplusplus
extern "C" {
#endif

struct ptl_lock
{
	FILE*file;
	unsigned char flag;
	long count;
}; 
#if 0
#define PTL_INIT { .file = stderr, .flag = 0, .count = 0 }
#else
#define PTL_INIT { .file = NULL, .flag = 0, .count = 0 }
#endif

struct caslock
{
	struct ptl_lock lock;
	unsigned char*buffer;
	size_t length;
};
#define CASLOCK_INIT(x) { .lock = PTL_INIT, .buffer = (unsigned char*)x, .length = sizeof(x) }

#ifdef __cplusplus
}
#endif

#endif

