#ifndef __AE_POLL_H_
#define __AE_POLL_H_

#include <stdint.h>
#include <error.h>
#include "cell_handler.h"

/*最大64K的socket,在linux ulimit中做限制*/
#define AE_SETSIZE (1024 * 64)

#define AE_OK 0
#define AE_ERR -1

#define AE_NONE			0x00
#define AE_READABLE		0x01
#define AE_WRITABLE		0x02

typedef struct fired_event_s
{
	int		fd;
	int		mask;
}fired_event_t;

typedef struct event_loop_s
{
	handler_t*			conns[AE_SETSIZE];
	fired_event_t		fired[AE_SETSIZE];
	int					nready;
	void*				apidata;
}event_loop_t;

int ae_create(event_loop_t *event);

int ae_free(event_loop_t* event);

int ae_add_event(event_loop_t* event, int fd, int mask);

int ae_update_event(event_loop_t*event, int fd, int mask);

int ae_del_event(event_loop_t* event, int fd);

int ae_wait(event_loop_t* event, struct timeval* tv);

#endif
