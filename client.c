#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <pthread.h> //threading
#include <unistd.h> // read(), write(), close()
#include <signal.h> //catch signals, here it will help catching ctr+c

//maximum length for the pseudo or the messages
#define MAX 254
//port used
#define PORT 8080
#define SA struct sockaddr

static volatile int stopRunning = 0;
pthread_t thread[1025]; 
char pseudo[MAX];
char messageBuffer[MAX];
pthread_mutex_t mutex_comp_signal = PTHREAD_MUTEX_INITIALIZER;

// This function converts "usleep" in miliseconds
// sleep() only works in seconds, usleep works in nano seconde (and then, msleep works in miliseconds)
int msleep(unsigned int tms) {
  return usleep(tms * 1000);
}

void intHandler(){
	stopRunning = 1;
}


// The function 'recvMessage' receives messages from the server via the socket 'sock'. 
// It reads data from the socket, clears the buffer and print the message received.
// If the client left the conversation, it prints an exit message.
void recvMessage(int sock)
{
	char buff[MAX];
	ssize_t r;
	while(1) {
        // like 'xor buff, buff'
		bzero(buff, MAX);

		r = read(sock, buff, sizeof(buff));
		if (r == -1){
			fprintf(stderr, "recvMessage : message could not be read\n");
			exit(0);
		}
		
		if (strlen(buff) != 0){

			//if messages paused, store received messages in a buffer
			if(stopRunning){
				strcat(messageBuffer,buff);
				char* trimmedBuff = buff+sizeof("\033[0;31m\t")-1;
				if (strncmp(trimmedBuff,pseudo,strlen(pseudo)) == 0){
					printf("%s",messageBuffer);
					messageBuffer[0] = '\0';
					stopRunning = 0;
				}
			}else if ((strncmp(buff, "/exit", 5)) == 0) {
				printf("\033[0;37m\nClient Exit...\n");
				break;
			}else
			{
				//if not paused, print the message immediately
				printf("%s",buff);
			}

		}
	}
}


// The function 'sendMessage' sends a message to the server via the socket 'sock'
// It reads the message from the user, clears the buffer with the function 'bzero' and sends the message to the server.
// If the message sent is /exit, it ends the function.
void sendMessage(int sock)
{
	char buff[MAX];
//	int n;
	ssize_t s;
	while(1) { 
		// like 'xor buff, buff'
		bzero(buff, MAX);
		//n = 0;
		fgets(buff,sizeof(buff),stdin);	
		//Message to send
		printf("\33[2K\r");
        	//while ((buff[n++] = getchar()) != '\n');
		if(strlen(buff) != 1 && strlen(buff) <= 1020){
			s = send(sock, buff, sizeof(buff), 0);
			if(s == -1){
				fprintf(stderr,"sendMessage : message could not be sent\n");
				exit(0);
			}
		}
		else 
			printf("Invalid message size...\n");
		
        	if ((strncmp(buff, "/exit", sizeof("/exit"))) == 0) {
			break;
		}
	}
}


// Creates the socket, establishes a connection with the server, prompts the user to enter a pseudonym and send it to the server.
// It forks a child process for sending messages and another for receiving messages. It finally closes the socket.
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
	else {
		printf("connected to the server\nPossible commands :\n\t/help to see all possible commands\n\t/list to list all connected clients\n\t/exit to exit your session\n\tctrl+c to stop message flow. To see messages again, send a message.\n");
	}


	int n = 0;
	printf("Please enter your pseudo : ");
	while ((pseudo[n++] = getchar()) != '\n');
	if(pseudo[strlen(pseudo) - 1] == '\n') pseudo[strlen(pseudo) - 1] = '\0';
	//add to the pseudo the pid of the user (process id of the user)
	char id[10];
	snprintf(id,10,"(%d)",getpid());
	strcat(pseudo,id);
	write(sock, pseudo, sizeof(pseudo));	
		
	signal(SIGINT,intHandler);
	pid_t pid = fork();
	if (pid == -1){
		perror("fork error");
		return EXIT_FAILURE;
	} else if (pid == 0) {
		sendMessage(sock);
	} else 
	{
		recvMessage(sock);
	}
	// close the socket
	close(sock);
}
