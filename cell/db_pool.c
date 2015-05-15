#include "db_pool.h"
#include "cell_log.h"
#include "cell_lock.h"
#include "cell_config.h"

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>


db_pool_t* create_db_pool(int size)
{
	int i, j;

	db_pool_t* pool = calloc(1, sizeof(db_pool_t));
	if(pool == NULL)
		return NULL;

	pool->helpers = calloc(1, sizeof(db_helper_t*) * size);
	if(pool->helpers == NULL){
		free(pool);
		return NULL;
	}

	pool->read_helpers = calloc(1, sizeof(db_helper_t*) * size);
	if(pool->read_helpers == NULL){
		free(pool->read_helpers);
		free(pool);
		return NULL;
	}

	pool->heplers_size = size;
	/*建立一个写的DB连接池*/
	for(i = 0; i < size; i ++){
		pool->helpers[i] = create_mysql(cell_config->db_master, cell_config->db_port, cell_config->user, cell_config->passwd, cell_config->db_name, i);
		if(pool->helpers[i] == NULL){
			destroy_db_pool(pool);
			return NULL;
		}
	}

	/*建立一个读的DB连接池*/
	j = 0;
	for(i = 0; i < size; i ++){
		char* db_slave = cell_config->db_slaves[j++];
		if(db_slave == NULL){ /*轮询式加入连接池,防止单个读服务器过热*/
			j = 0;
			db_slave = cell_config->db_slaves[j++];
		}

		pool->read_helpers[i] = create_mysql(db_slave, cell_config->db_port, cell_config->user, cell_config->passwd, cell_config->db_name, i);
		if(pool->read_helpers[i] == NULL){
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

	for(i = 0; i < pool->heplers_size; i ++){
		if(pool->read_helpers[i] != NULL){
			destroy_mysql(pool->read_helpers[i]);
			pool->read_helpers[i] = NULL;
		}
	}
	
	if(pool->helpers != NULL){
		free(pool->helpers);
		pool->helpers = NULL;
	}

	if(pool->read_helpers != NULL){
		free(pool->read_helpers);
		pool->read_helpers = NULL;
	}

	free(pool);

	log_info("destroy db pool !!");
}

db_helper_t* get_helper(db_pool_t* pool)
{
	int i;
	db_helper_t* ret = NULL;

	assert(pool);

	LOCK(&(pool->wmutex));

	for(i = pool->wpos; i < pool->heplers_size; i ++){
		if(pool->helpers[i]->busy == 0){
			ret = pool->helpers[i];
			pool->helpers[i]->busy = 1;

			pool->wpos = i + 1;
			if(pool->wpos >= pool->heplers_size)
				pool->wpos = 0;
			break;
		}
	}

	UNLOCK(&(pool->wmutex));

	
	return ret;
}

void release_helper(db_pool_t* pool, db_helper_t* helper)
{
	int i;
	assert(pool);
	assert(helper);

	i = helper->seq;
	if(pool->helpers[i] == helper){
		LOCK(&(pool->wmutex));
		helper->busy = 0;
		if(pool->wpos > i)
			pool->wpos = i;
		UNLOCK(&(pool->wmutex));
	}
	else{
		assert(0);
	}
}

db_helper_t* get_read_helper(db_pool_t* pool)
{
	int i;
	db_helper_t* ret = NULL;

	assert(pool);

	LOCK(&(pool->rmutex));
	for(i = pool->rpos; i < pool->heplers_size; i ++){
		if(pool->read_helpers[i]->busy == 0){
			ret = pool->read_helpers[i];
			pool->read_helpers[i]->busy = 1;

			pool->rpos = i + 1;
			if(pool->rpos >= pool->heplers_size)
				pool->rpos = 0;

			break;
		}
	}
	UNLOCK(&(pool->rmutex));

	return ret;
}

void release_read_helper(db_pool_t* pool, db_helper_t* helper)
{
	int i;
	assert(pool && helper);

	i = helper->seq;
	if(pool->read_helpers[i] == helper){
		LOCK(&(pool->rmutex));
		helper->busy = 0;
		if(pool->rpos > i)
			pool->rpos = i;
		UNLOCK(&(pool->rmutex));
	}
	else{
		assert(0);
	}
}


	