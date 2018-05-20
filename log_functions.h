#include <stdio.h>


int create_log_file(char *path);

void log_data(int level, const char *format, ...);

void *clear_old_logs(void *path);

void close_log_file();
