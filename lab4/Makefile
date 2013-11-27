BIN:= server client
SERVER_SRC:=common.c server.c
CLIENT_SRC:= common.c client.c

########################################################
SERVER_OBJS:= $(patsubst out/%, %.c, $(SERVER_SRC)) 
CLIENT_OBJS:= $(patsubst out/%, %.c, $(CLIENT_SRC))
########################################################

.PHONY: clean

all: server client

%.c: 
	$(CC) $(patsubst out/%, $@, out/%.c) -o $@ -g

server: $(SERVER_OBJS)	
	
client: $(CLIENT_OBJS)

out:
	mkdir -p out

clean: 
	rm -f out/*
