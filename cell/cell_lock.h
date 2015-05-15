#ifndef __CELL_LOCK_H_
#define __CELL_LOCK_H_

#include <pthread.h>
#include <stdlib.h>

#define barrier() __asm__ __volatile__("mfence" : : : "memory")
#define cpu_pause() __asm__ __volatile__("pause")

/*定义一个简单的spin lock*/
static inline void LOCK(int* q)
{
	int i;
	while(__sync_lock_test_and_set(q, 1)){
		for(i = 0; i < 32; i ++){
			cpu_pause();
		}
		pthread_yield();
	}
};

#define UNLOCK(q) __sync_lock_release((q))

#define SYNC_ADD(q) __sync_add_and_fetch((q), 1)
#define SYNC_SUB(q) __sync_sub_and_fetch((q), 1)

/*定义rwlock*/
typedef struct rwlock_t
{
	int		write;
	int		read;
}rwlock_t;

static inline void rwlock_init(rwlock_t* lock)
{
	lock->write = 0;
	lock->read = 0;
}

static inline void rwlock_rlock(rwlock_t* lock)
{
	for(;;){
		while(lock->write)
			__sync_synchronize();

		__sync_add_and_fetch(&(lock->read), 1);
		if(lock->write) /*没有获得读锁权，读计数 -1*/
			__sync_sub_and_fetch(&(lock->read), 1);
		else
			break;
	}
}

static inline void rwlock_wlock(rwlock_t* lock)
{
	/*先获得写锁控制权*/
	while(__sync_lock_test_and_set(&lock->write,1)) {};
	/*再控制读锁变量,确定读空闲*/
	while(lock->read)
		__sync_synchronize();
}

static inline void rwlock_runlock(rwlock_t* lock)
{
	__sync_sub_and_fetch(&(lock->read), 1);
}

static inline void rwlock_wunlock(rwlock_t* lock)
{
	__sync_lock_release(&(lock->write));
}

#endif







