#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>

#define BUFFER_SIZE 512

void echo(int fd_in, int fd_out)
{
    char buffer[BUFFER_SIZE];
    ssize_t r;
    ssize_t w;
    ssize_t offset;

    while(1)
    {
        r = read(fd_in, buffer, BUFFER_SIZE-1);
        if(r == 0)
        {
            break;
        }

        if(r == -1)
        {
            fprintf(stderr, "Can't read file\n");
            exit(0);
        }

        offset = 0;

        while(r > 0)
        {
            w = write(fd_out, buffer + offset, r);

            if(w == -1)
            {
                fprintf(stderr, "Can't write file\n");
                exit(0);
            }

            offset += w;
            r -= w;
        }
    }
}
