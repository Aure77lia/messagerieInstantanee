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
#define TIMEOUT_SECONDS 20

int clientCount = 0, top = -1;
char stack[MAX][1024];
//static pthread_mutex_t mutex; 
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_t timer;
int serverSocket;


struct client {
	//int index;
	int sockID;
	char *grpID;
	char *pseudo;
	struct sockaddr_in clientAddr;
	int len;
	char color[sizeof("\033[0;30m")];
};

struct client ClientList[1024];
pthread_t thread[1024]; 

int indexClientSock(int sockID){
    pthread_mutex_lock(&mutex);
    int index = -1;
    for(int i = 0; i < clientCount; i++){
        if (ClientList[i].sockID == sockID){
            index = i ;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    return index;
}

int indexClientPseudo(char *pseudo){
    pthread_mutex_lock(&mutex);
    int index = -1;
    for(int i = 0; i < clientCount; i++){
		if (strcmp(ClientList[i].pseudo, pseudo) == 0){
            index = i ;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    return index;
}

int removeClient(int index) {
    int size = 1024;
	if (index < 0 || index >= size) {
        printf("Index hors limites\n");
        return -1;
    }

    for (int i = index; i < size - 1; i++) {
        ClientList[i] = ClientList[i + 1];
    }
	return 0 ;
}

void *timerThread(void *arg) {
    sleep(TIMEOUT_SECONDS);
    pthread_mutex_lock(&mutex);
    if (clientCount == 0) {
        printf("Aucun client connecté. Arrêt du serveur...\n");
		close(serverSocket);
        exit(0);
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

// The push function is used to store incoming messages in a stack.
// when used it is always protected by a mutex to avoid concurrency issues when multiple threads try to modify the stack at the same time.
/*int push(char stack[MAX][1024], int *top, char data[1024])
{
      if(*top == MAX -1)
            return(-1);
      else {
            *top = *top + 1;
            strcpy(stack[*top], data);
            return(1);
      }
} 
int push(char stack[MAX][1024], int *top, char data[1024]) {
    pthread_mutex_lock(&mutex);
    if (*top >= MAX - 1) {
        pthread_mutex_unlock(&mutex);
        fprintf(stderr, "Stack overflow prevented at push\n");
        return -1;
    } else {
        *top = *top + 1;
        strncpy(stack[*top], data, 1023);  // Safeguard against buffer overflow
        stack[*top][1023] = '\0';  // Ensure null termination
        pthread_mutex_unlock(&mutex);
        return 1;
    }
} */

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

void broadcastClient(char *dataOut) {
    	for (int i = 0; i < clientCount; i++) {
        	if (send(ClientList[i].sockID, dataOut, 1024, 0) == -1){
	       		fprintf(stderr, "broadcastClient : error sending messages \n");
		}
    	}
    
    printf("%s", dataOut);
}


// The Dispatcher function ensures the reception, analysis and distribution of messages between clients
void * dispatcher(char* dataIn){
	char* data;
	int err;
	// identifies the sender of the message
	char *sender = parseWord(dataIn, 1);
	char *thirdWord = parseWord(dataIn, 3);
	int clientSocket;
       	//Gets the client's socketID
	int indexClient = indexClientPseudo(sender);

	pthread_mutex_lock(&mutex);
	clientSocket = ClientList[indexClient].sockID;
	if ((strncmp(thirdWord, "/exit", 5)) == 0) {
		data = malloc(sizeof(char)*sizeof("\033[1;91m"));
		strcpy(data, "\033[1;91m");
		err = send(clientSocket, "/exit", sizeof("/exit"), 0);
		if (err == -1){
			fprintf(stderr, "dispatcher : exit failed\n");
		}
		else {
			data = realloc(data, sizeof(char) * (sizeof(sender)+sizeof(" disconnected..\n")));
			strcat(data, sender);
			strcat(data, " disconnected...\n");
			broadcastClient(data);
			close(clientSocket);
			clientCount--;
			removeClient(indexClient);
			if (clientCount == 0){
				if (pthread_create(&timer, NULL, timerThread, NULL) != 0){
					perror("dispatcher : error creation thread");
				}
			}
		}
		free(data);
	}
	// Allows to list all online clients, and sends it to the client
		
	else if ((strncmp(thirdWord, "/list", 5)) == 0) {
		data = malloc(sizeof(char)*sizeof("\033[1;95m"));
		strcpy(data, "\033[1;95m");
		for (int i = 0; i < clientCount; i++) {
			data = realloc(data, sizeof(char) * (sizeof(ClientList[i].pseudo)+sizeof("\n")));
     			strcat(data, ClientList[i].pseudo);
			strcat(data, "\n");
		}
		err = send(clientSocket, data, strlen(data), 0); // Directly send to the requesting client
		if (err == -1){
			fprintf(stderr, "dispatcher : /list failed\n");
		}
		free(data);
	}

	// Gives helpful commands
	else if ((strncmp(thirdWord, "/help", 5)) == 0) {
		data = malloc(sizeof(char)*sizeof("\033[1;92m"));
		strcpy(data, "\033[1;92m");
		data = realloc(data, sizeof(char) * (sizeof("Possible commands :\n\t/list to list all connected clients\n")+sizeof("\t/exit to exit your session\n")+sizeof("\tctrl+c to stop message flow. To see messages again, send a message.\n")));

		strcat(data, "Possible commands :\n\t/list to list all connected clients\n");
		strcat(data, "\t/exit to exit your session\n");
		strcat(data, "\tctrl+c to stop message flow. To see messages again, send a message.\n");
		err = send(clientSocket, data, strlen(data), 0); // Directly send to the requesting client
		if (err == -1){
			fprintf(stderr, "dispatcher : /help failed\n");
		}
		free(data);
	}
			
	// Sends message to all connected clients
	else {

		broadcastClient(dataIn);
	}
	
	pthread_mutex_unlock(&mutex);

	return NULL;
}


// The clientListener function is used to listen for messages sent by a specific client and properly handle client disconnection.
// Function handeling connexions
void * clientListener(void * ClientDetail){
	// Variables
	struct client* clientDetail = (struct client*) ClientDetail;
	//int index = clientDetail -> index;
	int clientSocket = clientDetail -> sockID;
	int index = indexClientSock(clientSocket);
	ClientList[index].pseudo = "default pseudo";
	clientDetail -> grpID = "all";
	char pseudo[254], dataIn[1024], dataOut[1024];
	int num = (index%7) + 31;
	char str[2];
	sprintf(str,"%d",num);
	strcat(ClientList[index].color,"\033[0;");
	strcat(ClientList[index].color,str);
	strcat(ClientList[index].color,"m");


	// pipe through fork
	int fd[2];
	pipe(fd);

	// Waits for the pseudonym of the client
	int receved = recv(clientSocket, pseudo, 254, 0);
	
	ClientList[index].pseudo = pseudo;
	strcpy(dataOut, ClientList[index].pseudo);
	strcat(dataOut, " is connected !\n");
	
	broadcastClient(dataOut);
	
	//pthread_mutex_lock(&mutex);
	//push(stack, &top, dataOut);
	//pthread_mutex_unlock(&mutex);

	

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
			strcpy(dataOut, ClientList[index].color);
			strcat(dataOut, "\t");
			strcat(dataOut, ClientList[index].pseudo);
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
			// Allows client to exit
			if (r == -1) {
				msleep(1000);
				close(clientSocket);
				clientDetail -> sockID = 0;
				break;
			}

			// Send message to all clients
			else{
				dispatcher(dataIn);
				//broadcastClient(dataIn);
				//pthread_mutex_lock(&mutex);
				//push(stack, &top, dataIn);
				//pthread_mutex_unlock(&mutex);
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

	//pthread_create(&thread_dispatcher, NULL, DispatcherBis, NULL);

	// Threads each new connection to the server, and links a struct to each client
	unsigned int newClient;
	while(1){
		pthread_mutex_init(&mutex, NULL);
		
		pthread_mutex_lock(&mutex);
		if (clientCount==0){
			if (pthread_create(&timer, NULL, timerThread, NULL) != 0) {
                		perror("Erreur lors de la création du thread du temporisateur");
                		pthread_mutex_unlock(&mutex);
                		return 1;
        	    	}
		}
		pthread_mutex_unlock(&mutex);

		newClient = ClientList[clientCount].len;

		ClientList[clientCount].sockID = accept(serverSocket, (struct sockaddr*) &ClientList[clientCount].clientAddr, &newClient);
		if (ClientList[clientCount].sockID == -1){
			fprintf(stderr, "cannot connect\n");
			exit(0);
		}
		int thr;
		//ClientList[clientCount].index = clientCount + 1;
		
		thr = pthread_create(&thread[clientCount], NULL, clientListener, (void *) &ClientList[clientCount]);
		if (thr != 0)
		{
			fprintf(stderr, "can't create thread. \n");
			exit(0);
		}
		
		clientCount ++;
		pthread_mutex_destroy(&mutex);
		
	}
	pthread_exit(NULL);
	// After chatting close the socket
	close(serverSocket);
}
