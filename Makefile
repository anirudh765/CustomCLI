CC       := gcc
CFLAGS   := -Wall -g
LDFLAGS  := -lsqlite3

OBJS     := auto_delete.o main.o
DAEMONOBJS := daemon.o auto_delete.o

all: auto_delete auto_delete_daemon

install: auto_delete auto_delete_daemon
	mkdir -p $(HOME)/bin
	cp auto_delete auto_delete_daemon $(HOME)/bin/

auto_delete: $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

auto_delete.o: auto_delete.c
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

auto_delete_daemon: $(DAEMONOBJS)
	$(CC) $^ $(LDFLAGS) -o $@

daemon.o: daemon.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(DAEMONOBJS) auto_delete auto_delete_daemon
