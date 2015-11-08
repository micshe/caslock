#include<stdio.h>
#include<errno.h>
#include<fcntl.h>
#include<unistd.h>

#include"caslock.h"

static int fail(int err) { errno = err; return -1; }

/***
statically initialisable stdio-based mutex locks
***/
static int ptl_init(struct ptl_lock*lock)
{
	if(lock==NULL)
		return fail(EINVAL);

	FILE*tmp;
	int fd;
	fd = open("/dev/null",O_RDONLY|O_CLOEXEC|O_NOCTTY); 
	tmp = fdopen(fd,"r");
	if(tmp==NULL)
		return -1;
	lock->flag = 0;
	lock->count = 0;
	lock->file = tmp;
	return 0;
}
static int ptl_destroy(struct ptl_lock*lock)
{
	if(lock==NULL)
		return fail(EINVAL);

	if(lock->file==stdin||lock->file==stdout||lock->file==stderr)
	{
		lock->file=NULL;
		lock->flag = 0;
		return 0;
	}

	fclose(lock->file);
	lock->file=NULL;
	lock->flag = 0;
	lock->count = 0;

	return 0;
}

static int ptl_lock(struct ptl_lock*lock)
{
	FILE*tmp;

	int err;
	if(lock==NULL)
		return fail(EINVAL);

	if(lock->flag==0)
	{
		/* lock has never been held before */
		flockfile(stderr);

		/* 
		once we have stderr locked, lock->flag won't be
		changed by any other thread.
		*/
		if(lock->flag==1)
		{
			/* we 'misread' on first check */
			funlockfile(stderr);
			/* lock->file will not be a corrupt value since flag==1 */
			flockfile(lock->file);
			/* FIXME check for overflow? */
			lock->count = lock->count + 1;
			return 0;
		}	
		else
		{
			/* we're the first to hold the lock */
			int fd;
			fd = open("/dev/null",O_RDONLY|O_CLOEXEC|O_NOCTTY);
			tmp = fdopen(fd,"r");
			if(tmp==NULL)
			{
				err = errno;
					funlockfile(stderr);
				errno = err;
				return -1;	
			}

			/* acquire new lock */
			flockfile(tmp);

			/*
			as long as these two updates
			occur within these two locks,
			their order shouldn't matter.
			*/
			lock->file = tmp;	
			lock->flag = 1;

			lock->count =  1;

			funlockfile(stderr);
			/* return holding the new lock */
			return 0;
		}	
	}

	/* lock has been held before */

	flockfile(lock->file);
	/* FIXME check for overflow? */
	lock->count = lock->count + 1;

	return 0;
}

static int ptl_trylock(struct ptl_lock*lock)
{
	int err;
	FILE*tmp;

	if(lock==NULL)
		return fail(EINVAL);

	if(lock->flag==0)
	{ 
		err = ftrylockfile(stderr);

		if(err!=0)
			return fail(EBUSY);
		if(lock->flag==1)
		{
			funlockfile(stderr);
			/* NOTE state can change before we re-lock */
			err = ftrylockfile(lock->file);
			if(err==0)
			{
				/* FIXME check for overflow? */
				lock->count = lock->count + 1;
				return 0;
			}
			else
				return fail(EBUSY);
		}

		int fd;
		fd = open("/dev/null",O_RDONLY|O_CLOEXEC|O_NOCTTY);
		tmp = fdopen(fd,"r");
		if(tmp==NULL)
		{
			err = errno;
				funlockfile(stderr);
			errno = err;
			return -1;
		}

		/* since this is a new file, it is guarenteed to succeed */
		flockfile(tmp);

		lock->file = tmp;
		lock->flag = 1;
		lock->count = 1;

		funlockfile(stderr);
		return 0;
	} 
	err = ftrylockfile(lock->file);	
	if(err!=0)
		return fail(EBUSY);

	/* FIXME check for overflow? */
	lock->count = lock->count + 1;
	return 0;
}

static int ptl_timelock(struct ptl_lock*lock, int timelimit)
{
	int err;
	int i;

	if(timelimit==-1)
		return ptl_lock(lock);

	else if(timelimit==0)
		return ptl_trylock(lock);
	else 
		for(i=0;i<timelimit;++i)
		{
			err = ptl_trylock(lock);
			if(err==0)
				return 0;
			err = errno;
				usleep(1000);
			errno = err;
		}

	return -1;
}

static int ptl_unlock(struct ptl_lock*lock)
{
	int err;
	err = ptl_trylock(lock);
	if(err!=0)
		return -1;

	if(lock->count==1)
	{
		lock->count=0;
		funlockfile(lock->file);
		errno = EBUSY;
		return -1;
	}

	/* FIXME check for underflow */
	lock->count = lock->count - 2;

	funlockfile(lock->file);
	funlockfile(lock->file);

	return 0;	
}

/***
public api
***/

int mkcaslock(struct caslock*lock,void*buffer,size_t length)
{
	if(lock==NULL)
		return fail(EINVAL);

	int err;
	err = ptl_init(&lock->lock);
	if(err==-1)
		return -1;

	lock->buffer = buffer;
	lock->length = length;

	return 0;
}

int rmcaslock(struct caslock*lock)
{
	if(lock==NULL)
		return fail(EINVAL);

	ptl_destroy(&lock->lock);
	lock->buffer = NULL;
	lock->length = 0;

	return 0;
}

int caslock(struct caslock*lock, void*iff,void*then,void*err, int timelimit)
{
	unsigned char*iff_buf;
	unsigned char*then_buf;
	unsigned char*err_buf;

	iff_buf = iff;
	then_buf = then;
	err_buf = err;
	
	if(lock==NULL)
		return fail(EINVAL);

	int error;
	error = ptl_timelock(&lock->lock,timelimit);
	if(error==-1)
		return -1;

	size_t i;
	if(err!=NULL)
		for(i=0;i<lock->length;++i)
			err_buf[i] = lock->buffer[i];

	if(iff!=NULL)
		for(i=0;i<lock->length;++i)
			if(iff_buf[i]!=lock->buffer[i])
				goto fail;

	if(then!=NULL)
		for(i=0;i<lock->length;++i)
			lock->buffer[i] = then_buf[i];

	ptl_unlock(&lock->lock);
	return 0;
fail:
	ptl_unlock(&lock->lock);
	errno = EACCES; /* permisison denied */
	return -1;
}

int chcaslock(struct caslock*lock,void*iff,void*then,void*err) 
{ 
	return caslock(lock,iff,then,err,-1); 
}
int lscaslock(struct caslock*lock,void*val) 
{ 
	return caslock(lock,NULL,NULL,val,-1); 
}

