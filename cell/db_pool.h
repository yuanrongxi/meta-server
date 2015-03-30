#ifndef __DB_POOL_H_
#define __DB_POOL_H_

#include "db_helper.h"

typedef struct db_pool_s
{
	db_helper_t**	helpers;
	int				heplers_size;

	int				mutex;
}db_pool_t;

/*size和IO线程数相同或者稍大*/
db_pool_t*			create_db_pool(int size, const char* host, uint32_t port, const char* user, const char* pwd, const char* db_name);

void				destroy_db_pool(db_pool_t* pool);

db_helper_t*		get_helper(db_pool_t* pool);

void				release_helper(db_pool_t* pool, db_helper_t* helper);

#endif





