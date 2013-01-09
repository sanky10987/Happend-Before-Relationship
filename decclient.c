
static char svnid[] = "$Id: soc.c 6 2009-07-03 03:18:54Z kensmith $";

#define	BUF_LEN	8192

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netdb.h>
#include	<netinet/in.h>
#include	<inttypes.h>
#include <pthread.h>

char *progname;
char buf[BUF_LEN];

void usage();
int setup_client();
int setup_server();
void processRequests(int);
void* handleRequest(char str[], int);

int s, sock, ch, help, done, bytes, aflg;
int soctype= SOCK_STREAM;
char *host= NULL;
char *port= NULL;
extern char *optarg;
extern int optind;
int x[100][100];
//int newSock;
volatile fd_set the_state;
pthread_mutex_t mutex_state= PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {
	fd_set ready;
	struct sockaddr_in msgfrom;
	int msgsize;
	union {
		uint32_t addr;
		char bytes[4];
	} fromaddr;

	if ((progname = rindex(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;
	while ((ch = getopt(argc, argv, "hs:p:")) != -1)
		switch (ch) {
		case 'h':
			usage();
			break;
		case 's':
			host = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	if (argc != 0)
		usage();
	if (help==1 || host == NULL || port == NULL)
		usage();
	
	/*
	 * Create socket on local host.
	 */
	if ((s = socket(AF_INET, soctype, 0)) < 0) {
		perror("socket");
		exit(1);
	}
		sock = setup_client();
	/*
	 * Set up select(2) on both socket and terminal, anything that comes
	 * in on socket goes to terminal, anything that gets typed on terminal
	 * goes out socket...
	 */
	while (!done) {
		FD_ZERO(&ready);
		FD_SET(sock, &ready);
		FD_SET(fileno(stdin), &ready);
		if (select((sock + 1), &ready, 0, 0, 0) < 0) {
			perror("select");
			exit(1);
		}
		if (FD_ISSET(fileno(stdin), &ready)) {
			if ((bytes = read(fileno(stdin), buf, BUF_LEN)) <= 0)
				done++;
			send(sock, buf, bytes, 0);
		}
		msgsize = sizeof(msgfrom);
		if (FD_ISSET(sock, &ready)) {
			if ((bytes = recvfrom(sock, buf, BUF_LEN, 0,
					(struct sockaddr *)&msgfrom, &msgsize)) <= 0) {
				done++;
			} else if (aflg) {
				fromaddr.addr = ntohl(msgfrom.sin_addr.s_addr);
				fprintf(stderr, "%d.%d.%d.%d: ", 0xff & (unsigned int)fromaddr.bytes[0],
				0xff & (unsigned int)fromaddr.bytes[1],
				0xff & (unsigned int)fromaddr.bytes[2],
				0xff & (unsigned int)fromaddr.bytes[3]);
			}
			write(fileno(stdout), buf, bytes);
			//printf("buffer contents %s", buf);
		}
	}
	return (0);
}

/*
 * setup_client() - set up socket for the mode of soc running as a
 *		client connecting to a port on a remote machine.
 */

int setup_client() {

	struct hostent *hp, *gethostbyname();
	struct sockaddr_in serv;
	struct servent *se;

	/*
	 * Look up name of remote machine, getting its address.
	 */
	if ((hp = gethostbyname(host)) == NULL) {
		fprintf(stderr, "%s: %s unknown host\n", progname, host);
		exit(1);
	}
	/*
	 * Set up the information needed for the socket to be bound to a socket on
	 * a remote host.  Needs address family to use, the address of the remote
	 * host (obtained above), and the port on the remote host to connect to.
	 */
	serv.sin_family = AF_INET;
	memcpy(&serv.sin_addr, hp->h_addr, hp->h_length);
	if (isdigit(*port))
		serv.sin_port = htons(atoi(port));
	else {
		if ((se = getservbyname(port, (char *)NULL)) < (struct servent *) 0) {
			perror(port);
			exit(1);
		}
		serv.sin_port = se->s_port;
	}
	/*
	 * Try to connect the sockets...
	 */
	if (connect(s, (struct sockaddr *) &serv, sizeof(serv)) < 0) {
		perror("connect");
		exit(1);
	} else
		fprintf(stderr, "Connected...\n");
	return (s);
}
/*
 * usage - print usage string and exit
 */

void usage() {
	//fprintf(stderr, "usage: %s -h host -p port\n", progname);
	printf("[−h] [-s server-host] [-p port-number]\n");
	printf("−h : Print a usage summary with all options and exit.\n"); 
	
	printf("-s server-host : Connect to the specified host (by hostname or by IP address). If not provided, connect to the localhost.");
	  printf(": Connect to the server at the given port. If not provided, connect to 9090.\n");
	printf("-p port-number : Connect to the server at the given port. If not provided, connect to 9090.\n");

	exit(1);
}