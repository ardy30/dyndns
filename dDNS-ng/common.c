#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include "common.h"

bool ReadCFG(config_t * cfg, char * filename) {
	FILE * cfgfile;
	char opt[50];
	char val[50];
	char buf[256];

	cfgfile = fopen(filename, "r");
	if(cfgfile == NULL)
		return false;
	while(fgets(buf, sizeof(buf), cfgfile) != NULL) {
		if(buf[0] == '#')
			continue;
		if(strstr(buf, "server ="))
			strcpy(cfg->client.host, getVal(buf));
		if(strstr(buf, "port ="))
			cfg->port = atoi(getVal(buf));
		if(strstr(buf, "domain ="))
			strcpy(cfg->client.domain, getVal(buf));
		if(strstr(buf, "zones ="))
			strcpy(cfg->server.zonedir, getVal(buf));
		if(strstr(buf, "log ="))
			strcpy(cfg->logfile, getVal(buf));
		if(strstr(buf, "interval ="))
			cfg->client.interval = atoi(getVal(buf));
		if(strstr(buf, "login ="))
			strcpy(cfg->client.username, getVal(buf));
		if(strstr(buf, "pass ="))
			strcpy(cfg->client.password, getVal(buf));
		if(strstr(buf, "pid ="))
			strcpy(cfg->pid, getVal(buf));
	}
	fclose(cfgfile);
	return true;
}
char * getVal(char * str) {
	char * newstr = str;

	while(*str) {
		newstr = ++str;
		if(isspace(*str) && (isalpha(*(str+1)) || isdigit(*(str+1)) || *(str+1) == '/'))
			break;
	}
	str = newstr;
	*(str+(strlen(str)-1)) = '\0';
	return ++str;
}
void log_event(int logfd, char *first, ...) {
	va_list msgs;
	char *next = first;
	extern char logmsg[LOG_MSG_LEN];
	extern char t_stamp[TIMESTAMP_LEN];

	va_start(msgs, first);
	strcpy(logmsg, timestamp_new(t_stamp));
	while(next != NULL) {
		strcat(logmsg, next);
		next = va_arg(msgs, char *);
	}
	va_end(msgs);
	write(logfd, logmsg, strlen(logmsg));
}
char * timestamp_new(char * t_stamp) {
    time_t sec;
    struct tm st_time;

    time(&sec);
    localtime_r(&sec, &st_time);
    strftime(t_stamp, TIMESTAMP_LEN * sizeof(t_stamp), "[%d/%B/%Y %T]", &st_time);

    return t_stamp;
}
int pidfile(pid_t pid, char *path) {
	int pfd;
	char buf[16];

	sprintf(buf, "%d", (int) pid);
	pfd = open(path, O_CREAT|O_RDWR, 0644);
	write(pfd, buf, strlen(buf)+1);
	
	close(pfd);
	return pfd;
}
