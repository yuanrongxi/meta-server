#ifndef CELL_ZK_H__
#define CELL_ZK_H__

#include <stdint.h>

#include <zookeeper/zookeeper.h>


#define ZK_MAX_NODE_SIZE 1024000
#define ZK_NOMAL_NODE_SIZE 2048

zhandle_t*		zk_get_handler();
zhandle_t* 		zk_init(const char* zk_host);
void 			zk_destroy();

int32_t			zk_node_exists(const char* path);
int32_t 		zk_create_node(const char* path, const char* value, int tmp);
int32_t			zk_delete_node(const char* path);
int32_t			zk_get_node(const char* path, char* value, int* len);
int32_t			zk_set_node(const char* path, const char* value);
int32_t			zk_append_node(const char* path, const char* value);
int32_t			zk_get_all_nodes(const char* path);

#endif



