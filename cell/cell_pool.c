#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "cell_pool.h"
#include "cell_log.h"
#include "cell_lock.h"

#define POOL_SIZE  64


cell_pool_t* pool_create(const char* name, size_t ob_size, ob_constructor_t constructor, ob_destructor_t destructor, 
							ob_check_t check, ob_reset_t reset)
{
	cell_pool_t* pool = (cell_pool_t *)calloc(1, sizeof(cell_pool_t));
	char* nm = strdup(name);
	void** ptr = (void **)calloc(1, sizeof(void*) * POOL_SIZE);
	if(ptr == NULL || nm == NULL || pool == NULL){
		free(ptr);
		free(nm);
		free(pool);

		return NULL;
	}

	pool->mutex = 0;
	pool->name = nm;
	pool->ptr = ptr;
	pool->array_size = POOL_SIZE;
	pool->ob_size = ob_size;

	pool->constructor = constructor;
	pool->destructor = destructor;
	pool->check = check;
	pool->reset = reset;

	return pool;
}

void pool_destroy(cell_pool_t* pool)
{
	/*释放掉所有pool中缓冲的对象*/
	while(pool->curr > 0){
		void* ptr = pool->ptr[--pool->curr];
		if(pool->destructor)
			pool->destructor(ptr);

		free(ptr);
	}

	free(pool->name);
	free(pool->ptr);
	free(pool);
}

void* pool_alloc(cell_pool_t* pool)
{
	void* ret;

	LOCK(&(pool->mutex));

	if(pool->curr > 0){
		ret = pool->ptr[--pool->curr];
		UNLOCK(&(pool->mutex));
	}
	else{
		UNLOCK(&(pool->mutex));

		ret = malloc(pool->ob_size);
		if(ret != NULL && pool->constructor != NULL && pool->constructor(ret) != 0){
			free(ret);
			ret = NULL;
		}
	}

	/*对object进行分配复位*/
	if(ret != NULL && pool->reset != NULL)
		pool->reset(ret, 1);

	return ret;
}

void pool_free(cell_pool_t* pool, void* ob)
{
	if(ob == NULL || (pool->check != NULL && pool->check(ob) != 0)){
		/*log_warn("repeat alloc object, ob = %x", ob);*/
		return ;
	}

	/*对object进行回收复位*/
	if(pool->reset != NULL){
		pool->reset(ob, 0);
	}
	
	LOCK(&(pool->mutex));

	if(pool->curr < pool->array_size){
		pool->ptr[pool->curr ++] = ob;
		UNLOCK(&(pool->mutex));
	}
	else{ /*缓冲数组不够，进行扩充*/
		size_t new_size = 2 * pool->array_size;
		void** new_ptr = (void**)realloc(pool->ptr, sizeof(void *) * new_size);
		if(new_ptr != NULL){
			pool->array_size = new_size;
			pool->ptr = new_ptr;
			pool->ptr[pool->curr ++] = ob;

			UNLOCK(&(pool->mutex));
		}
		else{ /*无法扩大缓冲数组,直接释放掉object*/
			UNLOCK(&(pool->mutex));

			if(pool->destructor != NULL)
				pool->destructor(ob);

			free(ob);
		}
	}
}

void pool_print(cell_pool_t* pool)
{
	if(pool == NULL){
		printf("cell pool's ptr = NULL!");
		return;
	}
	printf("cell pool:\n");
	printf("\t name = %s\n\tob size = %d\n\tarray size = %d\n\tcurr = %d\n", 
		pool->name, pool->ob_size, pool->array_size, pool->curr);
}

int32_t get_pool_info(cell_pool_t* pool, char* buf)
{
	int32_t pos = 0;
	assert(pool != NULL);

	LOCK(&(pool->mutex));
	
	pos = sprintf(buf, "%s pool:\n", pool->name);
	pos += sprintf(buf + pos, "\tob size = %d\n\tarray size = %d\n\tcurr = %d\n", pool->ob_size, pool->array_size, pool->curr);

	UNLOCK(&(pool->mutex));

	return pos;
}





