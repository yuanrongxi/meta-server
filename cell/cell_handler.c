#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>

#include "cell_handler.h"
#include "ae_poll.h"
#include "poll_thread.h"
#include "cell_pool.h"
#include "cell.h"
#include "cell_log.h"

#define HANDLER_MAGIC 0x0e65970abcd37f41

/*备注：只有一个thread会操作一个handler的条件下作为设计实现*/

int32_t	handler_init(void* ptr)
{
	handler_t* handler = (handler_t *)ptr;
	
	handler->fd = -1;
	handler->ev_flags = 0;
	handler->upated = 0;
	handler->state = HANDLER_IDLE;
	handler->magigc = 0;

	handler->sstrm = (bin_stream_t *)pool_alloc(strm_pool);
	assert(handler->sstrm != NULL);
	handler->rstrm = (bin_stream_t *)pool_alloc(strm_pool);
	assert(handler->rstrm != NULL);

	return 0;
}

void handler_destroy(void* ptr)
{
	handler_t* handler = (handler_t *)ptr;
	/*归还bin stream*/
	
	pool_free(strm_pool, handler->sstrm);
	pool_free(strm_pool, handler->rstrm);
}

void handler_reset(void* ptr, int32_t flag)
{
	handler_t* handler = (handler_t *)ptr;
	if(handler == NULL)
		return ;

	if(flag == 1)
		handler->magigc = HANDLER_MAGIC;
	else
		handler->magigc = 0;

	handler->fd = -1;
	handler->ev_flags = 0;
	handler->upated = 0;
	handler->state = HANDLER_IDLE;

	/*清空缓冲区*/
	bin_stream_rewind(handler->rstrm, 1);
	bin_stream_reduce(handler->rstrm);
	bin_stream_rewind(handler->sstrm, 1);
	bin_stream_reduce(handler->sstrm);
}

int32_t	handler_check(void* ptr)
{
	if(ptr != 0){
		handler_t* handler = (handler_t *)ptr;
		if(handler->magigc == HANDLER_MAGIC)
			return 0;
	}

	return -1;
}

handler_t* handler_new(int fd, int32_t state)
{
	handler_t* h;

	h = (handler_t *)pool_alloc(handler_pool);
	if(h == NULL){
		log_error("out of memory!");
		return NULL;
	}

	h->fd = fd;
	h->state = state;

	h->upated = 1;

	h->ev_flags = AE_READABLE;
	if(h->state == HANDLER_CONNECTING)
		h->ev_flags |= AE_WRITABLE;

	if(add_event(fd, h->ev_flags, h) == -1){
		delete_event(h->fd);
		pool_free(handler_pool, h);

		return NULL;
	}

	return h;
}

void handler_close(handler_t* handler)
{
	log_info("handler close, fd = %d", handler->fd);
	assert(handler != NULL);

	delete_event(handler->fd);
	close(handler->fd);

	pool_free(handler_pool, handler);
}

/*一般是有handler_on_read调用触发*/
void hander_send(handler_t* h, uint16_t id, void* body)
{
	if(body != NULL){
		uint8_t* head;
		uint32_t len = 0;

		head = h->sstrm->wptr;
		mach_uint32_write(h->sstrm, len);
		encode_msg(h->sstrm, id, body);
		/*设置报文长度*/
		len = h->sstrm->wptr - head - sizeof(uint32_t);
		mach_put_4(head, len);

		h->upated = 1;
		h->ev_flags  |= AE_WRITABLE;
	}
}

static void check_write(handler_t* h)
{
	if(h->sstrm->used > h->sstrm->rsize){
		h->upated = 1;
		h->ev_flags |= AE_WRITABLE;
	}
	/*空出可以利用的bin stream空间*/
	bin_stream_move(h->sstrm);
}

static int32_t handler_on_write(handler_t* h)
{
	int rc;

	assert(h->state == HANDLER_CONNECTED);

	h->ev_flags = AE_NONE;

AGAIN:
	if(h->sstrm->used > h->sstrm->rsize){
		int32_t size = h->sstrm->used - h->sstrm->rsize;
		rc = write(h->fd, h->sstrm->rptr, size);
		if(rc > 0){
			h->sstrm->rptr += rc;
			h->sstrm->rsize += rc;

			goto AGAIN;
		}
		else if(rc == 0){
			log_error("socket write error! error = %d\n", errno);
			handler_close(h);
			return -1;
		}
		else{
			if(errno != EWOULDBLOCK && errno != EAGAIN){
				log_error("socket write error! error = %d\n", errno);
				handler_close(h);
				return -1;
			}
		}
	}

	check_write(h);
	h->ev_flags |= AE_READABLE;

	return 0;
}

static void split_message(handler_t* h)
{
	uint32_t len;
	uint16_t id;

	if(h->rstrm->rsize + 4 >= h->rstrm->used)
		return ;

	for(;;){
		len = mach_get_4(h->rstrm->rptr);
		id = mach_get_2(h->rstrm->rptr + 4);

		if(len <= h->rstrm->used - h->rstrm->rsize - 4){ /*收全了一个报文*/
			mach_uint32_read(h->rstrm, &len);
			mach_uint16_read(h->rstrm, &id);

			process(id, h);
		}
		else
			return;

		if(h->rstrm->rsize >= h->rstrm->used)
			return ;
	}
}

static int32_t handler_on_read(handler_t* h)
{
	int rc;

	assert(h->state == HANDLER_CONNECTED);

	h->ev_flags = AE_NONE;

	for(;;){
		/*对接受缓冲区做扩展*/
		bin_stream_move(h->rstrm);
		if(h->rstrm->used >= h->rstrm->size)
			bin_stream_resize(h->rstrm, h->rstrm->size * 2);

		rc = read(h->fd, h->rstrm->wptr, h->rstrm->size - h->rstrm->used);
		if(rc > 0){
			h->rstrm->wptr += rc;
			h->rstrm->used += rc;

			split_message(h);
		}
		else if(rc == 0){
			log_error("client close connection!\n");
			handler_close(h);
			return -1;
		}
		else {
			if(errno != EWOULDBLOCK && errno != EAGAIN){
				log_error("socket read error! error = %d", errno);
				handler_close(h);
				return -1;
			}
			else 
				goto EXIT_FOR;
		}
	}

EXIT_FOR:
	check_write(h);

	h->upated = 1;
	h->ev_flags |= AE_READABLE;

	return 0;
}

static void handler_on_listening(handler_t* h)

{
	struct sockaddr_in addr;
	socklen_t addrlen;
	int fd = -1, flags = 1;
	char ip_addr[32];

	addrlen = sizeof(addr);
	for(;;){
		fd = accept(h->fd, (struct sockaddr *)&addr, &addrlen);
		if(fd == -1){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				return ;
			}
			else if(errno == EMFILE){
				log_error("too mach opened socket!");
				return;
			}
			else{
				log_error("accept failed, error = %d", errno);
				return ;
			}
		}

		/*设置异步accept socket*/
		if ((flags = fcntl(fd, F_GETFL, 0)) < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
				log_error("setting O_NONBLOCK");
				close(fd);
				break;
		}

		if(handler_new(fd, HANDLER_CONNECTING) == NULL){
			log_error("create handler failed! fd = %d", fd);
			close(fd);
		}

		inet_ntop(addr.sin_family, &(addr.sin_addr), ip_addr, sizeof(ip_addr));
		log_info("accept tcp socket, addr = %s:%d", ip_addr, ntohs(addr.sin_port));
	}
}

int32_t handle_machine(handler_t* h, int mask)
{
	switch(h->state){
	case HANDLER_LISTENING:
		handler_on_listening(h);
		break;

	case HANDLER_CONNECTING:
		h->state = HANDLER_CONNECTED;
	case HANDLER_CONNECTED:
		{
			if((mask & AE_READABLE) && handler_on_read(h) != 0)
				return -1;

			if((mask & AE_WRITABLE) && handler_on_write(h) != 0)
				return -1;
		}
		break;

	case HANDLER_DISCONNECTED:
	case HANDLER_IDLE:
		handler_close(h);
		return -1;

	default:
		log_fatal("handler state is error, fd = %d", h->fd);
	}

	return 0;
}

