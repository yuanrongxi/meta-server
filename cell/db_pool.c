#include "db_pool.h"
#include "cell_log.h"
#include "cell_lock.h"

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

db_pool_t* create_db_pool(int size, const char* host, uint32_t port, const char* user, const char* pwd, const char* db_name)
{
	int i;

	db_pool_t* pool = calloc(1, sizeof(db_pool_t));
	if(pool == NULL)
		return NULL;

	pool->helpers = calloc(1, sizeof(db_helper_t*) * size);
	if(pool->helpers == NULL){
		free(pool);
		return NULL;
	}

	pool->heplers_size = size;

	for(i = 0; i < size; i ++){
		pool->helpers[i] = create_mysql(host, port, user, pwd, db_name);
		if(pool->helpers[i] == NULL){
			destroy_db_pool(pool);
			return NULL;
		}
	}

	log_info("create db pool sucess!");

	return pool;
}

void destroy_db_pool(db_pool_t* pool)
{
	int i;
	assert(pool);

	mysql_thread_end();

	for(i = 0; i < pool->heplers_size; i ++){
		if(pool->helpers[i] != NULL){
			destroy_mysql(pool->helpers[i]);
			pool->helpers[i] = NULL;
		}
	}
	
	if(pool->helpers != NULL){
		free(pool->helpers);
		pool->helpers = NULL;
	}

	free(pool);

	log_info("destroy db pool !!");
}

db_helper_t* get_helper(db_pool_t* pool)
{
	int i;
	db_helper_t* ret = NULL;

	assert(pool);

	LOCK(&(pool->mutex));
	for(i = 0; i < pool->heplers_size; i ++){
		if(pool->helpers[i]->busy == 0){
			ret = pool->helpers[i];
			pool->helpers[i]->busy = 1;

			break;
		}
	}
	UNLOCK(&(pool->mutex));

	return ret;
}

void release_helper(db_pool_t* pool, db_helper_t* helper)
{
	int i;
	assert(pool && helper);

	LOCK(&(pool->mutex));
	for(i = 0; i < pool->heplers_size; i ++){
		if(pool->helpers[i] == helper){
			helper->busy = 0;
			break;
		}
	}
	UNLOCK(&(pool->mutex));
}

	