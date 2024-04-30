#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>
#include <gmodule.h>
#include <glib.h>
#include <glib/gprintf.h>

#define BUFFER_SIZE 500

int main()
{
    struct addrinfo hints;
    struct addrinfo *addr_list, *addr;
    int socket_id, client_socket_id;
    int res;
    ssize_t request_size;
    char request[BUFFER_SIZE];

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
    printf("Static Server\nListening to port 2048...\n");

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

        //Print the request and free full_request
        printf("%s", full_request->str);

        g_string_free(full_request, TRUE);
        
        //Create & Send a valid response.
        char response[] = "HTTP/1.1 200 OK\r\n\r\nHello World!";
        size_t response_size = strlen(response);
        if (write(client_socket_id, response, response_size) != ((int) response_size))
        {
            fprintf(stderr, "partial/failed write\n");
            exit(0);
        }

        //Close client sockets
        close(client_socket_id);
    }

    //Close server sockets
    close(socket_id);
}
