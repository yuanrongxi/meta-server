#include <sys/epoll.h>
#include <pthread.h>
#include <malloc.h>
#include <errno.h>
#include "ae_poll.h"
#include "cell_log.h"

typedef struct ae_state_s
{
	int epfd;		/*epool hander*/
	struct epoll_event events[AE_SETSIZE];
}ae_state_t;


int ae_create(event_loop_t *event)
{
	ae_state_t* state = malloc(sizeof(ae_state_t));
	if(state == NULL)
		return AE_ERR;

	state->epfd = epoll_create(1024);
	if(state->epfd == -1)
		return AE_ERR;

	event->apidata = state;

	return 0;
}

int ae_free(event_loop_t* event)
{
	ae_state_t* state = (ae_state_t*)(event->apidata);
	if(state != NULL){
		close(state->epfd);
		free(state);
	}
}

int ae_add_event(event_loop_t* event, int fd, int mask)
{
	ae_state_t* state = (ae_state_t*)(event->apidata);
	
	/*set event read or write*/
	struct epoll_event e;

	e.events = EPOLLONESHOT;
	if(mask & AE_READABLE)
		e.events |= EPOLLIN;
	if(mask & AE_WRITABLE)
		e.events |= EPOLLOUT;

	e.data.u64 = 0;
	e.data.fd = fd;
	if(epoll_ctl(state->epfd, EPOLL_CTL_ADD, fd, &e) == -1 && errno !=  EEXIST){
		log_error("epoll_ctl(EPOLL_CTL_ADD, %d) failed: error = %d\n", fd, errno);
		return AE_ERR;
	}

	return AE_OK;
}

int ae_update_event(event_loop_t*event, int fd, int mask)
{
	ae_state_t* state = (ae_state_t*)(event->apidata);
	
	struct epoll_event e;

	e.events = EPOLLONESHOT;
	if(mask & AE_READABLE)
		e.events |= EPOLLIN;
	if(mask & AE_WRITABLE)
		e.events |= EPOLLOUT;

	e.data.u64 = 0;
	e.data.fd = fd;

	if(epoll_ctl(state->epfd, EPOLL_CTL_MOD, fd, &e) == -1){
		log_error("epoll_ctl(EPOLL_CTL_MOD, %d) failed: error = %d\n", fd, errno);
		return AE_ERR;
	}

	return AE_OK;
}

int ae_del_event(event_loop_t* event, int fd)
{
	ae_state_t* state = (ae_state_t*)(event->apidata);
	
	struct epoll_event e;

	e.events = 0;
	e.data.u64 = 0;
	e.data.fd = fd;

	if(epoll_ctl(state->epfd, EPOLL_CTL_DEL, fd, &e) == -1 && errno != ENOENT && errno != EBADF){
		log_error("epoll_ctl(EPOLL_CTL_MOD, %d) failed: error = %d\n", fd, errno);
		return AE_ERR;
	}

	return AE_OK;
}

int ae_wait(event_loop_t* event, struct timeval* tv)
{
	ae_state_t* state = (ae_state_t*)(event->apidata);
	int retval, numevents = 0;

	retval = epoll_wait(state->epfd, state->events, AE_SETSIZE, (tv != NULL ? (tv->tv_sec*1000 + tv->tv_usec/1000) : -1));
	if(retval > 0){
		int i;

		numevents = retval;
		for(i = 0; i < retval; i ++){
			int mask = 0;
			struct epoll_event* e = state->events + i;
			
			if(e->events & EPOLLIN) mask |= AE_READABLE;
			if(e->events & EPOLLOUT) mask |= AE_WRITABLE;
			if (e->events & EPOLLERR) mask |= AE_WRITABLE;
			if (e->events & EPOLLHUP) mask |= AE_WRITABLE;

			/*set event socket*/
			event->fired[i].fd = e->data.fd;
			event->fired[i].mask = mask;
		}
	}

	return numevents;
}








