# Makefile

CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS =
LDLIBS =

SRC = server.c client.c
OBJ = ${SRC:.c=.o}

all: server client

server: server.o
client: client.o

clean:
	${RM} ${OBJ}
	${RM} server client

# END
