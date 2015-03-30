#ifndef __CELL_H_
#define __CELL_H_

#include "cell_pool.h"
#include "cell_codec.h"
#include "cell_handler.h"
#include "db_pool.h"

extern cell_pool_t* strm_pool;
extern cell_pool_t* handler_pool;

/*数据库连接池*/
extern db_pool_t* db_pool;

extern int daemon_run;

void process(uint16_t msg_id, handler_t* h);


#endif
