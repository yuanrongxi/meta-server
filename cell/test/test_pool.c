#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/time.h>
#include "cell_pool.h"

#define MAGIC_N 0x0193d7ff

typedef struct test_ob_s
{
	int32_t		magic;
	int32_t		value;
	char*		name;
}test_ob_t;

int32_t	ob_init(void* ob)
{
	if(ob != NULL){
		test_ob_t* b = (test_ob_t *)ob;
		b->magic = 0;
		b->value = 0;
		b->name = (char *)calloc(1, 32 * sizeof(char));

		return 0;
	}
	
	return -1;
}

void ob_destroy(void* ob)
{
	if(ob != NULL){
		test_ob_t* b = (test_ob_t *)ob;
		b->magic = 0;
		b->value = 0;

		if(b->name != NULL){
			free(b->name);
			b->name = NULL;
		}
	}
}

void ob_reset(void* ob, int32_t flag)
{
	if(ob != NULL){
		test_ob_t* b = (test_ob_t *)ob;
		if(flag == 1){
			b->magic = MAGIC_N;
			b->value = 17888;
			strcpy(b->name, "zerok");
		}
		else{
			b->magic = 0;
			b->value = 0;
			memset(b->name, 0, 32);
		}
	}
}

int32_t ob_check(void* ob)
{
	int32_t ret = -1;
	if(ob != NULL){
		test_ob_t* b = (test_ob_t *)ob;
		if(b->magic == MAGIC_N)
			ret = 0;
	}
	return ret;
}

cell_pool_t* pool = NULL;

static void* pt_func(void* arg)
{
	int i = 0, count = 0;
	int delay = 0;
	test_ob_t* obs[128];
	test_ob_t* ob;

	printf("start thread\n");

	struct timeval b, s;
	gettimeofday(&b, NULL);

	while(count < 10000){
		for(i = 0; i < 128; i ++){
			obs[i] = NULL;
		}

		i = 0;

		while(i < 128){
			ob = pool_alloc(pool);
			if(ob != NULL)
				obs[i] = ob;

			i ++;
		}

		i = 0;
		while(i < 128){
			if(obs[i] != NULL)
				pool_free(pool, obs[i]);
			i ++;
		}

		count ++;
	}

	gettimeofday(&s, NULL);
	if(s.tv_usec > b.tv_usec)
		delay = (s.tv_sec - b.tv_sec) * 1000 + (s.tv_usec - b.tv_usec) / 1000;

	printf("alloc/free 1280000 object, delay = %dms\n", delay);
}


int main(int argc, const char* argv)
{
	test_ob_t* ob;
	pthread_t id[10];
	int i;

	pool = pool_create("test pool", sizeof(test_ob_t), ob_init, ob_destroy, ob_check, ob_reset);
	if(pool == NULL){
		printf("pool create failed!");
		exit(0);
	}

	for(i = 0; i < 2; i ++)
		pthread_create(&id[i], NULL, pt_func, NULL);

	for(i = 0; i < 2; i ++)
		pthread_join(id[i], NULL);

	pool_print(pool);

	pool_destroy(pool);

	return 0;
}




