#ifndef __POLL_THREAD_H_
#define __POLL_THREAD_H_

#include <stdint.h>
#include "cell_handler.h"

typedef void (*check_zookeeper_t)();

/*leader flower thread model*/
void	thread_init(int nthreads);

void	thread_destroy();

int32_t	add_event(int fd, int mask, handler_t* c);

int32_t update_event(int fd, int mask, handler_t* c);

int32_t delete_event(int fd);

void	loop_run(check_zookeeper_t fun);

#endif
