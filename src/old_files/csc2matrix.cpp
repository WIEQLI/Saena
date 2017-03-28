//
// Created by abaris on 10/12/16.
//

#include "csc2matrix.h"

CSC2Matrix::CSC2Matrix(int s1, int s2, double** A){

    M = s1;
    N = s2;
    unsigned int nz = 0;

    for(int i=0; i<M; i++){
        for(int j=0; j<N; j++){
            if (A[i][j] > matrixTol){
                nz++;
            }
        }
    }
    nnz = nz;

    values = (double*)malloc(sizeof(double)*nnz);
    rows = (int*)malloc(sizeof(int)*nnz);
    pointerB  = (int*)malloc(sizeof(int)*N);
    pointerE = (int*)malloc(sizeof(int)*N);

    int iter = 0;
    for(int j=0; j<N; j++){
        pointerB[j] = iter;

        for(int i=0; i<M; i++){
            if (A[i][j] > matrixTol){
                values[iter] = A[i][j];
                rows[iter] = i;
                iter++;
            }
        } //for j
        pointerE[j] = iter;
    } //for i

}

CSC2Matrix::~CSC2Matrix()
{
    free(values);
    free(rows);
    free(pointerB);
    free(pointerE);
}