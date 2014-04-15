#ifndef MATRIX_H
#define MATRIX_H


typedef struct {
    double *a;
    int width, height;
} matrix_t;

int matrix_load(FILE* f, matrix_t* m) {
    fscanf(f, "%d %d", &m->width, &m->height);
    m->a = (double*) malloc(m->width * m->height * sizeof(double));
    double v;

    for (int i=0; i<m->height; i++) {
        for (int j=0; j<m->width; j++) {
            fscanf(f, "%lf", &v);
            m->a[i*m->width + j] = v;
        }
    }

    return 0;
}

int matrix_print(matrix_t* m)
{
    if (m == NULL)
        return -1;
    printf("____________________________________________________\n");
    printf("Matrix width: %d, height: %d\n", m->width, m->height);

    for (int i=0; i<m->height; i++) {
        for (int j=0; j<m->width; j++) {
            printf("%5.2lf ", m->a[i*m->width + j]);
        }
        printf("\n");
    }
    printf("====================================================\n");
}

int matrix_mul(matrix_t *A, matrix_t  *B, matrix_t *C) {
    if (A->width != B->height) {
        error("Matrix sizes doesn't match A=%p, width=%d, height=%d, B=%p, width=%d, height=%d\n", A,
              A->width, A->height, B, B->width, B->height);

        return -1;
    }

    C->height = A->height;
    C->width = B->width;

    C->a = (double*) calloc(C->width*C->height, sizeof(double));

    for (int row = 0; row < A->height; row++) { //row
        for (int column = 0; column < B->width; column++) { //collumn
            C->a[row * C->width + column] = 0;
            for (int k = 0; k < B->height; k++) { //sum
                C->a[row * C->width + column] += A->a[row * A->width + k] * B->a[column + k*B->width];
            }
        }
    }
}

int matrix_cmp(matrix_t *m1, matrix_t *m2) {
    if (m1->width != m2->width || m1->height != m2->height)
        return 1;

    return memcmp(m1->a, m2->a, m1->width * m1->height * sizeof(double));
}


#endif // MATRIX_H
