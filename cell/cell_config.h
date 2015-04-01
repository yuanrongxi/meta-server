#ifndef __CELL_CONFIG_H_
#define __CELL_CONFIG_H_

#include <stdint.h>

/*最大从数据库服务器的个数*/
#define DB_SLAVE_N			10

#define PATH_SIZE			1024
#define VALUE_SIZE			64

typedef struct cell_conf_s
{
	int			thread_n;				/*线程个数*/
	char		name[VALUE_SIZE];		/*meta server的名称，node1/node2/node3...,名字不能和其他meta Server重复*/
	char		listen_ip[VALUE_SIZE];	/*meta server的ip或者域名*/
	int			listen_port;			/*meta server对外接口*/
	int64_t		cache_size;				/*lru cache最大占用的内存空间*/
	char		zk_host[PATH_SIZE];		/*zookeeper的访问地址*/

	char		user[VALUE_SIZE];		/*数据库访问用户名*/
	char		passwd[VALUE_SIZE];		/*数据库访问密码*/
	char		db_name[VALUE_SIZE];	/*操作的数据库名*/

	char		db_master[VALUE_SIZE];	/*主数据库服务IP或域名*/
	char*		db_slaves[DB_SLAVE_N];	/*从数据库服务IP或域名，多个*/
	int			db_port;				/*数据库访问端口，*/					

	char		log_path[PATH_SIZE];	/*日志文件路径*/
}cell_conf_t;

extern cell_conf_t*	cell_config;

/*从配置文件中将cell config导入到内存中*/
void			load_config(const char* config_file);
/*关闭cell config内存对象*/
void			close_config();

void			print_config();

int				load_zookeeper_host();

#endif




