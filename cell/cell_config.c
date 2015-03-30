#include "cell_config.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include<ctype.h>

#ifdef WIN32
#define atoll _atoi64
#endif

cell_conf_t* cell_config = NULL;

typedef struct conf_item_s
{
	char*	key;
	char*	value;
}conf_item_t;

static char* strtrimr(char* pstr)
{
	int i;
	i = strlen(pstr) - 1;
	while(isspace(pstr[i]) && i >= 0)
		pstr[i --] = '\0';

	return pstr;
}

static char* strtriml(char* pstr)
{
	int len, i = 0;
	len = strlen(pstr) - 1;
	while(isspace(pstr[i]) && (i <= len))
		i ++;

	if(i > 0)
		strcpy(pstr, &pstr[i]);

	return pstr;
}

static char* strtrim(char* pstr)
{
	return strtriml(strtrimr(pstr));
}

/*获取一个item*/
static int get_conf_item(char* line, conf_item_t* item)
{
	char *p = strtrim(line);
	int len = strlen(p);
	if(len <= 0)
		return -1;
	else if(p[0] == '#') /*却掉行注释*/
		return -2;
	else{
		char* pos;
		char* split_pos = strchr(p, '=');
		if(split_pos == NULL)
			return -3;

		*split_pos ++ = '\0';
		item->key = strtrim(p);
		
		pos = split_pos;
		while(*pos != '\0'){
			if(*pos == '#') /*去掉后面的注释*/
				*pos = '\0';
			pos ++;
		}
		item->value = strtrim(split_pos);
		if(strlen(item->value) == 0) /*value不能为""空串*/
			return -3;
	}

	return 0;
}

static void get_db_slaves(char* v)
{
	int i = 0;
	char* start = v;
	char* pos = v;
	int size;

	assert(v != NULL);

	while(*pos != '\0'){
		size = pos - start;
		if(*pos == ',' && size > 0 && i < DB_SLAVE_N){
			char* slave = calloc(1, size + 1);
			memcpy(slave, start, size);
			cell_config->db_slaves[i] = slave;

			start = pos + 1;
			i ++;
		}
		pos ++;
	}

	size = pos - start;
	if(size > 0 && i < DB_SLAVE_N){
		char* slave = calloc(1, size + 1);
		memcpy(slave, start, size);
		cell_config->db_slaves[i] = slave;
	}
}

void load_config(const char* config_file)
{
	FILE* fp;
	char* line = NULL;
	size_t len;
	size_t read;
	conf_item_t item;

	assert(config_file != NULL);

	fp = fopen(config_file, "r");
	if(fp == NULL){
		printf("open config file failed! config file = %s\n", config_file);
		exit(-1);
	}

	cell_config = calloc(1, sizeof(cell_conf_t));
	if(cell_config == NULL){
		printf("malloc cell config failed!\n");
		exit(-1);
	}

	/*getline只有linux下才有*/
	while(read = getline(&line, &len, fp) != -1){
		if(get_conf_item(line, &item) != 0){
			continue;
		}

		/*对各项配置进行读取*/
		if(strcmp(item.key, "thread num") == 0){
			cell_config->thread_n = atoi(item.value);
			if(cell_config->thread_n == 0)
				cell_config->thread_n = 2;
		}
		else if(strcmp(item.key, "name") == 0)
			strncpy(cell_config->name, item.value, VALUE_SIZE);
		else if(strcmp(item.key, "server ip") == 0)
			strncpy(cell_config->listen_ip, item.value, VALUE_SIZE);
		else if(strcmp(item.key, "server port") == 0){
			cell_config->listen_port = atoi(item.value);
			if(cell_config->listen_port == 0)
				cell_config->listen_port = 3200;
		}
		else if(strcmp(item.key, "cache size") == 0)
			cell_config->cache_size = atoll(item.value);
		else if(strcmp(item.key, "zookeeper host") == 0)
			strncpy(cell_config->zk_host, item.value, PATH_SIZE);
		else if(strcmp(item.key, "db user") == 0)
			strncpy(cell_config->user, item.value, VALUE_SIZE);
		else if(strcmp(item.key, "db passwd") == 0)
			strncpy(cell_config->passwd, item.value, VALUE_SIZE);
		else if(strcmp(item.key, "db name") == 0)
			strncpy(cell_config->db_name, item.value, VALUE_SIZE);
		else if(strcmp(item.key, "db master") == 0)
			strncpy(cell_config->db_master, item.value, VALUE_SIZE);
		else if(strcmp(item.key, "db slaves") == 0)
			get_db_slaves(item.value);
		else if(strcmp(item.key, "db port") == 0){
			cell_config->db_port = atoi(item.value);
			if(cell_config->db_port == 0)
				cell_config->db_port = 3306;
		}
		else if(strcmp(item.key, "log") == 0)
			strncpy(cell_config->log_path, item.value, PATH_SIZE);
		else
			printf("config's item error, key = %s, value = %s\n", item.key, item.value);
	}

	if(line != NULL)
		free(line);

	fclose(fp);
}

void close_config()
{
	int i;
	if(cell_config == NULL)
		return ;

	for(i = 0; i < DB_SLAVE_N; i ++){
		if(cell_config->db_slaves[i] != NULL){
			free(cell_config->db_slaves[i]);
			cell_config->db_slaves[i] = NULL;
		}
	}

	free(cell_config);
	cell_config = NULL;
}

void print_config()
{
	int i;
	if(cell_config == NULL)
		return ;

	printf("meta server config:\n");
	printf("\tthread num = %d\n", cell_config->thread_n);
	printf("\tserver name = %s\n", cell_config->name);
	printf("\tserver ip = %s\n", cell_config->listen_ip);
	printf("\tserver port = %d\n", cell_config->listen_port);
	printf("\tcache size = %ld\n", cell_config->cache_size);
	printf("\tzookeeper host = %s\n", cell_config->zk_host);
	printf("\tlog = %s\n", cell_config->log_path);
	printf("\tdb user = %s\n", cell_config->user);
	printf("\tdb passwd = %s\n", cell_config->passwd);
	printf("\tdb name = %s\n", cell_config->db_name);
	printf("\tdb master = %s\n", cell_config->db_master);
	printf("\tdb slaves:");
	for(i = 0; i < DB_SLAVE_N; i++){
		if(cell_config->db_slaves[i] != NULL)
			printf("%s,", cell_config->db_slaves[i]);
	}
	printf("\n\tdb port = %d\n", cell_config->db_port);
}

