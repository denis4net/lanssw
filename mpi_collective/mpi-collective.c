#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#define TAG_BLOCK_SIZE    2
#define TAG_MATRIX_FIRST  3
#define TAG_MATRIX_RESULT 4


#define debug(format, ...) fprintf(stderr, "[%i:%d] <d>: ", color, current_rank); fprintf(stderr, format, ##__VA_ARGS__); fflush(stderr);
#define error(format, ...) fprintf(stderr, "[%i%d] <e>: ", color, current_rank); fprintf(stderr, format, ##__VA_ARGS__); fflush(stderr);

int async, current_rank, current_size, color;
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

    C.height = A.height;
    C.width = B.width;
    C.a = (double*) malloc(C.width * C.height * sizeof(double));
    return 0;
}

int max_row_count() {
    return (int) ceil( (double)A.height / (current_size));
}

int peer_rows_count(const int peer_id) {
    const a_row_count = max_row_count();

    if (peer_id * a_row_count <= A.height) {
        if ((peer_id+1) * a_row_count <= A.height)
            return  a_row_count;
        else if((peer_id+1) * a_row_count >= A.height)
            return A.height - (peer_id * a_row_count);
    }
    else
        return 0;
}

int group_peer_communication(MPI_Comm comm) {
    matrix_t pA, pB, pC;

    //receiving A.height by scalability reasons
    MPI_Bcast(&A.height, 1, MPI_INTEGER, 0, comm);
    MPI_Bcast(&A.width, 1, MPI_INTEGER, 0, comm);
    pA.width = A.width;

    int a_row_count = peer_rows_count(current_rank);
    pA.height = a_row_count;
    pA.a = (double*) calloc(pA.height * pA.width, sizeof(double));

    //receiving full B matix from master-peer
    MPI_Bcast(&B.width, 1, MPI_INTEGER, 0, comm);
    MPI_Bcast(&B.height, 1, MPI_INTEGER, 0, comm);

    if (current_rank != 0)
        B.a = (double *) malloc(B.width * B.height * sizeof(double));

    MPI_Bcast(B.a, B.width * B.height, MPI_DOUBLE, 0, comm);

    debug("pA: w=%i, h=%i; B: w=%i, h=%i\n", pA.width, pA.height, B.width, B.height);

    int sendcounts[current_size];
    int displ[current_size];

    if (current_rank == 0) {
        memset(sendcounts, 0x0, sizeof(sendcounts));
        memset(displ, 0x0, sizeof(displ));

        for (int i=0; i < current_size; i++) {
            sendcounts[i] = peer_rows_count(i) * A.width;
            displ[i] = sendcounts[i] == 0  ? 0 : i * peer_rows_count(i-1) * A.width;
            debug("Scatterv %i %i\n", sendcounts[i], displ[i]);
        }
    }

    MPI_Scatterv(A.a, sendcounts, displ, MPI_DOUBLE,
                 pA.a, pA.width * pA.height, MPI_DOUBLE,
                 0, comm);

    //matrix_print(&pA);
    matrix_mul(&pA, &B, &pC);
    //matrix_print(&pC);

    int recvcount[current_size];
    int rdispl[current_size];

    if (current_rank == 0) {
        for (int i=0; i<current_size; i++) {
            //We should use C.width instead of B.widht there, but I wan't to bcast C.width to all peers
            recvcount[i] = peer_rows_count(i) * B.width;
            rdispl[i] = peer_rows_count(i-1) * B.width * i;
            debug("Gatherv %i %i\n", recvcount[i], rdispl[i]);
        }
    }

    MPI_Gatherv(pC.a, pC.width * pC.height, MPI_DOUBLE,
                C.a, recvcount, rdispl, MPI_DOUBLE,
                0, comm);

    free(pA.a);
    free(pC.a);

    return 0;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (argc < 3) {
        printf("Params: <source_file> <groups count>\n");
        return 0;
    }

    int groups_count = atoi( argv[2] );

    MPI_Init(&argc, &argv);
    int g_rank, g_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &g_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &g_size);

    if (g_size<2) {
        error("At least 2 peers may instantiated and work properly!\n");
        return -2;
    }

    //coloring peers
    color = rand() % (g_rank+1);
    color = color % groups_count;

    int abs_rank = 1;
    MPI_Comm comm;
    MPI_Comm_split(MPI_COMM_WORLD, color, abs_rank, &comm);

    int status;

    //group logic
    MPI_Comm_rank(comm, &current_rank);
    MPI_Comm_size(comm, &current_size);

    double start_time, end_time;
    if (current_rank == 0) {
        load_from_file(argv[1]);
        matrix_mul(&A, &B, &C_self);
        start_time = MPI_Wtime();
    }

    group_peer_communication(comm);

    if (current_rank == 0) {
        status = matrix_cmp(&C, &C_self);
        end_time = MPI_Wtime();
        printf("GroupID: %d, Processes in group: %d, matrix_cmp: %d, Computation duration: %lf\n", comm, current_size, status, end_time - start_time);
    }

    MPI_Finalize();
    return status;
}
