#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <pthread.h> //threading
#include <unistd.h> // read(), write(), close()

#define MAX 254
#define PORT 8080
#define SA struct sockaddr

pthread_t thread[1024]; 
char pseudo[MAX];

// sleep() only works in seconds, usleep works in nano seconde (and then, msleep works in miliseconds)
int msleep(unsigned int tms) {
  return usleep(tms * 1000);
}

void recvMessage(int sock)
{
	char buff[MAX];
	while(1) {
        // like 'xor buff, buff'
		bzero(buff, MAX);

        read(sock, buff, sizeof(buff));
		printf("%s", buff);

		if ((strncmp(buff, "/exit", 5)) == 0) {
			printf("\nClient Exit...\n");
			break;
		}
	}
}

void sendMessage(int sock)
{
	char buff[MAX];
	int n;

	while(1) { 
        // like 'xor buff, buff'
		bzero(buff, MAX);
		n = 0;
		
		//Message to send
		printf("\33[2K\r");
        while ((buff[n++] = getchar()) != '\n');
		if(strlen(buff) != 1 && strlen(buff) <= 1020){
			send(sock, buff, sizeof(buff), 0);
		}
		else 
			printf("Invalid message size...\n");

        if ((strncmp(buff, "/exit", sizeof("/exit"))) == 0) {
			break;
		}
	}
}

int main()
{
	int sock;
	struct sockaddr_in servaddr;

	// socket create and verification
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket creation error\n");
		exit(0);
	}
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(PORT);

	// connect the client socket to server socket
	if (connect(sock, (SA*)&servaddr, sizeof(servaddr)) != 0) {
		perror("connection with server error");
		exit(0);
	}
	else printf("connected to the server...\n");

	int n = 0;
	printf("Please enter your pseudo : ");
	while ((pseudo[n++] = getchar()) != '\n');
	if(pseudo[strlen(pseudo) - 1] == '\n') pseudo[strlen(pseudo) - 1] = '\0';
	write(sock, pseudo, sizeof(pseudo));

	pid_t pid = fork();
	if (pid == -1){
		perror("fork error");
		return EXIT_FAILURE;
	} else if (pid == 0) {
		sendMessage(sock);
	} else {
		recvMessage(sock);
	}
	// close the socket
	close(sock);
}
