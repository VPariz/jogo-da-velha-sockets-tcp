# Cross-Platform Makefile for Jogo da Velha (Tic-Tac-Toe) TCP Sockets
CC = gcc
CFLAGS = -Wall -Wextra -O2

ifeq ($(OS),Windows_NT)
    LDFLAGS = -lws2_32
    EXT = .exe
    RM = del /Q /F
else
    LDFLAGS =
    EXT =
    RM = rm -f
endif

TARGETS = servidor_velha$(EXT) cliente_velha$(EXT)

all: $(TARGETS)

servidor_velha$(EXT): servidor_velha.c
	$(CC) $(CFLAGS) servidor_velha.c -o $@ $(LDFLAGS)

cliente_velha$(EXT): cliente_velha.c
	$(CC) $(CFLAGS) cliente_velha.c -o $@ $(LDFLAGS)

clean:
	$(RM) *$(EXT) *.o
