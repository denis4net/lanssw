
SRC:=server.c client.c

.PHONY: clean

all: $(patsubst %.c, out/%, $(SRC))

out/%: out $(SRC)
	$(CC) $(patsubst out/%, %.c, $@) -o $@

out:
	mkdir -p out

clean: 
	rm -f out/*
