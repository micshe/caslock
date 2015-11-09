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
#define PTL_INIT { .file = NULL, .flag = 0, .count = 0 }

struct caslock
{
	struct ptl_lock lock;
	unsigned char*buffer;
	size_t length;
};
#define CASLOCK_INIT(x) { .lock = PTL_INIT, .buffer = (unsigned char*)x, .length = sizeof(x) }

int mkcaslock(struct caslock*lock,void*buffer,size_t length);
int rmcaslock(struct caslock*lock);
int caslock(struct caslock*lock, void*iff,void*then,void*err, int timelimit);
int chcaslock(struct caslock*lock,void*iff,void*then,void*err);
int lscaslock(struct caslock*lock,void*val);

#ifdef __cplusplus
}
#endif

#endif

