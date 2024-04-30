#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>
#include <semaphore.h>
#include <gmodule.h>
#include <glib.h>
#include <glib/gprintf.h>

#define BUFFER_SIZE 500

// Grid of the game.
// "_" represents an empty cell.
// "x" represents the cross.
// "o" represents the nought.
char grid[] = "_________";

// Current number of players.
int player_count = 0;

// Semaphore to protect player_count management
sem_t lock;

// Restart pressed.
int restart_pressed = 0;

// Update command
void update_cmd(int client_socket_id)
{
    //Send grid as message content
    if (write(client_socket_id, grid, 9) != 9)
    {
        fprintf(stderr, "partial/failed write\n");
    }
}

// Set command
void set_cmd(int client_socket_id, gchar* resource)
{
    grid[atoi(resource+5)] = resource[4];
    update_cmd(client_socket_id);
}

// Get resource
void get_www_resource(int client_socket_id, gchar* resource)
{
    gchar* file_content;
    gsize response_size;
    GError* error = NULL;
    
    gchar* resource_path = malloc((strlen(resource) + 4) * sizeof(char));
    sprintf(resource_path, "www/%s", resource);

    //Read resource file content
    if(g_file_get_contents(resource_path, &file_content, &response_size, &error) == FALSE)
    {
        if(player_count < 2)
        {
        printf("trc");
            strcpy(resource_path, "www/new_player.html");
        }
        else
        {
            strcpy(resource_path, "www/busy.html");
        }
        g_file_get_contents(resource_path, &file_content, &response_size, &error);
    }
    
    //Send message content
    if (write(client_socket_id, file_content, response_size) != ((int) response_size))
    {
        fprintf(stderr, "partial/failed write\n");
    }
    
    if(error != NULL) g_error_free(error);
    g_free(file_content);
    g_free(resource_path);
}

// Grid command
void grid_cmd(int client_socket_id, gchar* resource)
{
    sem_wait(&lock);

    int name_size = strlen(resource) - 14;
    char player_name[name_size];
    strcpy(player_name, resource+14);

    char symbol;
    if(player_count == 0) symbol = 'x';
    if(player_count == 1) symbol = 'o';

    if(player_count > 1) get_www_resource(client_socket_id, "busy.html");
    else
    {
        player_count++;
        
        gchar* file_content;
        gsize response_size;
        GError* error = NULL;
        
        g_file_get_contents("www/grid.html", &file_content, &response_size, &error);
        
        char* response = g_strdup_printf(file_content, symbol, player_name);
        response_size = strlen(response);
        
        if (write(client_socket_id, response, response_size) != ((int) response_size))
        {
            fprintf(stderr, "partial/failed write\n");
        }
        
        if(error != NULL) g_error_free(error);
        g_free(file_content);
        free(response);
    }
    
    sem_post(&lock);
}

void restart_cmd(int client_socket_id)
{
    sem_wait(&lock);

    if(restart_pressed == 0)
    {
        strcpy(grid, "_________");
        restart_pressed++;
    }
    else restart_pressed = 0;

    update_cmd(client_socket_id);

    sem_post(&lock);
}

// Define the thread function.
void* worker(void* arg)
{
    int client_socket_id = *((int*) arg);
    ssize_t request_size;
    char request[BUFFER_SIZE];

    //Get the request from the web client
    //Loop until full message is read
    GString *full_request = g_string_new("");
    do
    {
        request_size = read(client_socket_id, request, BUFFER_SIZE-1);
        if (request_size == -1)
        {
            perror("read");
            exit(0);
        }
        request[request_size] = '\0';
        full_request = g_string_append(full_request, request);
    } while (request_size > 0 &&
             g_str_has_suffix(full_request->str, "\r\n\r\n") == FALSE);

    //Get resource from the request
    if (g_str_has_prefix(full_request->str, "GET ") == TRUE)
    {
        gchar* resource = g_strndup(full_request->str+5, g_strstr_len(full_request->str, -1, " HTTP/")-full_request->str-5);
         
        //Print resource and free full_request and resource
        printf("%d: %s\n", client_socket_id, resource);

        //Send message status
        char message[] = "HTTP/1.1 200 OK\r\n\r\n";
        send(client_socket_id, message, strlen(message), MSG_MORE);
        
        //Compute and send content message depending on requested resource
        
        //Treat update command
        if(strcmp(resource, "update") == 0) update_cmd(client_socket_id);
        else
        {
            // Treat set command
            if(g_str_has_prefix(resource, "set_") == TRUE) set_cmd(client_socket_id, resource);
            else
            {
                // Treat grid command
                if(g_str_has_prefix(resource, "grid?nickname=") == TRUE) grid_cmd(client_socket_id, resource);
                else
                {
                    // Treat restart command
                    if(strcmp(resource, "restart") == 0) restart_cmd(client_socket_id);
                    else get_www_resource(client_socket_id, resource);
                }
            }
        }

        g_free(resource);

        //Close client sockets
        close(client_socket_id);
    }

    g_string_free(full_request, TRUE);
    
    return NULL;
}

int main()
{
    struct addrinfo hints;
    struct addrinfo *addr_list, *addr;
    int socket_id, client_socket_id;
    int res;

    // Init with a value of 1
    // first 0 is for unused option, keep it this way
    if ( sem_init(&lock, 0, 1) == -1)
        err(1, "Fail to initialized semaphore");
    
    //Get addresses list
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    res = getaddrinfo(NULL, "2048", &hints, &addr_list);

    //If error, exit the program
    if (res != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
        exit(0);
    }


    //Try to connect to each adress returned by getaddrinfo()
    for (addr = addr_list; addr != NULL; addr = addr->ai_next)
    {
        //Socket creation
        socket_id = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

        //If error, try next adress
        if (socket_id == -1)
            continue;

        //Set options on socket
        int enable = 1;
        if (setsockopt(socket_id, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
            perror("setsockopt(SO_REUSEADDR) failed");

        //Bind a name to a socket, exit if no error
        if (bind(socket_id, addr->ai_addr, addr->ai_addrlen) == 0)
            break;

        //Close current not connected socket
        close(socket_id);
    }

    //addr_list freed
    freeaddrinfo(addr_list);

    //If no address works, exit the program
    if (addr == NULL)
    {
        fprintf(stderr, "Could not bind\n");
        exit(0);
    }

    //Specify that the socket can be used to accept incoming connections
    if(listen(socket_id, 5) == -1)
    {
        fprintf(stderr, "Cannot wait\n");
        exit(0);
    }

    //Socket waiting for connections on port 2048
    printf("Tic-Tac-Toe Server\nListening to port 2048...\n");

    //Allow multiple connections
    while(1)
    {
        //Accept connection from a client and exit the program in case of error
        client_socket_id = accept(socket_id, addr->ai_addr, &(addr->ai_addrlen));
        if(client_socket_id == -1)
        {
            fprintf(stderr, "Cannot connect\n");
            exit(0);
        }

        int thread;
        pthread_t thread_id;

        // - Create and execute the thread.
        thread = pthread_create(&thread_id, NULL, &(worker), (void*)&client_socket_id);
        if(thread != 0)
        {
            fprintf(stderr, "hello: Can't create thread.\n");
            exit(0);
        }
    }

    //Close server sockets
    close(socket_id);
    
    sem_destroy(&lock);
}
