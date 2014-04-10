#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#define TAG_BLOCK_SIZE    2
#define TAG_MATRIX_FIRST  3
#define TAG_MATRIX_RESULT 4


#define debug(format, ...) fprintf(stderr, "[%d] <d>: ", current_rank); fprintf(stderr, format, ##__VA_ARGS__); fflush(stderr);
#define error(format, ...) fprintf(stderr, "[%d] <e>: ", current_rank); fprintf(stderr, format, ##__VA_ARGS__); fflush(stderr);

int async, current_rank, current_size;
#include "matrix.h"

matrix_t A, B, C, C_self;

int load_from_file(const char* file_name) {
    FILE *f = fopen(file_name, "r");
    if (f==NULL) {
        perror("Can't open file");
        return -1;
    }

    matrix_load(f, &A);
    matrix_load(f, &B);
    fclose(f);

    //matrix_print(&A);
    //matrix_print(&B);

    C.height = A.height;
    C.width = B.width;
    C.a = (double*) malloc(C.width * C.height * sizeof(double));
    return 0;
}

int peer_rows_count(const int peer_id) {
    const a_row_count = (int) ceil( (double)A.height / (current_size-1));
    int current_worker_block_size;

    if (peer_id * a_row_count <= A.height) {
        if ((peer_id+1) * a_row_count <= A.height)
            return  a_row_count;
        else if((peer_id+1) * a_row_count >= A.height)
            return A.height - (peer_id * a_row_count);
    }
    else
        return 0;
}

void main_peer_communication() {
    int workers_count = current_size - 1;
    int real_workers_count = 0;

    //send matrix B to all peers
    MPI_Bcast(&B.width, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    MPI_Bcast(&B.height, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    MPI_Bcast(B.a, B.width * B.height, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    //send block_size to all peers
    if (async == 0) {
        for (int i = 0; i < workers_count; i++) {
            int current_worker_block_size = peer_rows_count(i);
            MPI_Send(&current_worker_block_size, 1, MPI_INT, i + 1, TAG_BLOCK_SIZE, MPI_COMM_WORLD);
        }

    } else {
        MPI_Request r;
        for (int i = 0; i < workers_count; i++) {
            int current_worker_block_size = peer_rows_count(i);
            MPI_Isend(&current_worker_block_size, 1, MPI_INT, i + 1, TAG_BLOCK_SIZE, MPI_COMM_WORLD, &r);
        }
    }

    //send rows of matrix A to peers
    debug("Sending matrix A to all peers\n");
    MPI_Bcast(&A.width, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    if (async == 0) {
        for (int i = 0; i < workers_count && peer_rows_count(i); i++) {
            int current_worker_block_size = peer_rows_count(i);
            debug("A row count for %i: %i\n", i+1, current_worker_block_size);
            MPI_Send(A.a + (A.width * i * peer_rows_count(i-1)), current_worker_block_size * A.width, MPI_DOUBLE, i + 1,
                     TAG_MATRIX_FIRST, MPI_COMM_WORLD);
            real_workers_count++;
        }
    } else {
        for (int i = 0; i < workers_count && peer_rows_count(i); i++) {
            int current_worker_block_size = peer_rows_count(i);
            MPI_Request r;
            MPI_Isend(A.a + (A.width * i *  peer_rows_count(i-1)), current_worker_block_size * A.width, MPI_DOUBLE, i + 1,
                      TAG_MATRIX_FIRST, MPI_COMM_WORLD, &r);
            real_workers_count++;
        }
    }

    //receive rows result matrix C
    debug("Trying to compose result matrix. Waiting data from peers.\n");
    if (async == 0) {
        for (int i = 0; i < real_workers_count; i++) {
            int current_worker_block_size = peer_rows_count(i);
            MPI_Recv(C.a +(C.width * i * current_worker_block_size), current_worker_block_size * C.width,
                     MPI_DOUBLE, i + 1, TAG_MATRIX_RESULT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    } else {
        MPI_Request *requests = calloc(real_workers_count, sizeof(MPI_Request));
        MPI_Status *statuses = calloc(real_workers_count, sizeof(MPI_Status));

        for (int i = 0; i < real_workers_count; i++) {
            int current_worker_block_size = peer_rows_count(i);
            MPI_Irecv(C.a +(C.width * i * current_worker_block_size), current_worker_block_size * C.width,
                      MPI_DOUBLE, i + 1, TAG_MATRIX_RESULT, MPI_COMM_WORLD, &requests[i]);
        }
        MPI_Waitall(real_workers_count, requests, statuses);
        free(requests);
        free(statuses);
    }
}

int peer_communication() {
    //receiving full B matix from master-peer
    int a_row_count;
    MPI_Bcast(&B.width, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    MPI_Bcast(&B.height, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    B.a = (double *) malloc(B.width * B.height * sizeof(double));
    MPI_Bcast(B.a, B.width * B.height, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    debug("Matrix B (%p) has been received: width=%d, height=%d\n", B.a, B.width, B.height);

    if (async == 0) {
        MPI_Recv(&a_row_count, 1, MPI_INT, 0, TAG_BLOCK_SIZE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } else {
        MPI_Request request;
        MPI_Status status;
        MPI_Irecv(&a_row_count, 1, MPI_INT, 0, TAG_BLOCK_SIZE, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
    }

    debug("block_size has been received: %d\n", a_row_count);
    if (a_row_count == 0) {
        debug("Nothing to do. Waiting for another peers.\n");
        return 0;
    }
    A.height = a_row_count;

    //**********************************************************
    //receiving matrix A
    MPI_Bcast(&A.width, 1, MPI_INTEGER, 0, MPI_COMM_WORLD);
    size_t peer_mem = A.height * A.width * sizeof(double);
    A.a = (double*) malloc(peer_mem);

    if (async == 0) {
        MPI_Recv(A.a, A.height * A.width, MPI_DOUBLE, 0, TAG_MATRIX_FIRST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } else {
        MPI_Request request;
        MPI_Status status;
        MPI_Irecv(A.a, A.height * A.width, MPI_DOUBLE, 0, TAG_MATRIX_FIRST, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
    }
    debug("Matrix A(%p) has been received from main peer: width=%d, height=%d\n", A.a, A.width, A.height);
    //matrix_print(&A);

    //************************************************************
    matrix_mul(&A, &B, &C);
    debug("Matrix multiple found\n");
    //matrix_print(&C);

    if (async == 0) {
        MPI_Send(C.a, C.height * C.width, MPI_DOUBLE, 0, TAG_MATRIX_RESULT, MPI_COMM_WORLD);
    } else {
        MPI_Request request;
        MPI_Status status;
        MPI_Isend(C.a, C.height * C.width, MPI_DOUBLE, 0, TAG_MATRIX_RESULT, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
    }

    free(A.a);
    free(B.a);
    free(C.a);

    debug("Peer computing done. Results synced.\n");

    return 0;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (argc < 3) {
        printf("Params: <source_file> <mode>\n");

        printf("Mode: 0 - sync\n");
        printf("      1 - async\n");
        return 0;
    }

    double start_time, end_time;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &current_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &current_size);

    if (current_size<2) {
        error("At least 2 peers may instantiated and work properly!\n");
        return -2;
    }

    async = atoi(argv[2]);

    int status;

    if (current_rank == 0) {
        load_from_file(argv[1]);
        matrix_mul(&A, &B, &C_self);
        start_time = MPI_Wtime();
        main_peer_communication();
        end_time = MPI_Wtime();
        //printf("Duration is %f\n", end_time - start_time);
        status = matrix_cmp(&C, &C_self);
        printf("Matrix_cmp status = %d\n", status);

        matrix_print(&C_self);
        matrix_print(&C);

    } else {
        status = peer_communication();
    }

    MPI_Finalize();
    return status;
}
