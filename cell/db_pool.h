#ifndef __DB_POOL_H_
#define __DB_POOL_H_

#include "db_helper.h"

typedef struct db_pool_s
{
	db_helper_t**	helpers;
	db_helper_t**	read_helpers;
	int				heplers_size;

	int				wmutex;
	int				rmutex;

	int				wpos;
	int				rpos;
}db_pool_t;

/*size和IO线程数相同或者稍大*/
db_pool_t*			create_db_pool(int size);

void				destroy_db_pool(db_pool_t* pool);

db_helper_t*		get_helper(db_pool_t* pool);

void				release_helper(db_pool_t* pool, db_helper_t* helper);

db_helper_t*		get_read_helper(db_pool_t* pool);

void				release_read_helper(db_pool_t* pool, db_helper_t* helper);

#endif





