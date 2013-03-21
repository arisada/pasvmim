#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>

struct sockaddr_in *address;
FILE *connection;
struct sockaddr_in sa;

int get_socket(char *hostname)
{
    struct hostent *hp;

    if ((hp = gethostbyname(hostname)) == NULL) {
	errno = ECONNREFUSED;
	return -1;
    }
    memset(&sa, 0, sizeof(sa));

    memcpy((char *) &sa.sin_addr, hp->h_addr, hp->h_length);

    sa.sin_family = hp->h_addrtype;
    return 0;
}

int create_socket(int portnum)
{
    int s;
    sa.sin_port = htons((u_short) portnum);
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	return (-1);
    if (connect(s, (struct sockaddr *) &sa, sizeof(struct sockaddr_in)) <
	0) {
	close(s);
	return (-1);
    }
    return (s);
}

extern int verbose;

int readline(char *dest)
{
    char *ptr = fgets(dest, 4096, connection);
    if (ptr == NULL) {
	printf("*** Error : Remote host closed connection\n");
	perror("fgets");
	exit(-1);
    }
    if ((ptr = strchr(dest, '\n')))
	*ptr = 0;
    if ((ptr = strchr(dest, '\r')))
	*ptr = 0;
    return strlen(dest);
}

int log_ftp(char *hostname, int port, char *user, char *password)
{

    char *buffer = malloc(4096);
    fd_set read_set;
    int fd;
    struct timeval seconds = { 3, 0 };
    fd = get_socket(hostname);
    if (fd) {
	printf("*** Unable to resolve %s\n", hostname);
	return 0;
    }
    fd = create_socket(port);
    if (fd <= 0) {		/* fucked */
	printf("*** Error connecting to %s:%d\n", hostname, port);
	perror("connect");
	exit(-1);
    }
    printf("*** Connection open on %s:%d\n", inet_ntoa(sa.sin_addr), port);
    connection = fdopen(fd, "rw");
    while (1) {
	readline(buffer);
	if (verbose)
	    printf("*** banner : %s\n", buffer);
	if (strstr(buffer, "220 "))
	    break;
    }
    snprintf(buffer, 256, "USER %s\r\n", user);
    write(fd, buffer, strlen(buffer));
    if (readline(buffer) == 0) {
	printf("*** Remote host closed connection\n");
	exit(-1);
    }
    if (verbose)
	printf("*** welcome : %s\n", buffer);
    snprintf(buffer, 256, "PASS %s\r\n", password);
    write(fd, buffer, strlen(buffer));

    if (readline(buffer) == 0) {
	printf("*** Remote host closed connection");
	exit(-1);
    }
    if (verbose)
	printf("*** welcome : %s\n", buffer);
    if (strstr(buffer, "230")) {
	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	while (select(fd + 1, &read_set, NULL, NULL, &seconds) != 0) {
	    read(fd, buffer, 4096);
	    if (verbose)
		printf("* : %s\n", buffer);
	}
	free(buffer);

	return fd;
    }
    printf("*** Login failed\n");
    exit(-1);
}

int ftp_pasv(int fd)
{
    int written;
    int code;
    int port;
    int a, b, c, d, e, f;
    char *p;
    static char *pasv = "PASV\r\n";
    static char buffer[4096];
    written = write(fd, pasv, strlen(pasv));
    if (!readline(buffer) || !written) {
	printf("*** Remote host closed connection\n");
	exit(0);
    }
    code = atoi(buffer);
    if (code != 227) {
	printf("Strange result ... : \"%s\"\n", buffer);
	exit(0);
    }
    p = strchr(buffer, '(');
    if (!p) {
	printf("Strange result ... : \"%s\"\n", buffer);
	exit(0);
    }
    if (sscanf(p, "(%d,%d,%d,%d,%d,%d)", &a, &b, &c, &d, &e, &f) != 6) {
	printf("Strange result ... : \"%s\"\n", buffer);
	exit(0);
    }
    port = (e << 8) + f;
    return port;
}
