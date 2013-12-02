BIN:= server client
SERVER_SRC:=common.c server.c tcp.c udp.c
CLIENT_SRC:= common.c client.c

########################################################
SERVER_OBJS:= $(patsubst %.c, out/%.o, $(SERVER_SRC)) 
CLIENT_OBJS:= $(patsubst %.c, out/%.o, $(CLIENT_SRC))
########################################################

.PHONY: clean all

all: client server

server: $(SERVER_OBJS)	
	$(CC) $(SERVER_OBJS) -o out/$@

client: $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o out/$@
	
%.o: 
	$(CC) $(patsubst out/%.o,  %.c, $@) -c -o $@ -g --std=c99


out:
	mkdir -p out

clean: 
	rm -f out/*
