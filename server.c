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
#include <arpa/inet.h>
#include <errno.h>


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
	//char *color;
	char color[sizeof("\033[0;30m")];
};

struct client ClientList[1024];
pthread_t thread[1024];

int indexClientSock(int sockID){
    //printf("indexClientSock : avant le verrou\n");
	pthread_mutex_lock(&mutex);
	//printf("indexClientSock : dans le verrou\n");
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
	//printf("indexClientSock : sorti du verrou\n");
    return index;
}

int indexClientPseudo(char *pseudo){
    //printf("indexCLientPseudo : avant le verrou\n");
	pthread_mutex_lock(&mutex);
	//printf("indexCLientPseudo : dans le verrou\n");

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
	if(index==-1){
		printf("indexClientPseudo : client non trouvé !\n");
	}
	return index;
}

int removeClient(int index){
	//printf("remove client : avant lock\n");;
	pthread_mutex_lock(&mutex);
	//printf("remove client : on est entre dans le verrou !\n");
	if(clientCount==0){
		//printf("removeClient : pas de client à remove !\n");
		return -1;
	}
	if (index < 0 || index >= clientCount) {
        //printf("removeClient : Index hors limites\n");
        return -1;
    }
    for (int i = index; i < clientCount - 1; i++) {
        ClientList[i] = ClientList[i + 1];
		//printf("hehe\n");
    }
	clientCount--;
	pthread_mutex_unlock(&mutex);
	//printf("remove client : on est sorti du verrou !\n");

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
	//printf("isClientConnected : avant le verrou\n");
	pthread_mutex_lock(&mutex);
	//printf("isClientConnected : dans le verrou\n");
    bool isConnected = false;
	if(clientCount!=0){
    for(int i = 0; i < clientCount; i++){
        if (ClientList[i].sockID == sockID){
            isConnected = true ;
            break;
        }
    }
	}
    pthread_mutex_unlock(&mutex);
	//printf("isClientConnected : après le verrou\n");

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
	//printf("broadcast : avant verrou \n");
	pthread_mutex_lock(&mutex);
	//printf("broadcast : dans verrou \n");

    	for (int i = 0; i < clientCount; i++) {
        	if (send(ClientList[i].sockID, dataOut, 1024, 0) == -1){
	       		fprintf(stderr, "broadcastClient : error sending messages \n");
		}
    	}
	pthread_mutex_unlock(&mutex);
	//printf("broadcast : après verrou \n");
    	printf("%s", dataOut);
}


// The Dispatcher function ensures the reception, analysis and distribution of messages between clients
void * dispatcher(char* dataIn){
	printf("Dispatcher : se lance\n");
	char* data;
	int err;
	// identifies the sender of the message
	char *sender = parseWord(dataIn, 1);
	char *thirdWord = parseWord(dataIn, 3);
	int clientSocket;
       	//Gets the client's socketID
	int indexClient = indexClientPseudo(sender);
	if(indexClient==-1){
		printf("dispatcher : erreur lors de la recherche d'index avec indexClientPseudo\n");
		//exit(0);
	}

	pthread_mutex_lock(&mutex);

	clientSocket = ClientList[indexClient].sockID;

	pthread_mutex_unlock(&mutex);
	//printf("ici\n");

	if ((strncmp(thirdWord, "/exit", 5)) == 0) {
		data = malloc(sizeof(char)*sizeof("\033[0;37m"));
		strcpy(data, "\033[0;37m");
		err = send(clientSocket, "/exit", sizeof("/exit"), 0);
		if (err == -1){
			fprintf(stderr, "dispatcher : exit failed\n");
		}
		else {
			data = realloc(data, sizeof(char) * (sizeof(sender)+sizeof(" disconnected..\n")));
			strcat(data, sender);
			strcat(data, " disconnected...\n");
			broadcastClient(data);
			//printf("djsjdjdjdjdsjk\n");
			pthread_mutex_lock(&mutex);
			//printf("djksjdhjkjkhddshjsdhjk\n");
			close(clientSocket);
			pthread_mutex_unlock(&mutex);
			removeClient(indexClient);
			pthread_mutex_lock(&mutex);

			if (clientCount == 0){
				if (pthread_create(&timer, NULL, timerThread, NULL) != 0){
					perror("dispatcher : error creation thread");
				}
			}
			pthread_mutex_unlock(&mutex);
			//printf("si c toi batard \n");
		}
		free(data);
	}
	// Allows to list all online clients, and sends it to the client
		
	else if ((strncmp(thirdWord, "/list", 5)) == 0) {
		data = malloc(sizeof(char)*sizeof("\033[0;37m"));
		strcpy(data, "\033[0;37m");
		//printf("fdp\n");
		pthread_mutex_lock(&mutex);
		//printf("salope\n");
		for (int i = 0; i < clientCount; i++) {
			data = realloc(data, sizeof(char) * (sizeof(ClientList[i].pseudo)+sizeof("\n")));
     			strcat(data, ClientList[i].pseudo);
			strcat(data, "\n");
		}
		pthread_mutex_unlock(&mutex);
		//printf("nique ta mere\n");
		err = send(clientSocket, data, strlen(data), 0); // Directly send to the requesting client
		if (err == -1){
			fprintf(stderr, "dispatcher : /list failed\n");
		}
		free(data);
	}

	// Gives helpful commands
	else if ((strncmp(thirdWord, "/help", 5)) == 0) {
		data = malloc(sizeof(char)*sizeof("\033[0;37m"));
		strcpy(data, "\033[0;37m");
		data = realloc(data, sizeof(char) * (sizeof("Possible commands :\n/help to see all possible commands\n\t/list to list all connected clients\n")+sizeof("\t/exit to exit your session\n")+sizeof("\tctrl+c to stop message flow. To see messages again, send a message.\n")));

		strcat(data, "Possible commands :\n\t/help to see all possible commands\n");
		strcat(data, "\t/list to list all connected clients\n");
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
		if(sizeof(dataIn) < 1000){
		//	strcpy(dataIn, "\033[0;37m");
		}
		//printf("hello world\n");
		pthread_mutex_unlock(&mutex);
		//printf("hahahahahahaahahahahahah\n");
		broadcastClient(dataIn);
		pthread_mutex_lock(&mutex);
		//printf("creve connard\n");
	}
	
	pthread_mutex_unlock(&mutex);
	//("je hais la vie\n");

	return NULL;
}


// The clientListener function is used to listen for messages sent by a specific client and properly handle client disconnection.
// Function handeling connexions
void * clientListener(void * ClientDetail){
	//printf("Début du clientListener !\n");
	// Variables
	//sleep(1);
	struct client* clientDetail = (struct client*) ClientDetail;
	
	int clientSocket = clientDetail -> sockID;
		

	int index = indexClientSock(clientSocket);
	//printf("On a passé indexClientSock\n");

	if(index==-1){
		printf("clientListener : la fonction indexClientSock n'a pas fonctionné\n");
		//exit(0);
	}
	//printf("avant le verrou\n");
	pthread_mutex_lock(&mutex);
	//printf("après le verrou\n");
	ClientList[index].pseudo = "default pseudo";
	clientDetail -> grpID = "all";
	char pseudo[254], dataIn[1024], dataOut[1024];
	int num = (index%7) + 91;
	char str[4];
	sprintf(str,"%d",num);
	strcpy(ClientList[index].color,"\033[1;");
	strcat(ClientList[index].color,str);
	strcat(ClientList[index].color,"m");
	
	pthread_mutex_unlock(&mutex);


	// pipe through fork
	int fd[2];
	if (pipe(fd) == -1) {
		printf("Erreur lors de la création du tube");
		//exit(EXIT_FAILURE); 
	}


	// Waits for the pseudonym of the client
	int receved = recv(clientSocket, pseudo, 254, 0);
	//send /help when client is connected to see all commands

	pthread_mutex_lock(&mutex);
	ClientList[index].pseudo = pseudo;
	strcpy(dataOut, ClientList[index].pseudo);
	pthread_mutex_unlock(&mutex);
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
				removeClient(clientDetail->sockID);
				clientDetail -> sockID = 0;
				break;
			}

			// Send message to all clients
			else{
				dispatcher(dataIn);
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
	int activity;
	fd_set readfds;

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
		//printf("avant le verrou de la putain de main de merde\n");
		pthread_mutex_lock(&mutex);
		//printf("dans le verrou de la putain de main de merde\n");
		if (clientCount==0){
			pthread_mutex_unlock(&mutex);
			//printf("ici ??\n");
			if (pthread_create(&timer, NULL, timerThread, NULL) != 0) {
                		perror("Erreur lors de la création du thread du temporisateur");
						close(serverSocket);
						pthread_mutex_destroy(&mutex);
                		return 1;
        	    	}
			pthread_mutex_lock(&mutex);
			//printf("la ??\n");

		}
		
		//printf("on atteint ici ???\n");

		newClient = ClientList[clientCount].len;
		//printf("on atteint ici 2 ???\n");
		pthread_mutex_unlock(&mutex);

		/*
		while (1) {
        // Effacer l'ensemble des descripteurs de fichiers à surveiller
        FD_ZERO(&readfds);

        // Ajouter le socket du serveur à l'ensemble
        FD_SET(serverSocket, &readfds);

		int maxfd = serverSocket;

		for (int i = 0; i < clientCount; i++) {
        FD_SET(ClientList[i].sockID, &readfds);
        if (ClientList[i].sockID > maxfd) {
            maxfd = ClientList[i].sockID;
        }
    }
        // Attendre une activité sur un des descripteurs de fichiers
    activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
    if ((activity < 0) && (errno != EINTR)) {
        perror("Erreur lors de la sélection");
        exit(EXIT_FAILURE);
    }

        // Si une activité est détectée sur le socket du serveur, cela signifie une nouvelle connexion entrante
        if (FD_ISSET(serverSocket, &readfds)) {
            struct sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);
    */
            // Accepter la nouvelle connexion
           /* if (ClientList[clientCount].sockID == -1){
			fprintf(stderr, "cannot connect\n");
			exit(0);
		}
        }
    */
		
		//printf("on atteint ici 3 ???\n");

		ClientList[clientCount].sockID = accept(serverSocket, (struct sockaddr*) &ClientList[clientCount].clientAddr, &newClient);
	if (ClientList[clientCount].sockID == -1){
			fprintf(stderr, "cannot connect\n");
			exit(0);
		}
        
		//printf("on atteint ici 4 ???\n");

		int thr;
		//ClientList[clientCount].index = clientCount + 1;
		
		//printf("Après le verrou de la main de merde\n");
		thr = pthread_create(&thread[clientCount], NULL, clientListener, (void *) &ClientList[clientCount]);
		
		if (thr != 0)
		{
			fprintf(stderr, "can't create thread. \n");
			exit(0);
		}
		pthread_mutex_lock(&mutex);
		//printf("verrou pour compter client dedans\n");
		clientCount ++;
		pthread_mutex_unlock(&mutex);
	}
	pthread_exit(NULL);
	// After chatting close the socket
	close(serverSocket);
}
