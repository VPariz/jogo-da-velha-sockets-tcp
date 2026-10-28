# Makefile for Jogo da Velha (Tic-Tac-Toe) TCP Sockets
CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lws2_32

TARGETS = servidor_velha.exe cliente_velha.exe

all: $(TARGETS)

servidor_velha.exe: servidor_velha.c
	$(CC) $(CFLAGS) servidor_velha.c -o servidor_velha.exe $(LDFLAGS)

cliente_velha.exe: cliente_velha.c
	$(CC) $(CFLAGS) cliente_velha.c -o cliente_velha.exe $(LDFLAGS)

clean:
	del /F /Q *.exe *.o 2>NUL || rm -f *.exe *.o
