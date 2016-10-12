//
// Created by abaris on 10/11/16.
//

#include "cssmatrix.h"

CSSMatrix::CSSMatrix(int s1, int s2, double** A){

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

    values      = (double*)malloc(sizeof(double)*nnz);
    rows        = (int*)malloc(sizeof(int)*nnz);
    colIndex    = (int*)malloc(sizeof(int)*(N+1));

    colIndex[0] = 0;
    int iter = 0;
    for(int j=0; j<N; j++){
        for(int i=0; i<M; i++){
            if (A[i][j] > matrixTol){
                values[iter] = A[i][j];
                rows[iter] = i;
                iter++;
            }
        } //for j
        colIndex[j+1] = iter;
    } //for i

}

CSSMatrix::~CSSMatrix()
{
    free(values);
    free(rows);
    free(colIndex);
}