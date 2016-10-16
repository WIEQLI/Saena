//
// Created by abaris on 10/14/16.
//

#include "coomatrix.h"

COOMatrix::COOMatrix(int s1, int s2, double** A, int p, int rank) {

    M = s1;
    N = s2;
    unsigned int nz = 0;

    int vSize = M;
    int vStart = rank * vSize;
    int vEnd = vStart + vSize;

    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            if (A[i][j] > matrixTol) {
                nz++;
            }
        }
    }
    nnz = nz;

    values = (double *) malloc(sizeof(double) * nnz);
    row = (int *) malloc(sizeof(int) * nnz);
    col = (int *) malloc(sizeof(int) * nnz);
    proc = (int *) malloc(sizeof(int) * nnz);

    int procNo = -1 ;
    int iter = 0;
    for (int j = 0; j < N; j++) {
        if(j%vSize == 0) procNo++;
        for (int i = 0; i < M; i++) {
            if (A[i][j] > matrixTol) {
                values[iter] = A[i][j];
                row[iter] = i;
                col[iter] = j;
                proc[iter] = procNo;
                iter++;
            }
        } //for i
    } //for j


    vElement = (int *) malloc(sizeof(int) * nnz);
    vElement[0] = col[0];
    vElementSize = 1;

    vProcess = (int *) malloc(sizeof(int) * nnz);
    // fix this part:  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    vProcess[0] = 0;

    for (unsigned int colIter = 1; colIter < nnz; ++colIter) {

        if (vElement[vElementSize-1] != col[colIter]){
            vElement[vElementSize] = col[colIter];
            vProcess[vElementSize] = proc[colIter];
            vElementSize++;
        }
    }
}

COOMatrix::~COOMatrix()
{
    free(values);
    free(row);
    free(col);
    free(vElement);
    free(vProcess);
}

void COOMatrix::matvec(double* v, double* w, int M, int N){
    for(unsigned int i=0;i<nnz;i++) {
        w[row[i]] += values[i] * col[i];
        //w[i] += values[j] * v[row[j]];
    }
}

void COOMatrix::valprint(){
    cout << "val:" << endl;
    for(unsigned int i=0;i<nnz;i++) {
        cout << values[i] << endl;
    }
}

void COOMatrix::rowprint(){
    cout << "row:" << endl;
    for(unsigned int i=0;i<nnz;i++) {
        cout << row[i] << endl;
    }
}

void COOMatrix::colprint(){
    cout << "col:" << endl;
    for(unsigned int i=0;i<nnz;i++) {
        cout << col[i] << endl;
    }
}

void COOMatrix::vElementprint(){
    cout << "vElement:" << endl;
    for(unsigned int i=0;i<vElementSize;i++) {
        cout << vElement[i] << endl;
    }
}

void COOMatrix::vProcessprint(){
    cout << "vProcess:" << endl;
    for(unsigned int i=0;i<vElementSize;i++) {
        cout << vProcess[i] << endl;
    }
}

void COOMatrix::print(){
    cout << "triple:" << endl;
    for(unsigned int i=0;i<nnz;i++) {
        cout << "(" << row[i] << " , " << col[i] << " , " << values[i] << ")" << endl;
    }
}