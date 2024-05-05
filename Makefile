# Makefile

CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS =
LDLIBS =

SRC = server.c client.c serverAlt.c
OBJ = ${SRC:.c=.o}

all: server client serverAlt

server: server.o
client: client.o
serverAlt: serverAlt.o

clean:
	${RM} ${OBJ}
	${RM} server client serverAlt

# END
