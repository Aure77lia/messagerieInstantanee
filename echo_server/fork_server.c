#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <err.h>
#include <signal.h>
#include "echo.h"

int main(int argc, char** argv)
{
    if (argc != 2)
        errx(0, "Usage:\n"
            "Arg 1 = Port number (e.g. 2048)");

    struct addrinfo hints;
    struct addrinfo *addr_list, *addr;
    int socket_id, client_socket_id;
    int res;
    pid_t child;

    //Get addresses list
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    res = getaddrinfo(NULL, argv[1], &hints, &addr_list);

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

    printf("Waiting for connections...\n");

    //On SIGCHLD signal, call signal to properly exit the forked process
    signal(SIGCHLD, SIG_IGN);

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

        //Creates a new process, exit if it does not work
        child = fork();
        if(child == -1)
        {
            fprintf(stderr, "Fork error\n");
            exit(0);
        }

        //Child process
        if(child == 0)
        {
            //Close server sockets
            close(socket_id);

            printf("New connection (pid = %i)\n", getpid());

            //Use echo() function on the client socket
            echo(client_socket_id, client_socket_id);

            //Close client sockets
            close(client_socket_id);

            printf("Close connection (pid = %i)\n", getpid());

            //Exit the child process
            exit(0);
        }

        //Parent process
        //Close client socket
        close(client_socket_id);
    }

    //Close server sockets
    close(socket_id);
}
