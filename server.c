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
#include <signal.h>

#define MAX 160
#define PORT 8080
#define SA struct sockaddr
#define TIMEOUT_SECONDS 20

int clientCount = 0, top = -1, sockCount = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
bool isServRunning = true;
static pthread_t timer;
static pthread_t recevoirData;
int serverSocket;
int sockList[1024];

struct ThreadData
{
	int *sockId;
	int *fd;
};

struct client
{
	int sockID;
	char *grpID;
	char *pseudo;
	struct sockaddr_in clientAddr;
	int len;
	char color[sizeof("\033[0;30m")];
};

struct client ClientList[1024];
pthread_t thread[1024];

int indexClientSock(int sockID)
{
	pthread_mutex_lock(&mutex);
	int index = -1;
	if (clientCount == 0)
	{
		perror("IndexClientSock : pas de client connecté");
	}
	else
	{
		for (int i = 0; i < clientCount; i++)
		{
			if (ClientList[i].sockID == sockID)
			{
				index = i;
				break;
			}
		}
	}
	if (index == -1)
	{
		perror("indexClientSock : client not found");
		return -1;
	}
	pthread_mutex_unlock(&mutex);
	return index;
}

void closeServer()
{
	isServRunning = false;
	pthread_mutex_destroy(&mutex);
	// Fermeture de toutes les connexions existantes et passées
	for (int i = 0; i < sockCount; i++)
	{
		close(sockList[i]);
	}
	close(serverSocket);
	exit(0);
}

int indexClientPseudo(char *pseudo)
{
	pthread_mutex_lock(&mutex);

	int index = -1;
	if (clientCount == 0)
	{
		perror("indexClientPseudo : pas de client connecté");
	}
	else
	{
		for (int i = 0; i < clientCount; i++)
		{
			if (strcmp(ClientList[i].pseudo, pseudo) == 0)
			{
				index = i;
				break;
			}
		}
	}
	pthread_mutex_unlock(&mutex);
	if (index == -1)
	{
		printf("indexClientPseudo : client non trouvé !\n");
	}
	return index;
}

void *timerThread();

int removeClient(int index)
{
	pthread_mutex_lock(&mutex);
	close(ClientList[index].sockID);
	if (clientCount == 0)
	{
		return -1;
	}
	if (index < 0 || index >= clientCount)
	{
		return -1;
	}
	for (int i = index; i < clientCount - 1; i++)
	{
		ClientList[i] = ClientList[i + 1];
	}
	clientCount--;
	if(clientCount==0){
		pthread_mutex_unlock(&mutex);
		if (pthread_create(&timer, NULL, timerThread, NULL) != 0)
					{
						perror("dispatcher : error creation thread");
					}
	}
	
	pthread_mutex_unlock(&mutex);
	return 0;
}

void *timerThread()
{
	printf("If in 20 seconds, no client is connected to the server, it will automatically shutdown ...\n");
	sleep(TIMEOUT_SECONDS);

	pthread_mutex_lock(&mutex);

	if (clientCount == 0)
	{
		printf("Aucun client connecté. Arrêt du serveur...\n");
		closeServer();
	}
	pthread_mutex_unlock(&mutex);

	return NULL;
}
void broadcastClient(char *dataOut);

bool isClientConnected(int sockID)
{
	pthread_mutex_lock(&mutex);
	bool isConnected = false;
	if (clientCount != 0)
	{
		for (int i = 0; i < clientCount; i++)
		{
			if (ClientList[i].sockID == sockID)
			{
				isConnected = true;
				break;
			}
		}
	}
	pthread_mutex_unlock(&mutex);

	return isConnected;
}

void handleClientDisconnect(int sockID) {
    
	int clientIndex = indexClientSock(sockID);
    close(sockID);
	char *str = malloc(sizeof(char)*(sizeof(" : has deconnected\n")+sizeof(ClientList[clientIndex].pseudo)));
	
	strcpy(str, ClientList[clientIndex].pseudo);
	strcat(str, " : has deconnected\n");
	removeClient(clientIndex);
	broadcastClient(str);
	
    
}

// sleep() only works in seconds, msleep works in miliseconds
int msleep(unsigned int tms)
{
	return usleep(tms * 1000);
}

// Allows to catch a specific word of the data send by the user
char *parseWord(const char str[], size_t pos)
{
	const char delim[] = " \t";
	char *inputCopy = malloc((strlen(str) + 1));
	char *p = NULL;

	if (inputCopy != NULL)
	{
		strcpy(inputCopy, str);
		p = strtok(inputCopy, delim);

		while (p != NULL && pos-- != 0)
		{
			p = strtok(NULL, delim);
		}
		if (p != NULL)
		{
			size_t n = strlen(p);
			memmove(inputCopy, p, n + 1);

			p = realloc(inputCopy, n + 1);
		}
		if (p == NULL)
		{
			free(inputCopy);
		}
	}
	return p;
}

// The broadcastClient function is used to send a message to all clients connected to the instant messaging server.
// *dataOut is the message to broadcast to all connected clients.

void broadcastClient(char *dataOut)
{
	pthread_mutex_lock(&mutex);

	for (int i = 0; i < clientCount; i++)
	{
		if (send(ClientList[i].sockID, dataOut, 1024, 0) == -1)
		{
			fprintf(stderr, "broadcastClient : error sending messages \n");
		}
	}
	pthread_mutex_unlock(&mutex);
	printf("%s", dataOut);
}

// The Dispatcher function ensures the reception, analysis and distribution of messages between clients
void *dispatcher(char *dataIn)
{
	char *data;
	int err;
	// identifies the sender of the message
	char *sender = parseWord(dataIn, 1);
	char *thirdWord = parseWord(dataIn, 3);
	int clientSocket;
	// Gets the client's socketID
	int indexClient = indexClientPseudo(sender);
	if (indexClient != -1)
	{
		pthread_mutex_lock(&mutex);

		clientSocket = ClientList[indexClient].sockID;

		pthread_mutex_unlock(&mutex);

		if ((strncmp(thirdWord, "/exit", 5)) == 0)
		{
			data = malloc(sizeof(char) * sizeof("\033[0;37m"));
			if(data != NULL){
			strcpy(data, "\033[0;37m");
			err = send(clientSocket, "/exit", sizeof("/exit"), 0);
			if (err == -1)
			{
				fprintf(stderr, "dispatcher : exit failed\n");
			}
			else
			{
				data = realloc(data, sizeof(char) * (sizeof(sender) + sizeof(" disconnected..\n")));
				if(data != NULL){
				strcat(data, sender);
				strcat(data, " disconnected...\n");
				broadcastClient(data);
				pthread_mutex_lock(&mutex);
				close(clientSocket);
				pthread_mutex_unlock(&mutex);
				removeClient(indexClient);
				pthread_mutex_lock(&mutex);
				pthread_mutex_unlock(&mutex);
				}
			}
			free(data);
			}
			if (data == NULL)
			{
				fprintf(stderr, "dispatcher : /exit failed\n");
			}
		}
		// Allows to list all online clients, and sends it to the client

		else if ((strncmp(thirdWord, "/list", 5)) == 0)
		{
			data = malloc(sizeof(char) * sizeof("\033[0;37m"));
			if(data != NULL){
			strcpy(data, "\033[0;37m");
			pthread_mutex_lock(&mutex);
			for (int i = 0; i < clientCount; i++)
			{
				data = realloc(data, sizeof(char) * (sizeof(ClientList[i].pseudo) + sizeof("\n")));
				if(data != NULL){
				strcat(data, ClientList[i].pseudo);
				strcat(data, "\n");
				}else{break;}
			}
			pthread_mutex_unlock(&mutex);
			if(data != NULL){
			err = send(clientSocket, data, strlen(data), 0); // Directly send to the requesting client
			if (err == -1)
			{
				fprintf(stderr, "dispatcher : /list failed\n");
			}
			free(data);
			}
			}
			if ( data == NULL)
			{
				fprintf(stderr, "dispatcher : /listfailed\n");
			}
		}

		// Gives helpful commands
		else if ((strncmp(thirdWord, "/help", 5)) == 0)
		{
			err = 0;
			data = malloc(sizeof(char) * sizeof("\033[0;37m"));
			if(data != NULL){

				strcpy(data, "\033[0;37m");
				data = realloc(data, sizeof(char) * (sizeof("\033[0;37m Possible commands :\n/help to see all possible commands\n\t/list to list all connected clients\n") + sizeof("\t/exit to exit your session\n") + sizeof("\tctrl+c to stop message flow. To see messages again, send a message.\n")));

				strcat(data, "Possible commands :\n\t/help to see all possible commands\n");
				strcat(data, "\t/list to list all connected clients\n");
				strcat(data, "\t/exit to exit your session\n");
				strcat(data, "\tctrl+c to stop message flow. To see messages again, send a message.\n");
				err = send(clientSocket, data, strlen(data), 0); // Directly send to the requesting client
				free(data);
			}
			if (err == -1 || data == NULL)
			{
				fprintf(stderr, "dispatcher : /help failed\n");
			}
		}

		// Sends message to all connected clients
		else
		{
			pthread_mutex_unlock(&mutex);
			broadcastClient(dataIn);
			pthread_mutex_lock(&mutex);
		}

		pthread_mutex_unlock(&mutex);
	}
	else
		printf("dispatcher : erreur lors de la recherche d'index avec indexClientPseudo\n");
	return NULL;
}

void *recupereData(void *arg)
{

	struct ThreadData *data = (struct ThreadData *)arg;
	int *sockId = data->sockId;
	int *fd = data->fd;
	int sockID = *sockId;
	char dataIn[1024], dataOut[1024];
	int index = indexClientSock(sockID);

	while (isClientConnected(sockID))
	{
		bzero(dataIn, 1024);
		bzero(dataOut, 1024);
		// Always & forever listen
		int bytesReceived = recv(sockID, dataIn, sizeof(dataIn), 0);
        if (bytesReceived <= 0) {
            // Erreur ou déconnexion du client
            if (bytesReceived == 0) {
				handleClientDisconnect(sockID);
				pthread_exit(NULL);

            } //else {
               // printf("Erreur de réception des données du client.\n");
            //}
            // Gérer la déconnexion du client
            
		} else {
		dataIn[bytesReceived] = '\0';
		strcpy(dataOut, ClientList[index].color);
		strcat(dataOut, "\t");
		strcat(dataOut, ClientList[index].pseudo);
		strcat(dataOut, " : ");
		strcat(dataOut, dataIn);
		// Sends data through the pipe
		pthread_mutex_lock(&mutex);
		if(write(fd[1], dataOut, sizeof(dataOut))==-1){
			printf("Erreur écriture dans le pipe\n");
			closeServer();
		}
	
		pthread_mutex_unlock(&mutex);
		}
	}

	pthread_exit(NULL);
}

// The clientListener function is used to listen for messages sent by a specific client and properly handle client disconnection.
// Function handeling connexions
void *clientListener(void *ClientDetail)
{
	// Variables
	struct client *clientDetail = (struct client *)ClientDetail;

	int clientSocket = clientDetail->sockID;
	int fd[2];
	int index = indexClientSock(clientSocket);

	if (index == -1)
	{
		printf("clientListener : la fonction indexClientSock n'a pas fonctionné\n");
		// exit(0);
	}
	pthread_mutex_lock(&mutex);
	ClientList[index].pseudo = "default pseudo";
	clientDetail->grpID = "all";
	char pseudo[254], dataIn[1024], dataOut[1024];
	int num = (index % 7) + 91;
	char str[4];
	sprintf(str, "%d", num);
	strcpy(ClientList[index].color, "\033[1;");
	strcat(ClientList[index].color, str);
	strcat(ClientList[index].color, "m");

	pthread_mutex_unlock(&mutex);

	if (pipe(fd) == -1)
	{
		printf("Error during tube creation");
		// exit(EXIT_FAILURE);
	}

	// Waits for the pseudonym of the client
	int receved = recv(clientSocket, pseudo, 254, 0);
	if (receved == -1)
	{
		printf("Error during the reception");
		closeServer();
	}
	// send /help when client is connected to see all commands

	pthread_mutex_lock(&mutex);
	ClientList[index].pseudo = pseudo;
	strcpy(dataOut, ClientList[index].pseudo);
	pthread_mutex_unlock(&mutex);
	strcat(dataOut, " is connected !\n");

	broadcastClient(dataOut);

	int arg = clientDetail->sockID;
	struct ThreadData data;
	data.sockId = &arg;
	data.fd = fd;

	if (pthread_create(&recevoirData, NULL, recupereData, (void *)&data) != 0)
	{
		printf("Erreur lors de la création du thread");
	}
	int r;
	// Parent process | its goal is to parse the data given by the listener
	
	while (isClientConnected(clientDetail->sockID))
	{

		bzero(dataIn, 1024);
		bzero(dataOut, 1024);
		r = read(fd[0], dataIn, sizeof(dataIn));
		// Allows client to exit
		if (r == -1)
		{
			msleep(1000);
			close(clientSocket);
			removeClient(clientDetail->sockID);
			clientDetail->sockID = 0;
			break;
		}

		// Send message to all clients
		else
		{
			dispatcher(dataIn);
		}
	}
	pthread_exit(NULL); //-> mettre pthread exit ne vire pas l'utilisateur quand il quitte (wtf)
	return NULL;
}

// The main function initializes and configures the instant messaging server
// listens for incoming client connections,
// creates threads to handle each client connection, and manages message distribution between clients via the dispatcher thread
int main()
{
	struct sockaddr_in servaddr;

	// Socket create & verification
	int serverSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1)
	{
		printf("socket creation failed...\n");
		return EXIT_FAILURE;
	}
	bzero(&servaddr, sizeof(servaddr));

	// Assign IP, PORT, PROTOCOL
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);

	// Binding & listening verification
	if (bind(serverSocket, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
	{
		perror("error binding...");
		return EXIT_FAILURE;
	}
	if (listen(serverSocket, 1024) == -1)
	{
		perror("error listening...");
		return EXIT_FAILURE;
	}
	else
		printf("Server listening...\n");

	// Threads each new connection to the server, and links a struct to each client
	unsigned int newClient;
	while (isServRunning)
	{
		pthread_mutex_lock(&mutex);
		if (clientCount == 0)
		{
			pthread_mutex_unlock(&mutex);
			if (pthread_create(&timer, NULL, timerThread, NULL) != 0)
			{
				perror("Erreur lors de la création du thread du temporisateur");
				close(serverSocket);
				pthread_mutex_destroy(&mutex);
				return 1;
			}
			pthread_mutex_lock(&mutex);
		}

		newClient = ClientList[clientCount].len;
		pthread_mutex_unlock(&mutex);

		while (isServRunning)
		{
			int newClientSocket = accept(serverSocket, (struct sockaddr *)&ClientList[clientCount].clientAddr, &newClient);
			if (newClientSocket == -1)
			{
				perror("Erreur lors de l'acceptation de la connexion");
				continue; 
			}

			pthread_mutex_lock(&mutex);

			ClientList[clientCount].sockID = newClientSocket;

			sockList[sockCount] = newClientSocket;

			sockCount++;
			clientCount++;
			pthread_mutex_unlock(&mutex);

			// Créer un thread pour gérer la connexion du nouveau client
			
			int thr = pthread_create(&thread[clientCount - 1], NULL, clientListener, (void *)&ClientList[clientCount - 1]);
			if (thr != 0)
			{
				fprintf(stderr, "Erreur lors de la création du thread client\n");
				exit(EXIT_FAILURE);
			}
		}

		pthread_exit(NULL);
		close(serverSocket);
	}
}
