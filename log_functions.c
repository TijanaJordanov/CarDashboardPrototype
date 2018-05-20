#include "log_functions.h"
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>


FILE *log_file;			//fajl za logovanje


/*
	Funkcija: create_log_file    
	Kreira log fajl 

	Argumenti:
		path - putanja do fajla

	Povratna vrednost: 
		0 - ok
		-1 - greska
 */
int create_log_file(char *path)
{
    char file_name[256], file_date[16];

    time_t t;
    time(&t);

    struct tm *time_data = localtime(&t);
    strftime(file_date, 16, "%d%m%Y", time_data);

    sprintf(file_name, "%sdashboard%s.log", path, file_date);

    //printf("path = %s\n", file_name);

    log_file = fopen(file_name, "a");

    if (log_file == NULL)
		return -1;

    return 0;
}


/*
	Funkcija: log_data    
	Upisuje poruke u log fajl

	Argumenti:
		level - nivo logovanja. Raspolozive vrednosti: 
			0 - informacija
			1 - upozorenje
			2 - greska
		format - poruka koju treba upisati u fajl

	Povratna vrednost: nema
 */
void log_data(int level, const char *format, ...)
{

    if (log_file == NULL)
		return;

    char log_time[64], buffer[2048];

    time_t t;
    time(&t);

    struct tm *time_data = localtime(&t);
    strftime(log_time, 256, "%d/%m/%Y - %H:%M:%S  ", time_data);

    fprintf(log_file, log_time);

    if (level == 1)
		fprintf(log_file, "WARNING: ");
    else if (level == 2)
		fprintf(log_file, "ERROR: ");

    va_list argptr;
    va_start(argptr, format);
    vsprintf(buffer, format, argptr);
    va_end(argptr);

    fprintf(log_file, buffer);
}


/*
	Funkcija: clear_old_logs    
	Brise stare ili prevelike log fajlove

	Argumenti:
		path - putanja do fajla

	Povratna vrednost: NULL
 */
void *clear_old_logs(void *path)
{
    char command[256];

    sprintf(command, "find %s -type f  \\( -mtime +30 -or -size +10M \\) -delete", (char *) path);

    system(command);

    log_data(0, "logs DELETED\n");

    return NULL;
}


/*
	Funkcija: close_log_file    
	Zatvara log fajl

	Argumenti: nema

	Povratna vrednost: nema
 */
void close_log_file()
{

    if (log_file == NULL)
		return;

    fclose(log_file);
}
