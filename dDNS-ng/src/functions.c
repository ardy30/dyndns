#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <mysql.h>
#include "dynsrv.h"
#include "auth.h"
#include "common.h"

int bindToInterface(int portno) {
	int sockfd;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		perror("ERROR opening socket");
		return -1;
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("ERROR on binding");
		return -1;
	}
	listen(sockfd, 5);

	return sockfd;
}
int clientConn(int fd, char * cliaddr) {
	int clilen, clifd;
	struct sockaddr_in cli_addr;

	clilen = sizeof(cli_addr);
	clifd = accept(fd, (struct sockaddr *) &cli_addr, &clilen);

	if(clifd < 0)
		return -1;
	else {
		strcpy(cliaddr, (char *) inet_ntoa(cli_addr.sin_addr));
		return clifd;
	}
}
int updateZone(cfgdata_t * cf, char * file) {
    FILE *zf;
    FILE *tmp;
    char tmp_path[21];
    char buf[256];
    char serial[12];
    char newserial[12];

    zf = fopen(file, "r");
    if(zf == NULL) {
            fprintf(stderr, "Error reading file: %s", file);
            return 0;
    }
    RandomFilename(tmp_path);
    tmp = fopen(tmp_path, "w");
    if(tmp == NULL) {
    	fprintf(stderr, "Error creating file: %s", tmp_path);
    	return 0;
    }
    while(fgets(buf, sizeof(buf), zf) != NULL) {
	    if(strstr(buf, "; serial") != NULL) {
			stripSerialNo(buf, serial);
			if(!updateSerialNo(serial, newserial))
				sprintf(buf, "\t%s\t; serial\n", newserial);
	    }
	    if(strstr(buf, cf->subdomain) != NULL) {
		    if(strlen(cf->subdomain) < 8)
		    	sprintf(buf, "%s\t300\tIN\tA\t%s\n", cf->subdomain, cf->ip_addr);
		    else
		    	sprintf(buf, "%s\t300\tIN\tA\t%s\n", cf->subdomain, cf->ip_addr);
	    }
	    fputs(buf, tmp);
    }
    fclose(zf);
    fclose(tmp);
    apply(tmp_path, file, cf->domain);

    return 1;
}
int updateSerialNo(char * oldserial, char * newserial) {
    time_t today;
    struct tm tf;
    char serial[12];
    char * ver = "00";
    long bigger_serial = 0;

    time(&today);
    localtime_r(&today, &tf);
    strftime(serial, sizeof(serial), "%Y%m%d", &tf);
    strcat(serial, ver);

    if(atol(oldserial) >= atol(serial)) {
            bigger_serial = atol(oldserial) + 1;
            sprintf(serial, "%ld", bigger_serial);
            strcpy(newserial, serial);
            return 0;
    }
    else {
    	strcpy(newserial, serial);
    	return 0;
    }
}
void RandomFilename(char *filename) {
    char * entropy = "1qaz2wsx3edc4rfv5tgb6yhn7ujm8i0poklaZAQ1XSW2CDE3VFR4BGT5NHY6MJU6MJU7";
    int n, i;
    const int len = 8;                              // random part lenght
    struct timeval tv;
    char tmp[20] = "/tmp/dyndns_";                  // prefix of temp file
    char * ptmp;                                    // pointer to the begining of temp filename
    ptmp = tmp + strlen(tmp);                       // moving to the end of a prefix

    for(i=0; i<len; i++, ptmp++) {                  // while counter is smaller then lenght of random part
            gettimeofday(&tv, NULL);                // get current time
            srand(tv.tv_usec);                      // generate new seed with time in micro seconds
            n = rand() % strlen(entropy);           // generate new random character
            *ptmp = entropy[n];                     // and put it in the next field of an array.
    }
    *ptmp = '\0';                                   // end of string containing random filename
    strcpy(filename, tmp);
}
void stripSerialNo(char *in, char *out) {
	char serial[12];
	char * pserial;

	pserial = serial;
	while(*in) {
		if(isdigit(*in)) {
			*pserial = *in;
			pserial++;
		}
		in++;
	}
	*in = '\0';
	strcpy(out, serial);
}
bool if_Exist(char *item, char *zfname) {
	FILE *zf;
	char buf[256];
	int found = 0;

	zf = fopen(zfname, "r");
	if(zf == NULL) {
		fprintf(stderr, "Error opening file: %s\n", zfname);
		return false;
	}
	while(fgets(buf, sizeof(buf), zf) != NULL) {
		if(strstr(buf,item) != NULL) {
			found = 1;
			break;
		}
	}
	fclose(zf);

	if(found)
		return true;
	else
		return false;
}
int NewEntry(cfgdata_t * cf, char * file) {
    FILE *zf;
    FILE *tmp;
    char tmp_path[21];
    char buf[256];
    char serial[12];
    char newserial[12];

    zf = fopen(file, "r");
    if(zf == NULL) {
            fprintf(stderr, "Error reading file: %s", file);
            return 1;
    }
    RandomFilename(tmp_path);
    tmp = fopen(tmp_path, "w");
    if(tmp == NULL) {
    	fprintf(stderr, "Error creating file: %s", tmp_path);
    	return 1;
    }
    while(fgets(buf, sizeof(buf), zf) != NULL) {
	    if(strstr(buf, "; serial") != NULL) {
			stripSerialNo(buf, serial);
			if(!updateSerialNo(serial, newserial))
				sprintf(buf, "\t%s\t; serial\n", newserial);
	    }
	    fputs(buf, tmp);
    }
	if(strlen(cf->subdomain) < 8)
		sprintf(buf, "%s\t300\tIN\tA\t%s\n", cf->subdomain, cf->ip_addr);
	else
		sprintf(buf, "%s\t300\tIN\tA\t%s\n", cf->subdomain, cf->ip_addr);
	fputs(buf, tmp);

	fclose(zf);
    fclose(tmp);

    apply(tmp_path, file, cf->domain);
    return 0;
}
int apply(char * tmp_f, char * dst_f, const char * domain) {
    int tmp, dst;
    struct stat st;
    char * buf;
    int buf_size;
    ssize_t n;
    pid_t reload_pid;
    int status, ret;

	tmp = open(tmp_f, O_RDWR);
	if(tmp < 0) {
			perror("error opening tmp");
			return -1;
	}
	dst = open(dst_f, O_WRONLY|O_TRUNC, 0644);
	if(dst < 0) {
			perror("error writing zonefile");
			return -1;
	}
	fstat(tmp, &st);
	buf_size = (int) st.st_size;
	buf = (char *) malloc(buf_size);
	if(buf == NULL) {
			fprintf(stderr, "Memory allocation failed.\n");
			return -1;
	}
	while(buf_size != 0 && (n = read(tmp, buf, buf_size)) < buf_size) {
			if(n == -1) {
					if(errno == EINTR)
							continue;
					perror("read tmp");
					break;
			}
			buf_size -= n;
			buf += n;
	}
	while(buf_size != 0 && (n = write(dst, buf, buf_size)) < buf_size) {
			if(n == -1) {
					if(errno == EINTR)
							continue;
					perror("write zone");
					break;
			}
			buf_size -= n;
			buf += n;
	}
	close(tmp);
	close(dst);
	free(buf);
	if(unlink(tmp_f) < 0) {
			perror("Warning: error deleting tmpfile");
	}
	reload_pid = fork();
    if(reload_pid == 0) {
    	ret = execl("/usr/sbin/rndc", "rndc", "reload", domain, NULL);
    	if(ret == -1) {
    		ret = execl("/usr/local/sbin/rndc", "rndc", "reload", domain, NULL);
    		if(ret == -1) {
    			perror("execl");
    			exit(-1);
    		}
    	}
    }
    else if(reload_pid > 0)
    	waitpid(reload_pid, NULL, WNOHANG);
    else
    	perror("fork");

    return 1;
}
bool getUserData(config_t * cf, sqldata_t *info, char *login) {
	MYSQL * sql;
	MYSQL_RES * res;
	MYSQL_ROW row;

	char * check_user_query = "SELECT * FROM users WHERE login = ";
	char * query;

	query = (char *) malloc((strlen(check_user_query) + strlen(login) + 3) * sizeof(char));
	strcpy(query, check_user_query);
	strcat(query,"'");
	strcat(query, login);
	strcat(query,"'");
	sql = mysql_init(NULL);
	if(sql == NULL) {
		fprintf(stderr, "Initialization failed\n");
		exit(1);
	}
	if(mysql_real_connect(sql, cf->server.db_host,
			cf->server.db_login, cf->server.db_pass, cf->server.db_name, 3306, NULL, 0) == NULL)
		return false;
	 if(mysql_query(sql, query) != 0) {
	 	return false;
	 }
	 res = mysql_store_result(sql);
	 if(res == NULL) {
	 	return false;
	 }
	 if(mysql_field_count(sql) == 0) {
	 	free(query);
	 	return false;
	 }
	 if((row = mysql_fetch_row(res)) != 0) {
		 strcpy(info->login, row[1]);
		 strcpy(info->pass, row[2]);
		 strcpy(info->subdomain, row[7]);
		 info->isactive = atoi(row[4]);
	 }
	 else
		 return false;

	 free(query);
	 mysql_close(sql);
	 return true;
}
