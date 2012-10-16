#ifndef _TFC_SPIN_LOCK_H_
#define _TFC_SPIN_LOCK_H_

#define LOCK "lock ; "

#define __raw_spin_lock_string \
        "\n1:\t" \
        LOCK \
        "decb %0\n\t" \
        "jns 3f\n" \
        "2:\t" \
        "rep;nop\n\t" \
        "cmpb $0,%0\n\t" \
        "jle 2b\n\t" \
        "jmp 1b\n" \
        "3:\n\t"

typedef struct {
    volatile unsigned int slock;
} raw_spinlock_t;

typedef struct {
    raw_spinlock_t raw_lock;
} spinlock_t;

static inline void __raw_spin_lock(raw_spinlock_t *lock)
{
    __asm__ __volatile__(
        __raw_spin_lock_string
        :"+m" (lock->slock) : : "memory"
    );
}

#define __raw_spin_unlock_string \
        "movb $1,%0" \
        :"+m" (lock->slock) : : "memory"

static inline void __raw_spin_unlock(raw_spinlock_t *lock)
{
    __asm__ __volatile__(
        __raw_spin_unlock_string
    );
}

#define spin_lock_init(lock)    do {(lock)->raw_lock.slock = 1; } while (0)
#define spin_lock(lock) __raw_spin_lock(&(lock)->raw_lock)
#define spin_unlock(lock)  __raw_spin_unlock(&(lock)->raw_lock)

//----------------------------------------------------------------------
#include <sys/shm.h>

inline spinlock_t* shm_spin_lock_init(key_t iShmKey,int iNumber=1)
{
	int iFirstCreate = 1;
	int iShmID = shmget(iShmKey,sizeof(spinlock_t)*iNumber,IPC_CREAT|IPC_EXCL|0666);
	if(iShmID < 0)
	{
		iFirstCreate = 0;
		iShmID = shmget(iShmKey,sizeof(spinlock_t)*iNumber,0666);
	}

	if(iShmID < 0)
	{
		iShmID = shmget(iShmKey,0,0666);
		if( iShmID < 0)
			return NULL;

		struct shmid_ds s_ds;
		shmctl(iShmID,IPC_STAT,&s_ds);
		if(s_ds.shm_nattch > 0)
		{
			return NULL;
		}

		if(shmctl(iShmID,IPC_RMID,NULL))
			return NULL;

		iShmID = shmget(iShmKey,sizeof(spinlock_t)*iNumber,IPC_CREAT|IPC_EXCL|0666);
		if( iShmID < 0)
			return NULL;

		iFirstCreate = 1;
	}

	spinlock_t* g_shm_spinlock_t = (spinlock_t*)shmat(iShmID,0,0);
	if((char*)g_shm_spinlock_t == (char*)-1)
		return 0;

	struct shmid_ds s_ds;
	shmctl(iShmID,IPC_STAT,&s_ds);
	int iAttach = s_ds.shm_nattch;

	if((iAttach==1) || iFirstCreate)
	{
		for(int i=0; i<iNumber; i++)
		{
			spin_lock_init(g_shm_spinlock_t+i);
		}
	}

	return g_shm_spinlock_t;
}

#endif

/*
加解锁1000w次： 无-O2
pthread_spin_lock  cost 224ms
spin lock cost 307ms
pthread_mutex  cost 558ms
sem  cost 4563ms


加解锁1000w次： 有-O2
spin lock cost 107ms
pthread_spin_lock  cost 200ms
pthread_mutex  cost 470ms
sem  cost 4449ms
*/

#if 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include "Sem.hpp"

int main()
{
	spinlock_t lock;
	spin_lock_init(&lock);

	timeval t1,t2;
	gettimeofday(&t1,NULL);
	for(int i=0; i<10000000;i++)
	{
		spin_lock(&lock);
		spin_unlock(&lock);
	}
	gettimeofday(&t2,NULL);
	int cost = (t2.tv_sec-t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/1000;
	printf("spin lock cost %dms\n",cost);

//
	pthread_mutex_t mp1 = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_init(&mp1,NULL);

	gettimeofday(&t1,NULL);
	for(int i=0; i<10000000;i++)
	{
		pthread_mutex_lock(&mp1);
		pthread_mutex_unlock(&mp1);
	}
	gettimeofday(&t2,NULL);
	cost = (t2.tv_sec-t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/1000;
	printf("pthread_mutex  cost %dms\n",cost);
//
	CSem m_stSem;
	m_stSem.Open(34343343,2);

	gettimeofday(&t1,NULL);
	for(int i=0; i<10000000;i++)
	{
		m_stSem.Wait(0);
		m_stSem.Post(0);
	}
	gettimeofday(&t2,NULL);
	cost = (t2.tv_sec-t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/1000;
	printf("sem  cost %dms\n",cost);

//
	pthread_spinlock_t   pthr_lock;
	pthread_spin_init(&pthr_lock,PTHREAD_PROCESS_SHARED); //PTHREAD_PROCESS_PRIVATE
	gettimeofday(&t1,NULL);
	for(int i=0; i<10000000;i++)
	{
		pthread_spin_lock(&pthr_lock);
		pthread_spin_unlock(&pthr_lock);
	}
	gettimeofday(&t2,NULL);
	cost = (t2.tv_sec-t1.tv_sec)*1000 + (t2.tv_usec-t1.tv_usec)/1000;
	printf("pthread_spin_lock  cost %dms\n",cost);


	return 0;
}
#endif

/*
int main()
{
	spinlock_t* lock = shm_spin_lock_init(3434343);

	spin_lock(lock);
	spin_unlock(lock);

	return 0;
}

*/

