#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX 160
#define PORT 8080
#define SA struct sockaddr

int clientCount = 0, top = -1;
char stack[MAX][1024];
static pthread_mutex_t mutex; 
//static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

struct client {
	int index;
	int sockID;
	char *grpID;
	char *pseudo;
	struct sockaddr_in clientAddr;
	int len;
};

struct client Client[1024];
pthread_t thread[1024]; 

// The push function is used to store incoming messages in a stack.
// when used it is always protected by a mutex to avoid concurrency issues when multiple threads try to modify the stack at the same time.
int push(char stack[MAX][1024], int *top, char data[1024])
{
      if(*top == MAX -1)
            return(-1);
      else {
            *top = *top + 1;
            strcpy(stack[*top], data);
            return(1);
      }
}

// The pop function is used to extract messages so that they can be processed and distributed to clients by the Dispatcher function.
int pop(char stack[MAX][1024], int *top, char data[1024])
{
      if(*top == -1)
            return(-1);
      else {
            strcpy(data, stack[*top]);
            *top = *top - 1;
            return(1);
      }
}

// sleep() only works in seconds, msleep works in miliseconds
int msleep(unsigned int tms) {
  return usleep(tms * 1000);
}

//Allows to catch a specific word of the data send by the user
char * parseWord( const char str[], size_t pos ){
    const char delim[] = " \t";
    char *inputCopy = malloc( ( strlen( str ) + 1 ) );
    char *p = NULL;

    if (inputCopy != NULL ){
        strcpy(inputCopy, str );
        p = strtok(inputCopy, delim );

        while ( p != NULL && pos -- != 0 ){
            p = strtok( NULL, delim );
        }
        if (p != NULL ){
            size_t n = strlen(p);
            memmove(inputCopy, p, n + 1);

            p = realloc(inputCopy, n + 1);
        }           
        if (p == NULL ){
            free(inputCopy);
        }
    }
    return p;
}

// The broadcastClient function is used to send a message to all clients connected to the instant messaging server.
// *dataOut is the message to broadcast to all connected clients.
void broadcastClient(char *dataOut){
	char buffer[1024];
	for(int i = 0; i < clientCount; i++){
		send(Client[i].sockID, dataOut, sizeof(buffer), 0);
	}
}

// The Dispatcher function ensures the reception, analysis and distribution of messages between clients
void * Dispatcher(){
	char data[1024], dataTMP[1024];
	int isNull;
	while(1){
		bzero(data, 1024);
		bzero(dataTMP, 1024);
		
		isNull = pop(stack, &top, data);
		if(isNull != -1){
			//pop(stack, &top, data);
			msleep(20);

			// identifies the sender of the message
			char *sender = parseWord(data, 0);
			char *thirdWord = parseWord(data, 2);
			int clientSocket;

			//Gets the client's socketID
			for(int i = 0; i < clientCount; i++){
				if ((strncmp(sender, Client[i+1].pseudo, strlen(Client[i+1].pseudo))) == 0){
					clientSocket = Client[i].sockID;
				}
			}

			if ((strncmp(thirdWord, "/exit", 5)) == 0) {
				send(clientSocket, "/exit", sizeof("/exit"), 0);
				strcpy(dataTMP, sender);
				strcat(dataTMP, " disconnected...\n");
				broadcastClient(dataTMP);
				close(clientSocket);
			}
			// Allows to list all online clients, and sends it to the client
			else if ((strncmp(thirdWord, "/list", 5)) == 0) {
				strcpy(dataTMP, "\033[0;32m");
				for (int i = 0; i < clientCount; i++) {
					if ((strncmp(Client[i+1].pseudo, "\0", 1)) != 0){
						strcat(dataTMP, Client[i+1].pseudo);
						dataTMP[strlen(dataTMP)] = '\n';
					}
				}
				dataTMP[strlen(dataTMP)] = '\n';
				for(int i = 0; i < clientCount; i++){
					if ((strncmp(sender, Client[i+1].pseudo, strlen(Client[i+1].pseudo))) == 0){
						send(Client[i].sockID, dataTMP, strlen(dataTMP), 0);
					}
				}
			} 

			// Gives helpful commands
			else if ((strncmp(thirdWord, "/help", 5)) == 0) {
				strcpy(dataTMP, "\033[0;32m");
				strcat(dataTMP, "Possible commands :\n\t/list to list all connected clients\n");
				strcat(dataTMP, "\t/exit to exit your session");
				strcat(dataTMP, "\tctrl+c to stop message flow. To see messages again, send a message.");
				dataTMP[strlen(dataTMP)] = '\n';
				for(int i = 0; i < clientCount; i++){
					if ((strncmp(sender, Client[i+1].pseudo, strlen(Client[i+1].pseudo))) == 0){
						send(Client[i].sockID, dataTMP, strlen(dataTMP), 0);
					}
				}
			} 

			// Sends message to all connected clients
			else {
				strcpy(dataTMP, "\033[0;31m");
				strcat(dataTMP, data);
				printf("%s", dataTMP);
				broadcastClient(dataTMP);
			}
		}
	}
	return NULL;
}


// The clientListener function is used to listen for messages sent by a specific client and properly handle client disconnection.
// Function handeling connexions
void * clientListener(void * ClientDetail){
	// Variables
	struct client* clientDetail = (struct client*) ClientDetail;
	int index = clientDetail -> index;
	int clientSocket = clientDetail -> sockID;
	Client[index].pseudo = "default pseudo";
	clientDetail -> grpID = "all";
	char pseudo[254], dataIn[1024], dataOut[1024];
	
	// pipe through fork
	int fd[2];
	pipe(fd);

	// Waits for the pseudonym of the client
	int receved = recv(clientSocket, pseudo, 254, 0);

	Client[index].pseudo = pseudo;
	strcpy(dataOut, Client[index].pseudo);
	strcat(dataOut, " is connected !\n");
	pthread_mutex_lock(&mutex);
	push(stack, &top, dataOut);
	pthread_mutex_unlock(&mutex);

	pid_t pid = fork();
	if(pid == -1){
		perror("forking");
		return NULL;
	}
	else if(pid == 0){
		// Child process | its goal is to listen & receive data and send it to the parser

		while(1){
			bzero(dataIn, 1024);
			bzero(dataOut, 1024);
			
			// Always & forever lisen
			receved = recv(clientSocket, dataIn, 1024, 0);
			dataIn[receved] = '\0';

			strcpy(dataOut, Client[index].pseudo);
			strcat(dataOut, " : ");
			strcat(dataOut, dataIn);

			// Sends data through the pipe
			write(fd[1], dataOut, sizeof(dataOut));
		}
	} 
	else { 
		int r;
		// Parent process | its goal is to parse the data given by the listener
		while(1){
			bzero(dataIn, 1024);
			bzero(dataOut, 1024);
			//int read = recv(clientSocket, dataIn, 1024, 0);
			r = read(fd[0], dataIn, sizeof(dataIn));
			char *command = parseWord(dataIn, 2);
			// printf("%s : %s", Client[index].pseudo, dataIn);
			// Allows client to exit
			if (r == -1 || (strncmp(command, "/exit", 5)) == 0) {
				pthread_mutex_lock(&mutex);
				push(stack, &top, dataIn);
				pthread_mutex_unlock(&mutex);
				msleep(1000);
				close(clientSocket);
				clientDetail -> sockID = 0;
				break;
			}

			// Send message to all clients
			else{
				pthread_mutex_lock(&mutex);
				push(stack, &top, dataIn);
				pthread_mutex_unlock(&mutex);
			}

		}
	}
	pthread_exit(NULL); //-> mettre pthread exit ne vire pas l'utilisateur quand il quitte (wtf)
	return NULL;
}

// The main function initializes and configures the instant messaging server,
//listens for incoming client connections, 
// creates threads to handle each client connection, and manages message distribution between clients via the dispatcher thread
int main()
{
	struct sockaddr_in servaddr;
	pthread_t thread_dispatcher;

	// Socket create & verification
	int serverSocket = socket(PF_INET, SOCK_STREAM, 0);

	if (serverSocket == -1) {
		printf("socket creation failed...\n");
		return EXIT_FAILURE;
	}
	bzero(&servaddr, sizeof(servaddr));

	// Assign IP, PORT, PROTOCOL
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);

	// Binding & listening verification
	if(bind(serverSocket,(struct sockaddr *) &servaddr , sizeof(servaddr)) == -1) {
		perror("error binding...");
		return EXIT_FAILURE;
	}
	if(listen(serverSocket, 1024) == -1){
		perror("error listening...");
		return EXIT_FAILURE;
	}
	else printf("Server listening...\n");

	pthread_create(&thread_dispatcher, NULL, Dispatcher, NULL);

	// Threads each new connection to the server, and links a struct to each client
	unsigned int newClient;
	while(1){
		pthread_mutex_init(&mutex, NULL);
		newClient = Client[clientCount].len;
		Client[clientCount].sockID = accept(serverSocket, (struct sockaddr*) &Client[clientCount].clientAddr, &newClient);
		if (Client[clientCount].sockID == -1){
			fprintf(stderr, "cannot connect\n");
			exit(0);
		}
		int thr;
		Client[clientCount].index = clientCount + 1;
		
		thr = pthread_create(&thread[clientCount], NULL, clientListener, (void *) &Client[clientCount]);
		if (thr != 0)
		{
			fprintf(stderr, "can't create thread. \n");
			exit(0);
		}
		
		clientCount ++;
		
	}
	pthread_exit(NULL);
	// After chatting close the socket
	close(serverSocket);
}
