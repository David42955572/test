CC = gcc
CFLAGS = -Wall -g

SRC = protocol.c storage_server.c transfer_server.c client.c
OBJ = $(SRC:.c=.o)

all: storage transfer client

storage: storage_server.o protocol.o
	$(CC) $(CFLAGS) -o storage storage_server.o protocol.o

transfer: transfer_server.o protocol.o
	$(CC) $(CFLAGS) -o transfer transfer_server.o protocol.o

client: client.o protocol.o
	$(CC) $(CFLAGS) -o client client.o protocol.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o storage transfer client
