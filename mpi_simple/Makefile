NAME=mpi-peer

CC=mpicc
CFLAGS=-ggdb -std=gnu99 -Wall

LIBS=-lm

MPI_FLAGS=-n 4
MPI_ARGS=

all: build

run: build
	@./mpirun $(MPI_FLAGS) $(NAME) $(MPI_ARGS)

build: clean
	@echo Building...
	@$(CC) $(CFLAGS) -o $(NAME) *.c $(LIBS)

clean:
	rm -f $(NAME) $(NAME).o
