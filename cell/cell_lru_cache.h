/*****************************************************************
*定义一个lru-cache的hash table对照表，内存分配采用tcmalloc来做控制
*
*****************************************************************/
#ifndef __CELL_LRU_CACHE_H_
#define __CELL_LRU_CACHE_H_

#include <stdint.h>

#define ITEM_KEY(k)				((uint8_t*)((k)->data))
#define ITEM_VALUE(k)			((uint8_t*)((k)->data + (k)->ksize))

typedef struct cache_item_t
{
	struct cache_item_t*	next;			/*lru list中的后一个单元关系*/
	struct cache_item_t*	prev;			/*lru list中的前一个单元关系*/
	struct cache_item_t*	hash_next;		/*hash table中的后一个关系*/
	size_t					ksize;			/*key占用的空间大小*/
	size_t					vsize;
	size_t					size;			/*整个结构体占用的空间数*/
	time_t					time;		
	uint16_t				refcount;		/*应用计数*/
	uint16_t				vcount;			/*被访问次数*/
	uint32_t				hash;			/*key的hash值*/
	uint8_t					data[1];		/*数据体，key + value*/
}cache_item_t;

/*初始化lru cache*/
void				create_cache(size_t max_size);
/*销毁lru cache*/
void				destroy_cache();
/*通过key查找一个item*/
cache_item_t*		get_cache(const uint8_t* key, size_t ksize);
/*update也在这个函数中做，如果已经存在会将老的item返回回来，在外面release*/
void				insert_cache(const uint8_t* key, size_t ksize, const uint8_t* value, size_t vsize);
/*用于释放item->refcount*/
void				release_cache(cache_item_t* item);
/*从lru_cache中删除一个key-value,metaserver一般不会主动调用*/
void				erase_cache(const uint8_t* key, size_t ksize);
/*对lru_cache的打印*/
void				print_cache();

#endif



