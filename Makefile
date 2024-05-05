# Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99
LDFLAGS = -pthread
LDLIBS = -lm -lpthread

SRC = server.c client.c
OBJ = ${SRC:.c=.o}

all: server client

server: server.o
client: client.o

clean:
	${RM} ${OBJ}
	${RM} server client

# END
