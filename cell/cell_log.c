#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include "cell_log.h"

#define PATH_MAX_SIZE 1024
#define MAX_LOG_COUNT (1024 * 1024)

#define LOCK(q) while(__sync_lock_test_and_set(q, 1)){}
#define UNLOCK(q) __sync_lock_release(q)

const char* level_info[] = {
	"[debug]",
	"[info]",
	"[warning]",
	"[error]",
	"[fatal]",
};

typedef struct log_file_s
{
	FILE*			fp;
	int32_t			level;
	int32_t			count;
	char			filename[PATH_MAX_SIZE];
	int				mutex;
}log_file_t;

log_file_t log_file;

void init_log()
{
	/*pthread_mutex_init(&(log_file.mutex), NULL);*/
	log_file.mutex = 0;
	log_file.fp = 0;
	log_file.count = 0;
	log_file.level = 0;
	memset(log_file.filename, 0x00, PATH_MAX_SIZE);
}

int32_t open_log(const char* filename, int32_t l)
{
	if(filename == NULL || log_file.fp != NULL)
		return -1;

	strcpy(log_file.filename, filename);
	log_file.fp = fopen(filename, "a");
	if(log_file.fp == NULL){
		printf("open %s failed!\r\n", filename);
		return -1;
	}

	log_file.level = l;
	log_file.count = 0;

	return 0;
}

void close_log()
{
	if(log_file.fp != NULL){
		fflush(log_file.fp);
		fclose(log_file.fp);

		log_file.fp = NULL;
	}
}

char* log_get_file(const char* file)
{
	if(file != NULL){
		size_t len = strlen(file);
		char* pos = (char*)file + len;
		while(*pos != '/' && pos != file)
			pos --;

		if(*pos == '/')
			pos ++;

		return pos;
	}

	return NULL;
}

/*获得时间*/
char* unix_time_2_datetime(int32_t ts, char* date_str, size_t date_size)
{
	time_t t = ts;
	strftime(date_str, date_size, "%Y-%m-%d %H:%M:%S", localtime(&t));
	return date_str;
}

static char* get_log_rename(int ts, char* date_str, size_t date_size)
{
	time_t t = ts;
	strftime(date_str, date_size, "%Y-%m-%d-%H-%M-%S", localtime(&t));
	return date_str;
}

void print_log(int32_t l, const char* file, int32_t line, const char* format, ...)
{
	if(log_file.fp != NULL && l < LEVEL_FATAL && l >= 0){
		char date_str[64] = {0};

		LOCK(&(log_file.mutex));

		if(log_file.count > MAX_LOG_COUNT){
			char file_name[PATH_MAX_SIZE];

			close_log();

			get_log_rename(time(NULL), date_str, 64);
			sprintf(file_name, "%s-%s", log_file.filename, date_str);
			rename(log_file.filename, file_name);

			open_log(log_file.filename, log_file.level);
		}

		unix_time_2_datetime(time(NULL), date_str, 64);

		fprintf(log_file.fp, "%s %s [%s-%d] ", level_info[l], date_str, log_get_file(file), line);
		
		va_list args;
		va_start(args, format);
		vfprintf(log_file.fp, format, args);
		va_end(args);
		fprintf(log_file.fp, "\n");
		++ log_file.count;
		UNLOCK(&(log_file.mutex));

		fflush(log_file.fp);
	}
}
