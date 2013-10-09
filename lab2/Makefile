
.PHONY: clean

all: out/server out/client
	

out/server: out
	$(CC) server.c -g -o out/server
out/client: out
	$(CC) client.c -g  -o out/client

out:
	mkdir -p out

clean: 
	rm -f lab1
