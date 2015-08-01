#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <mysql.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "clientmanager.h"

static char * getdata(char *);

void InitConnData(REMOTEDATA_t * conn) {
	conn->client_ip_addr = NULL;
	conn->login = NULL;
	conn->pass = NULL;
	conn->subdomain = NULL;
}
void InitDBData(DB_USERDATA_t * db) {
	db->active = 0;
	db->email = NULL;
	db->id = 0;
	db->login = NULL;
	db->md5 = NULL;
	db->subdomain = NULL;
}
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
REMOTEDATA_t * readCLientData(int sockfd, int logger) {
	int clifd;
	REMOTEDATA_t * data;
	struct conn_st * conn_data;

	char * welcome = "dDNS-ng server ready. Go ahead\n";
	char * unknown = "Unknown command\n";
	char * ack = "ACK\n";
	char * val;

	char readbuf[256];

	data = (REMOTEDATA_t *) malloc(sizeof(REMOTEDATA_t));
	bzero(readbuf, 256);

	if((conn_data = clientConn(sockfd)) == NULL) {
		log_event(logger, " ERROR: can't establish connection\n", NULL);
		return NULL;
	}
	data->client_ip_addr = conn_data->client_ip;

	write(conn_data->fd, welcome, strlen(welcome) +1);
	bzero(readbuf, 256);
	while(read(conn_data->fd, readbuf, 256) > 0) {
		val = getdata(readbuf);
		if(strstr(readbuf, "QUIT") != NULL)
			break;
		else if(strstr(readbuf, "LOGIN") != NULL) {
			data->login = (char *) malloc((strlen(val) + 1) * sizeof(char));
			bzero(data->login, sizeof(data->login));
			strcpy(data->login, val);
		}
		else if(strstr(readbuf, "PASS") != NULL) {
			data->pass = (char *) malloc((strlen(val) + 1) * sizeof(char));
			bzero(data->pass, sizeof(data->pass));
			strcpy(data->pass, val);
		}
		else if(strstr(readbuf, "SUBDOMAIN") != NULL) {
			data->subdomain = (char *) malloc((strlen(val) + 1) * sizeof(char));
			bzero(data->subdomain, sizeof(data->subdomain));
			strcpy(data->subdomain, val);
		}
		else {
			write(conn_data->fd, unknown, strlen(unknown)+1);
			continue;
		}
		write(conn_data->fd, ack, strlen(ack)+1);
		free(val);
		bzero(readbuf, 256);
	}
	close(conn_data->fd);
	return data;
}
struct conn_st * clientConn(int sock) {
	int clilen, clifd;
	struct sockaddr_in cli_addr;
	struct conn_st * conn;

	clilen = sizeof(cli_addr);
	clifd = accept(sock, (struct sockaddr *) &cli_addr, &clilen);

	if(clifd < 0)
		return NULL;
	else {
		conn->client_ip = (char *) malloc((strlen((char *) inet_ntoa(cli_addr.sin_addr)) + 1) * sizeof(char));
		strcpy(conn->client_ip, (char *) inet_ntoa(cli_addr.sin_addr));
		conn->fd = clifd;
		return conn;
	}
}
MYSQL_RES * queryUserData(MYSQL * dbh, char * login, int logger) {
	MYSQL_RES * res;
	char * query_prefix = "SELECT u.id, u.login, u.pass, CONCAT(s.subdomain, \".\",  d.domain) as subdomain, \
			u.email, u.active, s.dynamic FROM subdomains s, domains d , users u \
			WHERE s.domain_id = d.id and u.id = s.user_id and u.login = '";
	char * query_suffix = "' AND dynamic = 1";
	char * query;

/*
 * build SQL query to get user account information
 */
	query = (char *) malloc((strlen(query_prefix) + strlen(login) + strlen(query_suffix) + 3) * sizeof(char));
	strcpy(query, query_prefix);
	strcat(query, login);
	strcat(query, query_suffix);
/*
 * send query to MySQL server
 */
	if(mysql_query(dbh, query) != 0) {
		log_event(logger, " SQLERR query: ", query, " failed!\n", NULL);
		free(query);
		return NULL;
	}
/*
 * retrieve sql data from database
 */

	if((res = mysql_store_result(dbh)) == NULL || mysql_num_rows(res) == 0)
		return NULL;

	free(query);
	return res;
}
static char * getdata(char * buf) {
	char * val;
	char * curr;
	int len = 0;
	int i;

	while(*buf++)
		if(isalnum(*buf) && isspace(*(buf+1))) {
			buf++;
			break;
		}
	curr = ++buf;
	while(*buf++ != '\n')
		len++;
	val = (char *) malloc((len+1) * sizeof(char));
	for(i = 0; i < len; i++)
		val[i] = curr[i];
	val[strlen(val)] = '\0';

	return val;
}
char * stripDomain(char * subdomain) {
	char * domain;
	char * cur;

	size_t len = 0;
	int i;

	cur = strchr(subdomain, '.');
	cur++;
	subdomain = cur;
	while(*subdomain++)
		len++;
	domain = (char *) malloc((len + 1) * sizeof(char));
	for(i = 0; i < len; i++)
		domain[i] = cur[i];
	domain[i] = '\0';

	return domain;
}
struct subdomain_st * explodeDomain(char * fulldomain) {
	struct subdomain_st * name;
	int sub_len = 0;				// subdomain string lenght
	int dom_len = 0;				// domain string lenght
	char * cur;						// actual position inside the string
	int i;

	cur = fulldomain;
	while(*cur++ != '.')
		sub_len++;
	name->sub = (char *) malloc((sub_len+1) * sizeof(char));
	for(i = 0; i < sub_len; i++)
		name->sub[i] = fulldomain[i];
	name->sub[i] = '\0';

	cur = strchr(fulldomain, '.');
	cur++;
	fulldomain = cur;
	while(*cur++)
		dom_len++;
	name->dom = (char *) malloc((dom_len + 1) * sizeof(char));
	for(i = 0; i < dom_len; i++)
		name->dom[i] = fulldomain[i];
	name->dom[i] = '\0';

	return name;
}
int existZoneFile(char * filepath) {
	FILE * zf;

	if((zf = fopen(filepath, "r")) == NULL)
		return 0;
	else {
		fclose(zf);
		return 1;
	}
}
int existEntry(char * what, char * where) {
	char * buf;
	FILE * zf;

	buf = (char *) malloc(256 * sizeof(char));
	if((zf = fopen(where, "r")) == NULL)
		return -1;			// this shouldn't happen
	while(fgets(buf, sizeof(buf), zf) != NULL)
		if(strstr(buf, what) != NULL) {
			free(buf);
			return 1;		// return success if string found in buffer
		}
	free(buf);
	return 0;
}
char * stripSerialNo(char * input) {
	char * serial;

	serial = (char *) malloc(13 * sizeof(char));
	while(*input) {
		if(isdigit(*input)) {
			*serial = *input;
			serial++;
		}
		input++;
	}
	*serial = '\0';

	return serial;
}
char * newSerialNo(char * serial) {
    time_t today;
    struct tm tf;
    char * newserial;
    char * ver = "00";
    long bigger_serial = 0;

    newserial = (char *) malloc((strlen(serial)+1) * sizeof(char));
    time(&today);
    localtime_r(&today, &tf);
    strftime(newserial, sizeof(newserial), "%Y%m%d", &tf);
    strcat(newserial, ver);

    if(atol(serial) >= atol(newserial)) {
            bigger_serial = atol(serial) + 1;
            sprintf(newserial, "%ld", bigger_serial);
            strcpy(newserial, serial);
    }
    else
    	strcpy(newserial, serial);

    return newserial;
}
int updateZone(char * subdomain, char * ipaddr, char * file, int logger) {
    FILE *zf;
    FILE *tmp;

    char * buf;
    char * serial;
    char * newserial;

    buf = (char *) malloc(256 * sizeof(char));

    zf = fopen(file, "r+");
    if(zf == NULL) {
    	log_event(logger, "Error: Unable to open: ", file, "\n", NULL);
        return 0;
    }

    tmp = tmpfile();
    if(tmp == NULL) {
    	log_event(logger, "Error: Unable to create tmpfile\n", NULL);
    	return 0;
    }

    while(fgets(buf, sizeof(buf), zf) != NULL) {
	    if(strstr(buf, "; serial") != NULL) {
	    	serial = stripSerialNo(buf);
	    	newserial = newSerialNo(serial);
	    	sprintf(buf, "\t%s\t; serial\n", newserial);
	    }
	    if(strstr(buf, subdomain) != NULL) {
		    if(strlen(subdomain) < 8)
		    	sprintf(buf, "%s\t300\tIN\tA\t%s\n", subdomain, ipaddr);
		    else
		    	sprintf(buf, "%s\t300\tIN\tA\t%s\n", subdomain, ipaddr);
	    }
	    fputs(buf, tmp);
    }
    free(serial);
    free(newserial);
    bzero(buf, sizeof(buf));
    rewind(zf);
    rewind(tmp);
    while(fgets(buf, sizeof(buf), tmp) != NULL)
    	fputs(buf, tmp);

    fclose(zf);
    fclose(tmp);
    free(buf);
//    apply(tmp, file, cf->domain);

    return 1;
}
