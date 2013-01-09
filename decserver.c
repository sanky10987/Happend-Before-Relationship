
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
char logpath[1024] = "/log.txt";
int s, sock, ch, server=1, done, bytes, aflg;
int soctype= SOCK_STREAM;
char *host= NULL;
char *port= "9090";
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
	while ((ch = getopt(argc, argv, "hp:l:")) != -1)
		switch (ch) {
		case 'h':
			usage();
			break;
		case 'p':
			port = optarg;
			break;
		case 'l':
			strcpy(logpath,optarg);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	if (argc != 0)
		usage();
	if (!server && (host == NULL || port == NULL))
		usage();
	if (server && host != NULL)
		usage();
	/*
	 * Create socket on local host.
	 */
	if ((s = socket(AF_INET, soctype, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	if (!server)
		sock = setup_client();
	else
		sock = setup_server();
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
 * setup_server() - set up socket for mode of soc running as a server.
 */

int setup_server() {
	struct sockaddr_in serv, remote;
	struct servent *se;
	int threadCreate1_ret, threadJoin_ret, k=0;
	pthread_t threads[50];
	int len;

	len = sizeof(remote);
	memset((void *)&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	if (port == NULL)
		serv.sin_port = htons(0);
	else if (isdigit(*port))
		serv.sin_port = htons(atoi(port));
	else {
		if ((se = getservbyname(port, (char *)NULL)) < (struct servent *) 0) {
			perror(port);
			exit(1);
		}
		serv.sin_port = se->s_port;
	}
	if (bind(s, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
		perror("bind");
		exit(1);
	}
	if (getsockname(s, (struct sockaddr *) &remote, &len) < 0) {
		perror("getsockname");
		exit(1);
	}
	fprintf(stderr, "Port number is %d\n", ntohs(remote.sin_port));
	listen(s, 1);
	int newSock = s;
	int i, j =0;
	for (i=65; i<90; i++) {
		for (j=65; j<90; j++) {
			x[i][j]=-1;
		}
	}
	//FD_ZERO(&the_state);
	while (1) {
		if (soctype == SOCK_STREAM) {
			fprintf(stderr, "Entering accept() waiting for connection.\n");
			newSock = accept(s, (struct sockaddr *) &remote, &len);
			//FD_SET(newSock,&the_state);
			Request(newSock);
			/*threadCreate1_ret = pthread_create(&threads[k], NULL,
					(void*)processRequests, (void*)&newSock);*/
			if (threadCreate1_ret) {
				printf("pthread create error\n");
				exit(0);
			}
			//threadJoin_ret =pthread_join(threads[i],NULL);
			k++;
		}
	}
	return (newSock);
}

/*
 * usage - print usage string and exit
 */

void usage() {
	printf("[−h] [-p port-number] [−l file]");
	printf("−h : Print a usage summary with all options and exit.");
	printf("-p port-number : Listen on the given port. If not provided, dec_server will listen on port 9090.");
	printf("−l file: Log all requests and responses to the given file. If not, print all to stdout.");
	exit(1);
}

/*
 * The handle request function - process and reply to client
 */
void* handleRequest(char str[], int newSock_ptr) {
	int a_i=0;
	int b_i=0;
	int i = 0;
	//char delims[] = { ";" };
	char *result= NULL;
	char *result1[100];
	char final[1000];
	char ret_str[10000]={""};

	result = strtok(str, ";");
	while (result != NULL && strcmp(result,"\n")!=0 && strcmp(result,"")!=0) {
		printf("result is %s\n", result);
		result1[i]=result;
		printf("result1 is %s\n", result1[i]);
		result = strtok( NULL, ";");
		i++;
	}
	int j =0;
	char pch[6];
	char pch1[6];
	for (j=0; j<i; j++) {
		char *temp;
		if (strstr(result1[j], "query")!=NULL) {
			char* a;
			char* b;
			//TODO query not working
			if (strstr(result1[j], "->")!=NULL) {
				//strcat(final,"Invalid query command\n");
				strcat(ret_str,"Invalid query command\n");
				printf("Invalid query command\n");
				continue;
			}
			temp = result1[j];
			temp = strtok(temp, " ");
			temp = strtok(NULL," ");
			a=*temp;
			temp = strtok(NULL," ");
			b=*temp;
			a_i = (int)(char)a;
			b_i = (int)(char)b;
			if(x[a_i][b_i]==-1 || x[b_i][a_i]==-1){
				strcat(ret_str,"The events does not exist");
			}
			else{
			if (x[a_i][b_i]==1) {
				ret_str[strlen(ret_str)]=(char)a_i;
				strcat(ret_str," before ");
				ret_str[strlen(ret_str)]=(char)b_i;
				printf("%c before %c\n", (char)a_i, (char)b_i);
			} else if (x[b_i][a_i]==1) {
				ret_str[strlen(ret_str)]=(char)b_i;
				strcat(ret_str," before ");
				ret_str[strlen(ret_str)]=(char)a_i;
				printf("%c before %c\n", (char)b_i, (char)a_i);
			} else {//TODO more check for this condition
				ret_str[strlen(ret_str)]=(char)b_i;
				strcat(ret_str," and ");
				ret_str[strlen(ret_str)]=(char)a_i;
				strcat(ret_str," both are concurrent;\n");
				printf("concurrent");
			}
			}
		} else {
			char* a;
			char* b;
			sscanf(result1[j], "%s %s", pch, pch1);
			if (strcmp(pch, "insert")==0) {
				temp = strtok(pch1, "->");
				printf("final = %s\n", temp);
				a=*temp;
				temp = strtok(NULL,">");
				b=*temp;
				printf("final = %s\n", temp);
				a_i = (int)(char)a;
				b_i = (int)(char)b;
				if(x[a_i][b_i]==-1 || x[b_i][a_i]==-1){
				strcat(ret_str,"The events does not exist");
				}
				else{
				strcat(ret_str,"INSERT DONE\n");
				if (x[b_i][a_i]==1) {
					printf("invalid");
				} else {
					x[a_i][b_i]=1;
					x[b_i][a_i]=0;
					int z=0;
					for (z=65; z<90; z++) {
						if (x[z][a_i]==1) {
							x[z][b_i]=1;
						}
					}
				}
				}
			} else if(strcmp(pch, "reset")==0) {
				int z, y =0;
				for (z=65; z<90; z++) {
					for (y=65; y<90; y++) {
						x[i][j]=-1;
					}
				}
				strcat(ret_str,"RESET DONE\n");
			}else{
				printf("Invalid command\n");
			}
		}
	}
	send(newSock_ptr, ret_str, strlen(ret_str), 0);
	return NULL;
}

void Request(int newSock_ptr) {
	//int newSock_ptr = *((int*)ptr);

	int n=0;
	printf("\nConnection accepted : %d\n", newSock_ptr);
	//while (1) {
	char buff[1024];
	//pthread_mutex_lock(&mutex_state);
	n=read(newSock_ptr, buff, sizeof(buff));
	buff[n-2]='\0';
	buff[n-1]='\0';
	
	FILE *fp = NULL;
	if(logpath != NULL)
	{
	fp = fopen(logpath,"w");
	fprintf(fp, "Received msg is %s\n", buff);
	fflush(fp);
	}
	else{
	printf("\nReceived msg is \n%s\n", buff);
	}
	
	char sft[10000];
	handleRequest(buff,newSock_ptr);
	printf("%s", sft);
	if (n==EOF || n==0) {
		printf("Client disconnected");
		free(newSock_ptr);
		return;
	}
	
	
	
	printf("\nConnection ended : %d\n", newSock_ptr);
	
	return NULL;
}

