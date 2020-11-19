#include        <netinet/in.h>  	/* sockaddr_in{} and other Internet defns */
#include        <stdio.h>		/* Buffered I/O Standard input/output library functions */
#include        <string.h>		/* memset	*/
#include 	<errno.h>		/* errors	*/ 
#include 	<stdlib.h>		/* exit		*/
#include 	<fcntl.h>		/* files	*/
#include 	<syslog.h>		/* logging to syslog	*/
#include 	<sys/resource.h>	/* struct rlimit rl	*/
#include 	<signal.h>		/* struct sigaction sa	*/
#include	<mysql.h>
//#include 	<sys/socket.h>
//#include 	<sys/types.h>
//#include 	<sys/uio.h>
//#include 	<unistd.h>

#define SA      struct sockaddr

/* Following could be derived from SOMAXCONN in <sys/socket.h>, but many
   kernels still #define it as 5, while actually supporting many more */
#define LISTENQ         1024    /* 2nd argument to listen() */

/* Miscellaneous constants */
#define MAXLINE         4096    /* max text line length */

/* Define bzero() as a macro if it's not in standard C library. */
// Fill memory with a constant byte = 0
#define bzero(ptr,n)            memset(ptr, 0, n)

void
daemonize(const char *cmd)
{
	int i, fd0, fd1, fd2;
	pid_t pid;
	struct rlimit rl;
	struct sigaction sa;
	
//	Сбросить маску режима создания файла.
	umask(0);

//	Получить максимально возможный номер дескриптора файла.
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
		fprintf(stderr, "%s: невозможно получить максимальный номер дескриптора: %s\n", cmd, strerror(errno));

//	Стать лидером новой сессии, чтобы утратить управляющий терминал.
	if ((pid = fork()) < 0)
		 fprintf(stderr, "%s: ошибка вызова функции fork: %s\n", cmd, strerror(errno));
	else if (pid != 0) /* родительский процесс */
		exit(0);
	setsid();

//	Обеспечить невозможность обретения управляющего терминала в будущем.
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		fprintf(stderr, "%s: невозможно игнорировать сигнал SIGHUP: %s\n", cmd, strerror(errno));
	if ((pid = fork()) < 0)
		fprintf(stderr, "%s: ошибка вызова функции fork: %s\n", cmd, strerror(errno));
	else if (pid != 0) /* родительский процесс */
		exit(0);
//	Назначить корневой каталог текущим рабочим каталогом, чтобы впоследствии можно было отмонтировать файловую систему.
	if (chdir("/") < 0)
		fprintf(stderr, "%s: невозможно сделать текущим рабочим каталогом /: %s\n", cmd, strerror(errno));

//	Закрыть все открытые файловые дескрипторы.
	if (rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;

	for (i = 0; i < rl.rlim_max; i++)
		close(i);

//	Присоединить файловые дескрипторы 0, 1 и 2 к /dev/null.
	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);

//	Инициализировать файл журнала.
	openlog(cmd, LOG_CONS, LOG_DAEMON);
	if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
		syslog(LOG_ERR, "ошибочные файловые дескрипторы %d %d %d", fd0, fd1, fd2);
		exit(1);
	}
}

int
main(int argc, char **argv)
{
	int			filefd, socketfd, bindfd, listenfd, connfd, readfd;
	struct sockaddr_in	servaddr;
	char			buff[MAXLINE];
	char ln[1] = {'\n'};		/*	Adding newline character	*/
	MYSQL 			mysql;

	mysql_init(&mysql);
//	mysql_options(&mysql,MYSQL_READ_DEFAULT_GROUP,"your_prog_name");
	if (!mysql_real_connect(&mysql,"localhost","avaya","passwd","avaya",0,NULL,0))
    		fprintf(stderr, "Failed to connect to database: Error: %s\n", mysql_error(&mysql));

	daemonize("smdrreceiver");

	filefd = open("/var/log/avaya/smdr.log", O_APPEND | O_RDWR);
                if (filefd == -1) 
		        filefd = open("/var/log/avaya/smdr.log", O_CREAT | O_RDWR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
			if (filefd == -1) {
                        	perror("Can't open file");
                        	exit (-1);
                	}


//	Open the socket for IPv4 tcp connections
	socketfd = socket(AF_INET, SOCK_STREAM, 6);
		if (socketfd == -1) {
			perror("Can't open socket");
			exit (-1);
		}

//	Fill memory area serveraddr with a constant byte = 0
	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(9910);	/* Avaya receiver */

//	Bind a name to a open socket
	bindfd = bind(socketfd, (SA *) &servaddr, sizeof(servaddr));
                if (bindfd == -1) {
                        perror("Can't bind name to socket");
                        exit (-1);
                }

//	Listen for connections on a socket
	listenfd = listen(socketfd, LISTENQ);
                if (listenfd == -1) {
                        perror("Can't bind name to socket");
                        exit (-1);
                }

	for ( ; ; ) {
/*	Accept a connection on a listening socket and extracts the first connection request on the queue of pending connections for the listening socket, 
	sockfd, creates a new connected socket, and returns a new file descriptor referring to that socket.	*/
		connfd = accept(socketfd, (SA *) NULL, NULL);

//	Read attempts to read up to count bytes from file descriptor fd into the buffer starting at buf.
	readfd = read(connfd, buff, 180);
		write(filefd, buff, strlen(buff));
		write(filefd, ln, sizeof(ln));
		close(connfd);
	}
	close(filefd);
}
