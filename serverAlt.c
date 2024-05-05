#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdbool.h>

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
};

struct client ClientList[1024];
pthread_t thread[1024];

int indexClientSock(int sockID){
    printf("indexClientSock : avant le verrou\n");
	pthread_mutex_lock(&mutex);
	printf("indexClientSock : dans le verrou\n");
    int index = -1;
	if(clientCount==0){
		perror("IndexClientSock : pas de client connecté");
	} else {
		for(int i = 0; i < clientCount; i++){
			if (ClientList[i].sockID == sockID){
				index = i ;
				break;
			}
		}
	}
	if(index==-1){
		perror("client non trouvé");
		return -1;
	}
    pthread_mutex_unlock(&mutex);
	printf("indexClientSock : sorti du verrou\n");
    return index;
}

int indexClientPseudo(char *pseudo){
    printf("indexCLientPseudo : avant le verrou\n");
	pthread_mutex_lock(&mutex);
	printf("indexCLientPseudo : dans le verrou\n");

    int index = -1;
    if(clientCount==0){
		perror("indexClientPseudo : pas de client connecté");
	} else {
		for(int i = 0; i < clientCount; i++){
			if (strcmp(ClientList[i].pseudo, pseudo) == 0){
				index = i ;
				break;
			
			}
		}
	}
	pthread_mutex_unlock(&mutex);
	printf("indexCLientPseudo : après le verrou\n");

	return index;
}

int removeClient(int index){
	printf("remove client : avant lock\n");;
	pthread_mutex_lock(&mutex);
	printf("remove client : on est entre dans le verrou !\n");
	if(clientCount==0){
		printf("removeClient : pas de client à remove !\n");
		return -1;
	}
	if (index < 0 || index >= clientCount) {
        printf("removeClient : Index hors limites\n");
        return -1;
    }
    for (int i = index; i < clientCount - 1; i++) {
        ClientList[i] = ClientList[i + 1];
		printf("hehe\n");
    }
	clientCount--;
	pthread_mutex_unlock(&mutex);
	printf("remove client : on est sorti du verrou !\n");

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

bool isClientConnected(int sockID){
	//pthread_mutex_lock(&mutex);
    bool isConnected = false;
	if(clientCount!=0){
    for(int i = 0; i < clientCount; i++){
        if (ClientList[i].sockID == sockID){
            isConnected = true ;
            break;
        }
    }
	}
    //pthread_mutex_unlock(&mutex);
    return isConnected;
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
    
	pthread_mutex_lock(&mutex);
    for (int i = 0; i < clientCount; i++) {
        send(ClientList[i].sockID, dataOut, 1024, 0); 
    }
	pthread_mutex_unlock(&mutex);
    
    printf("%s", dataOut);
}


// The Dispatcher function ensures the reception, analysis and distribution of messages between clients
void * DispatcherBis(char* dataIn){
	printf("Dispatcher : se lance\n");
	char* data;
	int err;
	// identifies the sender of the message
	char *sender = parseWord(dataIn, 0);
	char *thirdWord = parseWord(dataIn, 2);
	int clientSocket;
       	//Gets the client's socketID
	int indexClient = indexClientPseudo(sender);
	if(indexClient==-1){
		printf("DispatcherBis : erreur lors de la recherche d'index avec indexClientPseudo\n");
		//exit(0);
	}
	//printf("Dispatcher : avant le verrou\n");

	pthread_mutex_lock(&mutex);
	//printf("Dispatcher : après le verrou\n");

	clientSocket = ClientList[indexClient].sockID;
	if ((strncmp(thirdWord, "/exit", 5)) == 0) {
		data = malloc(sizeof(char)*sizeof("\033[0;31m"));
		//strcpy(data, "\033[0;32m");
		err = send(clientSocket, "/exit", sizeof("/exit"), 0);
		if (err == -1){
			fprintf(stderr, "dispatcher : exit failed");
		}
		else {
			data = realloc(data, sizeof(char) * (sizeof(sender)+sizeof(" disconnected..\n")));
			strcat(data, sender);
			strcat(data, " disconnected...\n");
			pthread_mutex_unlock(&mutex);
			broadcastClient(data);
			pthread_mutex_lock(&mutex);

			close(clientSocket);
			pthread_mutex_unlock(&mutex);
			removeClient(indexClient);
			pthread_mutex_lock(&mutex);

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
		data = malloc(sizeof(char)*sizeof("\033[0;32m"));
		//strcpy(data, "\033[0;32m");
		for (int i = 0; i < clientCount; i++) {
			data = realloc(data, sizeof(char) * (sizeof(ClientList[i].pseudo)+sizeof("\n")));
     			strcat(data, ClientList[i].pseudo);
			strcat(data, "\n");
		}
		err = send(clientSocket, data, strlen(data), 0); // Directly send to the requesting client
		if (err == -1){
			fprintf(stderr, "dispatcher : /list failed");
		}
		free(data);
	}

	// Gives helpful commands
	else if ((strncmp(thirdWord, "/help", 5)) == 0) {
		data = malloc(sizeof(char)*sizeof("\033[0;32m"));
		//strcpy(data, "\033[0;32m");
		data = realloc(data, sizeof(char) * (sizeof("Possible commands :\n\t/list to list all connected clients\n")+sizeof("\t/exit to exit your session\n")+sizeof("\tctrl+c to stop message flow. To see messages again, send a message.\n")));

		strcat(data, "Possible commands :\n\t/list to list all connected clients\n");
		strcat(data, "\t/exit to exit your session\n");
		strcat(data, "\tctrl+c to stop message flow. To see messages again, send a message.\n");
		err = send(clientSocket, data, strlen(data), 0); // Directly send to the requesting client
		if (err == -1){
			fprintf(stderr, "dispatcher : /help failed");
		}
		free(data);
	}
			
	// Sends message to all connected clients
	else {
		if(sizeof(dataIn) < 1000){
		//	strcpy(dataIn, "\033[0;37m");
		}
		pthread_mutex_unlock(&mutex);
		broadcastClient(dataIn);
		pthread_mutex_lock(&mutex);
	}
	
	pthread_mutex_unlock(&mutex);

	return NULL;
}


// The clientListener function is used to listen for messages sent by a specific client and properly handle client disconnection.
// Function handeling connexions
void * clientListener(void * ClientDetail){
	// Variables
	struct client* clientDetail = (struct client*) ClientDetail;
	int clientSocket = clientDetail -> sockID;
	int index = indexClientSock(clientSocket);
	if(index==-1){
		printf("clientListener : la fonction indexClientSock n'a pas fonctionné\n");
		//exit(0);
	}
	ClientList[index].pseudo = "default pseudo";
	clientDetail -> grpID = "all";
	char pseudo[254], dataIn[1024], dataOut[1024];
	
	// pipe through fork
	int fd[2];
	pipe(fd);

	// Waits for the pseudonym of the client
	int receved = recv(clientSocket, pseudo, 254, 0);

	ClientList[index].pseudo = pseudo;
	strcpy(dataOut, ClientList[index].pseudo);
	strcat(dataOut, " is connected !\n");
	
	broadcastClient(dataOut);


	pid_t pid = fork();
	if(pid == -1){
		perror("forking");
		return NULL;
	}
	else if(pid == 0){
		// Child process | its goal is to listen & receive data and send it to the parser

		while(isClientConnected(clientDetail->sockID)){
			sleep(1); // pour ne pas bloquer en boucle
			bzero(dataIn, 1024);
			bzero(dataOut, 1024);
			
			// Always & forever listen
			receved = recv(clientSocket, dataIn, 1024, 0);
		
			dataIn[receved] = '\0';

			strcpy(dataOut, ClientList[index].pseudo);
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
				removeClient(clientDetail->sockID);
				clientDetail -> sockID = 0;
				break;
			}

			// Send message to all clients
			else{
				DispatcherBis(dataIn);
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
		
		//pthread_mutex_lock(&mutex);
		if (clientCount==0){
			if (pthread_create(&timer, NULL, timerThread, NULL) != 0) {
                		perror("Erreur lors de la création du thread du temporisateur");
                		pthread_mutex_unlock(&mutex);
                		return 1;
        	    	}
		}
		//pthread_mutex_unlock(&mutex);

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
		
	}
	pthread_exit(NULL);
	// After chatting close the socket
	close(serverSocket);
}
