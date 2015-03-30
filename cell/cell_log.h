#ifndef __CELL_LOG_H_
#define __CELL_LOG_H_

#include <stdint.h>

/*日志级别*/
enum {
	LEVEL_DEBUG,
	LEVEL_INFO,
	LEVEL_WARN,
	LEVEL_ERROR,
	LEVEL_FATAL
};


void		init_log();
int32_t		open_log(const char* filename, int32_t l);
void		close_log();
void		print_log(int32_t l, const char* file, int32_t line, const char* format, ...);
char*		unix_time_2_datetime(int32_t ts, char* date_str, size_t date_size);

#define OPEN_LOG() open_ui_log(LEVEL_DEBUG)
#define CLOSE_LOG() close_ui_log()

#define log_debug(...) \
	print_log(LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

#define log_info(...) \
	print_log(LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)

#define log_warn(...) \
	print_log(LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)

#define log_error(...) \
	print_log(LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#define log_fatal(...) \
	print_log(LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif


