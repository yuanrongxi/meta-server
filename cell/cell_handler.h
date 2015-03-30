#ifndef __CELL_HANDLER_H_
#define __CELL_HANDLER_H_

#include <stdint.h>

#include "cell_msg.h"
#include "cell_codec.h"

/*hander state*/
#define HANDLER_IDLE			0
#define HANDLER_LISTENING		1
#define HANDLER_CONNECTING		2
#define HANDLER_CONNECTED		3
#define HANDLER_DISCONNECTED	4

typedef struct handler_s
{
	int				fd;					/*socket 句柄*/
	int32_t			upated;				/*是否改变了监听事件*/
	int				ev_flags;			/*监听时间集*/

	int32_t			state;				/*当前的连接状态*/

	bin_stream_t*	rstrm;				/*接收缓冲区*/	
	bin_stream_t*	sstrm;				/*发送缓冲区*/

	uint64_t		magigc;				/*校验魔法字,用于cell pool*/
}handler_t;

/*cell pool callback function*/
int32_t				handler_init(void* ptr);
void				handler_destroy(void* ptr);
void				handler_reset(void* ptr, int32_t flag);
int32_t				handler_check(void* ptr);

handler_t*			handler_new(int fd, int32_t state);
void				handler_close(handler_t* handler);

int32_t				handle_machine(handler_t* hander, int mask);

void				hander_send(handler_t* handler, uint16_t id, void* body);

#endif
