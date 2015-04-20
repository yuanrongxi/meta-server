#include "db_helper.h"
#include "cell_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errmsg.h>
#include <assert.h>
#include <mysqld_error.h>

#ifdef WIN32
#define atoll		_atoi64
#endif

void init_meta_kv(meta_kv_t* meta)
{
	meta->key = NULL;
	meta->key_size = 0;

	meta->value = NULL;
	meta->value_size = 0;

	meta->free_flag = FREE_NONE;
}

void destroy_meta_kv(meta_kv_t* meta)
{
	assert(meta != NULL);

	meta->key_size = 0;
	meta->value_size = 0;

	if(meta->free_flag == FREE_NONE)
		return ;

	if(meta->key != NULL && (meta->free_flag & FREE_KEY))
		free(meta->key);

	if(meta->key != NULL && (meta->free_flag & FREE_VALUE))
		free(meta->value);

	if(meta->free_flag & FREE_META)
		free(meta);

	meta->free_flag = FREE_NONE;
}

db_helper_t* create_mysql(const char* host, uint32_t port, const char* user, const char* pwd, const char* db_name)
{
	db_helper_t* h;

	assert(host && user && pwd && db_name && port != 0);
	assert(strlen(host) < HOST_SIZE);
	assert(strlen(user) < USER_SIZE);
	assert(strlen(pwd) < USER_SIZE);
	assert(strlen(db_name) < DB_NAME_SIZE);

	h = malloc(sizeof(db_helper_t));
	if(h == NULL)
		return NULL;

	h->sql = (char *)malloc(MAX_SQL_SIZE * sizeof(char));
	if(h->sql == NULL){
		free(h);
		return NULL;
	}

	strcpy(h->db_host, host);
	strcpy(h->user, user);
	strcpy(h->passwd, pwd);
	strcpy(h->db_name, db_name);
	h->port = port;

	h->opened = 0;
	h->busy = 0;
	h->retry_count = 3;

	return h;
}

void destroy_mysql(db_helper_t* h)
{
	assert(h);

	close_mysql(h);

	if(h->sql != NULL)
		free(h->sql);

	free(h);
}

int32_t open_mysql(db_helper_t* h)
{
	/*设置读写连接超时，5秒*/
	int v = 5;
	MYSQL* conn = NULL;

	assert(h);
	if(h->opened != 0)
		return 0;

	mysql_init(&(h->mysql));
	mysql_options(&(h->mysql), MYSQL_OPT_CONNECT_TIMEOUT, (const char *)&v);
	mysql_options(&(h->mysql), MYSQL_OPT_READ_TIMEOUT, (const char *)&v);
	mysql_options(&(h->mysql), MYSQL_OPT_WRITE_TIMEOUT, (const char *)&v);

	conn = mysql_real_connect(&(h->mysql), h->db_host, h->user, h->passwd, h->db_name, h->port, NULL, CLIENT_MULTI_STATEMENTS);
	if(conn == NULL){
		log_error("connect mysql database (%s:%d, u = %s, p = %s, db = %s, error = %s)", 
					h->db_host, h->port, h->user, h->passwd, h->db_name, mysql_error(&(h->mysql)));
		return -1;
	}

	log_info("connect mysql(%s:%d) ok!", h->db_host, h->port);

	h->opened = 1;

	return 0;
}

int32_t close_mysql(db_helper_t* h)
{
	assert(h);

	if(h->opened){
		 mysql_close(&(h->mysql));
		 h->opened = 0;
		 log_warn("close msyql(%s, %d)!!", h->db_host, h->port);
	}

	return 0;
}

int32_t insert_kv(db_helper_t* h, meta_kv_t* kv, const char* space, char* info)
{
	int count = 0;
	int pos = 0;

	assert(h && kv && space);
	assert(kv->key_size < MAX_KEY_SIZE && kv->value_size < MAX_VALUE_SIZE);

#ifndef WIN32
	pos = snprintf(h->sql, MAX_SQL_SIZE, "insert into %s (meta_key, meta_value) values ('", space);
#else
	pos = _snprintf(h->sql, MAX_SQL_SIZE, "insert into %s (meta_key, meta_value) values ('", space);
#endif

RETRY:
	if(h->opened == 0) /*连接已经断开，重新连接mysql*/
		open_mysql(h);

	if(h->opened != 0){
		if(count == 0){
			pos += mysql_real_escape_string(&(h->mysql), h->sql + pos, kv->key, kv->key_size);
			pos += sprintf(h->sql + pos, "%s", "','");

			pos += mysql_real_escape_string(&(h->mysql), h->sql + pos, kv->value, kv->value_size);
			pos += sprintf(h->sql + pos, "%s", "')");
		}
		
		log_debug("execute sql:%s", h->sql);

		/*进行MYSQL语句执行*/
		if(mysql_real_query(&(h->mysql), h->sql, pos) != 0){
			int err = mysql_errno(&(h->mysql));
			if(err == CR_SERVER_GONE_ERROR && count++ < h->retry_count){ /*重新尝试执行*/
				log_error("mysql is error, error = CR_SERVER_GONE_ERROR, retry!");
				close_mysql(h);
				goto RETRY;
			}
			else{
				log_error("mysql error = %d, err = %s", err, mysql_error(&(h->mysql)));
				strncpy(info, mysql_error(&(h->mysql)), MAX_INFO_SIZE);
				close_mysql(h);
				return -1;
			}
		}
	}
	else{
		log_error("mysql disconnected!!");
		strncpy(info, "metaserver connect mysql failed", MAX_INFO_SIZE);
		return -1;
	}

	log_debug("insert affect row 0");
	return 0;
}

int32_t replace_kv(db_helper_t* h, meta_kv_t* kv, const char* space, char* info)
{
	int count = 0;
	int pos = 0;

	assert(h && kv && space);
	assert(kv->key_size < MAX_KEY_SIZE && kv->value_size < MAX_VALUE_SIZE);

	pos = snprintf(h->sql, MAX_SQL_SIZE, "insert into %s (meta_key, meta_value) values ('", space);

RETRY:
	if(h->opened == 0) /*连接已经断开，重新连接mysql*/
		open_mysql(h);

	if(h->opened != 0){
		if(count == 0){
			pos += mysql_real_escape_string(&(h->mysql), h->sql + pos, kv->key, kv->key_size);
			pos += sprintf(h->sql + pos, "%s", "','");

			pos += mysql_real_escape_string(&(h->mysql), h->sql + pos, kv->value, kv->value_size);
			pos += sprintf(h->sql + pos, "%s", "')  on duplicate key update meta_value='");

			pos += mysql_real_escape_string(&(h->mysql), h->sql + pos, kv->value, kv->value_size);
			pos += sprintf(h->sql + pos, "%s", "'");
		}

		log_debug("execute sql:%s", h->sql);

		/*进行MYSQL语句执行*/
		if(mysql_real_query(&(h->mysql), h->sql, pos) != 0){
			int err = mysql_errno(&(h->mysql));
			if(err == CR_SERVER_GONE_ERROR && count++ < h->retry_count){ /*重新尝试执行*/
				log_error("mysql is error, error = CR_SERVER_GONE_ERROR, retry!");
				close_mysql(h);
				goto RETRY;
			}
			else{
				log_error("mysql error = %d", err);
				strncpy(info, mysql_error(&(h->mysql)), MAX_INFO_SIZE);
				close_mysql(h);
				return -1;
			}
		}
	}
	else{
		log_error("mysql disconnected!!");
		strncpy(info, "metaserver connect mysql failed", MAX_INFO_SIZE);
		return -1;
	}

	return 0;
}

int32_t update_kv(db_helper_t* h, meta_kv_t* kv, const char* space, char* info)
{
	int count = 0;
	int pos = 0;

	assert(h && kv && space);
	assert(kv->key_size < MAX_KEY_SIZE && kv->value_size < MAX_VALUE_SIZE);

	pos = snprintf(h->sql, MAX_SQL_SIZE, "update %s set meta_value='", space);

RETRY:
	if(h->opened == 0) /*连接已经断开，重新连接mysql*/
		open_mysql(h);

	if(h->opened != 0){
		if(count == 0){
			pos += mysql_real_escape_string(&(h->mysql), h->sql + pos, kv->value, kv->value_size);
			pos += sprintf(h->sql + pos, "%s", "' where meta_key='");

			pos += mysql_real_escape_string(&(h->mysql), h->sql + pos, kv->key, kv->key_size);
			pos += sprintf(h->sql + pos, "%s", "'");
		}

		log_debug("execute sql:%s", h->sql);
		/*进行MYSQL语句执行*/
		if(mysql_real_query(&(h->mysql), h->sql, pos) != 0){
			int err = mysql_errno(&(h->mysql));
			if(err == CR_SERVER_GONE_ERROR && count++ < h->retry_count){ /*重新尝试执行*/
				log_error("mysql is error, error = CR_SERVER_GONE_ERROR, retry!");
				close_mysql(h);
				goto RETRY;
			}
			else{
				log_error("mysql error = %d", err);
				strncpy(info, mysql_error(&(h->mysql)), MAX_INFO_SIZE);
				close_mysql(h);
				return -1;
			}
		}
	}
	else{
		log_error("mysql disconnected!!");
		strncpy(info, "metaserver connect mysql failed", MAX_INFO_SIZE);
		return -1;
	}

	return 0;
}

int32_t delete_kv(db_helper_t* h, const char* key, size_t key_size, const char* space, char* info)
{
	int count = 0;
	int pos = 0;

	assert(h && key && space);
	assert(key_size < MAX_KEY_SIZE);

	pos = snprintf(h->sql, MAX_SQL_SIZE, "delete from %s where meta_key='", space);

RETRY:
	if(h->opened == 0)
		open_mysql(h);

	if(h->opened != 0){
		if(count == 0){
			pos += mysql_real_escape_string(&(h->mysql), h->sql + pos, key, key_size);
			pos += sprintf(h->sql + pos, "%s", "'");
		}

		log_debug("execute sql:%s", h->sql);

		if(mysql_real_query(&(h->mysql), h->sql, pos) != 0){
			int err = mysql_errno(&(h->mysql));
			if(err == CR_SERVER_GONE_ERROR && count++ < h->retry_count){ /*重新尝试执行*/
				log_error("mysql is error, error = CR_SERVER_GONE_ERROR, retry!");
				close_mysql(h);
				goto RETRY;
			}
			else{
				log_error("mysql error = %d, info = %s", err, mysql_error(&(h->mysql)));
				strncpy(info, mysql_error(&(h->mysql)), MAX_INFO_SIZE);
				close_mysql(h);
				return -1;
			}
		}
	}
	else{
		log_error("mysql disconnected!!");
		strncpy(info, "metaserver connect mysql failed", MAX_INFO_SIZE);
		return -1;
	}

	return 0;
}

int32_t get_kv(db_helper_t* h, meta_kv_t* kv, const char* space, char* info)
{
	int count = 0;
	int pos = 0;
	int ret = 0;

	 MYSQL_ROW row;
	 MYSQL_RES *mysql_ret;

	assert(h && kv && space);
	assert(kv->key_size < MAX_KEY_SIZE);

	pos = snprintf(h->sql, MAX_SQL_SIZE, "select meta_value from %s where meta_key='", space);

RETRY:
	if(h->opened == 0)
		open_mysql(h);

	if(h->opened != 0){
		if(count == 0){
			pos += mysql_real_escape_string(&(h->mysql), h->sql + pos, kv->key, kv->key_size);
			pos += sprintf(h->sql + pos, "%s", "'");
		}

		log_debug("execute sql:%s", h->sql);

		if(mysql_real_query(&(h->mysql), h->sql, pos) != 0){
			int err = mysql_errno(&(h->mysql));
			if(err == CR_SERVER_GONE_ERROR && count++ < h->retry_count){ /*重新尝试执行*/
				log_error("mysql is error, error = CR_SERVER_GONE_ERROR, retry!");
				close_mysql(h);
				goto RETRY;
			}
			else{
				log_error("mysql error = %d, info = %s", err, mysql_error(&(h->mysql)));
				strncpy(info, mysql_error(&(h->mysql)), MAX_INFO_SIZE);
				close_mysql(h);
				return -1;
			}
		}
	}
	else{
		log_error("mysql disconnected!!");
		strncpy(info, "metaserver connect mysql failed", MAX_INFO_SIZE);
		return -1;
	}

	mysql_ret = mysql_store_result(&(h->mysql));
	if(mysql_ret == NULL){
		log_error("mysql store result fialed! %s %s", h->db_host, mysql_error(&(h->mysql)));
		return -1;
	}

	while((row = mysql_fetch_row(mysql_ret)) != NULL){
		int value_size = 0;
		unsigned long* len = mysql_fetch_lengths(mysql_ret);
		value_size = len[0];
		if(value_size > MAX_VALUE_SIZE){
			log_error("value size error, size = %d", value_size);
			value_size = MAX_VALUE_SIZE;
		}

		kv->value = calloc(1, sizeof(char) * (value_size + 1));
		memcpy(kv->value, row[0], value_size);
		kv->value_size = value_size;
		kv->free_flag |= FREE_VALUE;

		ret ++;
		break;
	}

	mysql_free_result(mysql_ret);

	return (ret>0 ? 0 : -1);
}

int32_t scan_kv(db_helper_t* h, const char* start_key, size_t start_key_size, 
								const char* end_key, size_t end_key_size, 
								meta_kv_t** kvs, const char* space, int limit, int skip, char* info)
{
	int count = 0;
	int pos = 0;
	int ret = 0;

	MYSQL_ROW row;
	MYSQL_RES *mysql_ret;

	if(limit == 0)
		return -1;

	assert(h && start_key && end_key && kvs);
	assert(start_key_size < MAX_KEY_SIZE && end_key_size < MAX_KEY_SIZE);

	pos = snprintf(h->sql, MAX_SQL_SIZE, "select meta_key, meta_value, from %s where ", space);

RETRY:
	if(h->opened == 0)
		open_mysql(h);

	if(h->opened != 0){
		limit = (limit > 0 ? limit : (0 - limit));
		if(count == 0){
			pos += sprintf(h->sql + pos, "meta_key%s'", (skip != 1 ? "<" : ">="));
			pos += mysql_real_escape_string(&(h->mysql), h->sql + pos, start_key, start_key_size);

			pos += sprintf(h->sql + pos, "' and meta_key >= '");
			pos += mysql_real_escape_string(&(h->mysql), h->sql + pos, end_key, end_key_size);
			pos += sprintf(h->sql + pos, "' order by meta_key desc limit %d ", limit);
		}

		log_debug("execute sql:%s", h->sql);

		if(mysql_real_query(&(h->mysql), h->sql, pos) != 0){
			int err = mysql_errno(&(h->mysql));
			if(err == CR_SERVER_GONE_ERROR && count++ < h->retry_count){ /*重新尝试执行*/
				log_error("mysql is error, error = CR_SERVER_GONE_ERROR, retry!");
				close_mysql(h);
				goto RETRY;
			}
			else{
				log_error("mysql error = %d", err);
				strncpy(info, mysql_error(&(h->mysql)), MAX_INFO_SIZE);
				close_mysql(h);
				return -1;
			}
		}
	}
	else{
		log_error("mysql disconnected!!");
		strncpy(info, "metaserver connect mysql failed", MAX_INFO_SIZE);
		return -1;
	}

	mysql_ret = mysql_store_result(&(h->mysql));
	if(mysql_ret == NULL){
		log_error("mysql store result failed!, %s %s", h->db_host, mysql_error(&(h->mysql)));
		return -1;
	}

	kvs = malloc(sizeof(meta_kv_t *) * limit);
	while((row = mysql_fetch_row(mysql_ret))){
		int key_size, value_size;
		unsigned long* len;

		key_size = len[0];
		value_size = len[1];
		if(key_size > MAX_KEY_SIZE){
			log_error("key size error, size = %d", key_size);
			key_size = MAX_KEY_SIZE;
		}

		if(value_size > MAX_VALUE_SIZE){
			log_error("value size error, size = %d", value_size);
			value_size = MAX_VALUE_SIZE;
		}

		meta_kv_t* kv = malloc(sizeof(meta_kv_t));
		kv->key = malloc(sizeof(char) * key_size);
		kv->value = malloc(sizeof(char) * value_size);

		memcpy(kv->key, row[0], key_size);
		kv->key_size = key_size;

		memcpy(kv->value, row[1], value_size);
		kv->value_size = value_size;

		kv->free_flag = FREE_KEY | FREE_VALUE | FREE_META;

		assert(ret + 1 < limit);
		kvs[ret ++] = kv;
	}

	mysql_free_result(mysql_ret);

	/*ret >= 0*/ 
	return ret;
}

int32_t insert_dfs_log(db_helper_t* h, add_log_t* info, char* oinfo)
{
	int count = 0;
	int pos = 0;

	assert(info != NULL && h != NULL);

#ifndef WIN32
	pos = snprintf(h->sql, MAX_SQL_SIZE, "insert into dfs_log_table (url, operator, ip, pool, type) values ('%s', '%s', '%s', '%s', '%d');",
		info->path, info->user, info->ip, info->pool, info->type);
#else
	pos = _snprintf(h->sql, MAX_SQL_SIZE, "insert into dfs_log_table (url, operator, ip, pool, type) values ('%s', '%s', '%s', '%s', '%d');",
		info->path, info->user, info->ip, info->pool, info->type);
#endif

RETRY:
	if(h->opened == 0) /*连接已经断开，重新连接mysql*/
		open_mysql(h);

	if(h->opened != 0){
		log_debug("execute sql:%s", h->sql);
		/*进行MYSQL语句执行*/
		if(mysql_real_query(&(h->mysql), h->sql, pos) != 0){
			int err = mysql_errno(&(h->mysql));
			if(err == CR_SERVER_GONE_ERROR && count++ < h->retry_count){ /*重新尝试执行*/
				log_error("mysql is error, error = CR_SERVER_GONE_ERROR, retry!");
				close_mysql(h);
				goto RETRY;
			}
			else{
				log_error("mysql error = %d", err);
				strncpy(oinfo, mysql_error(&(h->mysql)), MAX_INFO_SIZE);
				close_mysql(h);
				return -1;
			}
		}
	}
	else{
		log_error("mysql disconnected!!");
		strncpy(oinfo, "metaserver connect mysql failed", MAX_INFO_SIZE);
		return -1;
	}

	return 0;
}

int32_t get_dfs_user_info(db_helper_t* h, const char* user, int64_t* total_size, int64_t* used_size, int64_t* day_size,
						int64_t* day_used, int* flag, char* info)
{
	int count = 0;
	int pos = 0;
	int ret;

	MYSQL_ROW row;
	MYSQL_RES *mysql_ret;

	assert(h != NULL && user != NULL);

#ifndef WIN32
	pos = snprintf(h->sql, MAX_SQL_SIZE, "select total_size, used_size, size_day, used_day, flag from dfs_user where user = '%s';", user);
#else
	pos = _snprintf(h->sql, MAX_SQL_SIZE, "select total_size, used_size, size_day, used_day, flag from dfs_user where user = '%s';", user);
#endif

RETRY:
	if(h->opened == 0) /*连接已经断开，重新连接mysql*/
		open_mysql(h);

	if(h->opened != 0){
		log_debug("execute sql:%s", h->sql);
		/*进行MYSQL语句执行*/
		if(mysql_real_query(&(h->mysql), h->sql, pos) != 0){
			int err = mysql_errno(&(h->mysql));
			if(err == CR_SERVER_GONE_ERROR && count++ < h->retry_count){ /*重新尝试执行*/
				log_error("mysql is error, error = CR_SERVER_GONE_ERROR, retry!");
				close_mysql(h);
				goto RETRY;
			}
			else{
				log_error("mysql error = %d", err);
				strncpy(info, mysql_error(&(h->mysql)), MAX_INFO_SIZE);
				close_mysql(h);
				return -1;
			}
		}
	}
	else{
		log_error("mysql disconnected!!");
		strncpy(info, "metaserver connect mysql failed", MAX_INFO_SIZE);
		return -1;
	}

	mysql_ret = mysql_store_result(&(h->mysql));
	if(mysql_ret == NULL){
		log_error("mysql store result fialed! %s %s", h->db_host, mysql_error(&(h->mysql)));
		strncpy(info, mysql_error(&(h->mysql)), MAX_INFO_SIZE);
		return -1;
	}

	ret = 0;

	/*取值*/
	if(mysql_num_fields(mysql_ret) == 5){
		while((row = mysql_fetch_row(mysql_ret)) != NULL){
			*total_size = atoll(row[0]);
			*used_size = atoll(row[1]);
			*day_size = atoll(row[2]);
			*day_used = atoll(row[3]);
			*flag = atoi(row[4]);

			ret ++;
			break;
		}
	}

	mysql_free_result(mysql_ret);

	return (ret>0 ? 0 : -2);
}

int32_t update_dfs_user_info(db_helper_t* h, const char* user, uint32_t file_size, char* info)
{
	int count = 0;
	int pos = 0;

	assert(user != NULL && h != NULL);

#ifndef WIN32
	pos = snprintf(h->sql, MAX_SQL_SIZE, "update dfs_user set used_size = used_size + %d, used_day = used_day + %d where user = '%s';",
		file_size, file_size, user);
#else
	pos = _snprintf(h->sql, MAX_SQL_SIZE, "update dfs_user set used_size = used_size + %d, used_day = used_day + %d where user = '%s';",
		file_size, file_size, user);
#endif

RETRY:
	if(h->opened == 0) /*连接已经断开，重新连接mysql*/
		open_mysql(h);

	if(h->opened != 0){
		log_debug("execute sql:%s", h->sql);
		/*进行MYSQL语句执行*/
		if(mysql_real_query(&(h->mysql), h->sql, pos) != 0){
			int err = mysql_errno(&(h->mysql));
			if(err == CR_SERVER_GONE_ERROR && count++ < h->retry_count){ /*重新尝试执行*/
				log_error("mysql is error, error = CR_SERVER_GONE_ERROR, retry!");
				close_mysql(h);
				goto RETRY;
			}
			else{
				log_error("mysql error = %d", err);
				strncpy(info, mysql_error(&(h->mysql)), MAX_INFO_SIZE);
				close_mysql(h);
				return -1;
			}
		}
	}
	else{
		log_error("mysql disconnected!!");
		strncpy(info, "metaserver connect mysql failed", MAX_INFO_SIZE);
		return -1;
	}

	return 0;
}

int32_t update_dfs_user_flag(db_helper_t* h, const char* user, int flag, char* info)
{
	int count = 0;
	int pos = 0;

	assert(user != NULL && h != NULL);

#ifndef WIN32
	pos = snprintf(h->sql, MAX_SQL_SIZE, "update dfs_user set flag = %d where user = '%s';", flag, user);
#else
	pos = _snprintf(h->sql, MAX_SQL_SIZE, "update dfs_user set flag = %d where user = '%s';", flag, user);
#endif

RETRY:
	if(h->opened == 0) /*连接已经断开，重新连接mysql*/
		open_mysql(h);

	if(h->opened != 0){
		log_debug("execute sql:%s", h->sql);
		/*进行MYSQL语句执行*/
		if(mysql_real_query(&(h->mysql), h->sql, pos) != 0){
			int err = mysql_errno(&(h->mysql));
			if(err == CR_SERVER_GONE_ERROR && count++ < h->retry_count){ /*重新尝试执行*/
				log_error("mysql is error, error = CR_SERVER_GONE_ERROR, retry!");
				close_mysql(h);
				goto RETRY;
			}
			else{
				log_error("mysql error = %d", err);
				strncpy(info, mysql_error(&(h->mysql)), MAX_INFO_SIZE);
				close_mysql(h);
				return -1;
			}
		}
	}
	else{
		log_error("mysql disconnected!!");
		strncpy(info, "metaserver connect mysql failed", MAX_INFO_SIZE);
		return -1;
	}

	return 0;
}




