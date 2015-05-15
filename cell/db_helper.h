#ifndef __DB_HELPER_H_
#define __DB_HELPER_H_

#include <mysql.h>
#include <stdint.h>
#include "cell_msg.h"

#define HOST_SIZE		64
#define USER_SIZE		64
#define DB_NAME_SIZE	64

#define FREE_NONE		0
#define FREE_KEY		0X01
#define FREE_VALUE		0X02
#define FREE_META		0x04

#define MAX_KEY_SIZE	512
#define MAX_VALUE_SIZE	(1024 * 1024)
#define MAX_SQL_SIZE	((MAX_VALUE_SIZE + MAX_KEY_SIZE) * 2 + 1024)

typedef struct meta_kv_s
{
	char*		key;
	size_t		key_size;

	char*		value;
	size_t		value_size;

	uint32_t	free_flag;
}meta_kv_t;

void			init_meta_kv(meta_kv_t* meta);
void			destroy_meta_kv(meta_kv_t* meta);

typedef struct db_helper_s
{
	/*数据库连接信息*/
	char		db_host[HOST_SIZE];
	char		user[USER_SIZE];
	char		passwd[USER_SIZE];
	char		db_name[DB_NAME_SIZE];
	uint32_t	port;

	/*状态信息*/
	uint32_t	opened;
	int32_t		busy;

	/*一些常使用的变量*/
	char*		sql;
	int32_t		retry_count;
	/*对应pool的slot序号*/
	int			seq;

	/*mysql对象*/
	MYSQL		mysql;
}db_helper_t;

/*将连接参数放入helper中,并初始化状态*/
db_helper_t*	create_mysql(const char* host, uint32_t port, const char* user, const char* pwd, const char* db_name, int seq);
/*撤销helper*/
void			destroy_mysql(db_helper_t* h);
/*连接并打开mysql数据库*/
int32_t			open_mysql(db_helper_t* h);
/*关闭打开的数据库*/
int32_t			close_mysql(db_helper_t* h);

/*数据库操作接口*/
int32_t			insert_kv(db_helper_t* h, meta_kv_t* kv, const char* space, char* err);
int32_t			replace_kv(db_helper_t* h, meta_kv_t* kv, const char* space, char* err);
int32_t			update_kv(db_helper_t* h, meta_kv_t* kv, const char* space, char* err);
int32_t			delete_kv(db_helper_t* h, const char* key, size_t key_size, const char* space, char* err);

int32_t			get_kv(db_helper_t* h, meta_kv_t* kv, const char* space, char* err);
int32_t			scan_kv(db_helper_t* h, const char* start_key, size_t start_key_size, 
						const char* end_key, size_t end_key_size, meta_kv_t** kvs, 
						const char* space, int limit, int skip, char* err);

/*写日志*/
int32_t			insert_dfs_log(db_helper_t* h, add_log_t* info, char* err);

/*上传者空间管理*/
int32_t			get_dfs_user_info(db_helper_t* h, const char* user, int64_t* total_size, int64_t* used_size, 
									int64_t* day_size, int64_t* day_used, int* flag, char* err);

int32_t			update_dfs_user_info(db_helper_t* h, const char* user, uint32_t file_size, char* err);
int32_t			update_dfs_user_flag(db_helper_t* h, const char* user, int flag, char* err);

#endif




