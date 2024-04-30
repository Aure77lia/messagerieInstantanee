# Makefile

CPPFLAGS = -MMD
CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS =
LDLIBS =

SRC = echo.c echo_test.c single_server.c loop_server.c fork_server.c
OBJ = ${SRC:.c=.o}
DEP = ${SRC:.c=.d}

all: echo_test single_server loop_server fork_server

echo_test: echo_test.o echo.o
single_server: single_server.o echo.o
loop_server: loop_server.o echo.o
fork_server: fork_server.o echo.o

-include ${DEP}

.PHONY: clean

clean:
	${RM} ${OBJ}
	${RM} ${DEP}
	${RM} echo_test single_server loop_server fork_server

# END
