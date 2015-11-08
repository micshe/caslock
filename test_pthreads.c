#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h> /* link with -l pthread */

#include"caslock.h"

char buffer[64];
struct caslock lock = CASLOCK_INIT(buffer);

void operate(char*buffer, int i)
{
	int j;
	int k;

	j = i % 8;
	k = (i-j)/8;

	buffer[k] = buffer[k] | 0x1<<j;
} 

int validate(char*buffer, int i)
{
	int j;
	int k;

	j = i % 8;
	k = (i-j)/8;

	return buffer[k] & 0x1<<j;
}

void*thread_racey(void*arg)
{
	int*i;
	i = arg;

	char tmp[64];
	memcpy(tmp,buffer,64); 
		operate(tmp,*i); 
		usleep(25*1000);
	memcpy(buffer,tmp,64);

	return NULL;
}

void*thread_caslock(void*arg)
{
	int err;

	int*i;
	i = arg;

	char tmp[64]; 
	char iff[64];
	char then[64];

retry:
	caslock(&lock, NULL,NULL, &tmp, -1);

		memcpy(iff,tmp,64);

		memcpy(then,tmp,64);
		operate(then,*i);

		usleep(25*1000);
	err = caslock(&lock, iff, then, tmp, -1);
	if(err!=0)
		goto retry;

	return NULL;
}

int main(int argc,char*args[])
{
	int i;
	int e;

	pthread_t tid[64];
	int idx[64];
	for(i=0;i<64;++i) idx[i] = i;

	printf("test: serial shared buffer update should pass\n");
		memset(buffer,0,64);
		for(i=0;i<64;++i)
			thread_racey(&i);

		printf("buffer: ");
		for(i=0;i<64;++i)
			printf("%02X", buffer[i]);
		printf("\n");

		e=0;
		for(i=0;i<64;++i)
			if(!validate(buffer,i))
				e=-1; 
		if(e==-1)
		{
			printf("fail: serial shared buffer update failed\n");
			exit(1);
		} 
	printf("pass: serial shared buffer update passed\n");

	printf("test: serialised shared buffer update should pass\n");
		memset(buffer,0,64);
		for(i=0;i<64;++i)
		{
			pthread_create(&tid[i], NULL, &thread_racey, &idx[i]);
			pthread_join(tid[i],NULL);
		}

		printf("buffer: ");
		for(i=0;i<64;++i)
			printf("%02X", buffer[i]);
		printf("\n");

		e=0;
		for(i=0;i<64;++i)
			if(!validate(buffer,i))
				e=-1;
		if(e==-1)
		{
			printf("fail: serialised shared buffer update failed\n");
			exit(1);
		} 
	printf("pass: serialised shared buffer update passed\n");

	printf("test: racey shared buffer update should fail\n");
		memset(buffer,0,64);
#if 0
		for(i=0;i<64;++i)
#else
		for(i=63;i>=0;--i)
#endif
			pthread_create(&tid[i], NULL, &thread_racey, &idx[i]);

		for(i=0;i<64;++i)
			pthread_join(tid[i],NULL);

		printf("buffer: ");
		for(i=0;i<64;++i)
			printf("%02X", buffer[i]);
		printf("\n");

		e=0;
		for(i=0;i<64;++i)
			if(!validate(buffer,i))
				e=-1;

		if(e==0)
		{
			printf("fail: racey shared buffer update passed\n");
			exit(1);
		} 
	printf("pass: racey shared buffer update failed\n");

	printf("test: caslocked shared buffer update should pass\n");
		memset(buffer,0,64);
		for(i=0;i<64;++i)
			pthread_create(&tid[i], NULL, &thread_caslock, &idx[i]);

		for(i=0;i<64;++i)
			pthread_join(tid[i],NULL);

		printf("buffer: ");
		for(i=0;i<64;++i)
			printf("%02X", buffer[i]);
		printf("\n");

		e=0;
		for(i=0;i<64;++i)
			if(!validate(buffer,i))
				e=-1;
		if(e==-1)
		{
			printf("fail: caslocked shared buffer update failed\n");
			exit(1);
		} 
	printf("pass: caslocked shared buffer update passed\n");

	return 0;	
}

