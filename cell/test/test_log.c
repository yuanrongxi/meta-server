#include "cell_log.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>

static void* pt_func(void* arg)
{
	int i = 0;
	printf("start thread\n");
	while(i < 1000){
		log_error("OK, i = %d", i);
		i ++;
	}
}

static void test_speed()
{
	int i, delay;
	struct timeval b, s;
	gettimeofday(&b, NULL);

	for(i = 0; i < 1000000; i ++){
		log_info("very speed i = %d\n", i);
	}
	gettimeofday(&s, NULL);

	delay = (s.tv_sec - b.tv_sec) * 1000 + (s.tv_usec - b.tv_usec) / 1000;
	printf("write 1000000 log, delay = %d\n", delay);
}

int main(int argc, const char* argv)
{
	int i;
	pthread_t id[10];

	init_log();

	if(open_log("/var/log/cell/cell.log", LEVEL_DEBUG) != 0){
		printf("open log failed!\n");
		return 1;
	}

	test_speed();

	for(i = 0; i < 2; i ++)
		pthread_create(&id[i], NULL, pt_func, NULL);

	for(i = 0; i < 2; i ++)
		pthread_join(id[i], NULL);


	close_log();
}





