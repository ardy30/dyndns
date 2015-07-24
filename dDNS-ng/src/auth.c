#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#define _XOPEN_SOURCE
#include <unistd.h>
#ifndef __FreeBSD__
#define _GNU_SOURCE
#include <crypt.h>
#endif
#include "common.h"
#include "auth.h"
#include "clientmanager.h"

int userauth(DB_USERDATA_t *dbdata, char *pass) {
	char * salt;

	salt = (char *) malloc(16 * sizeof(char));

	get_salt(dbdata->md5, salt);
	if(strcmp((char *) dbdata->md5, (char *) crypt(pass, salt)) != 0) {
		free(salt);
		return 0;
	}
	free(salt);
	return 1;
}
void get_salt(char *p, char *salt) {
	int delim = 0;

	while(delim < 3) {
		if(*p == '$')
			delim++;
		*salt++ = *p++;
	}
	*salt = '\0';
}
int isAuthorized(sqldata_t *dbdata, char *user, char *domain) {

	if(strcmp(user, dbdata->login) != 0)
		return -1;
	if(strcmp(domain, dbdata->subdomain) == 0)
		return 1;
	else
		return 0;
}
