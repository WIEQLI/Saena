#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <mpi.h>
#include "strength_matrix.h"

using namespace std;

// sort indices and store the ordering.
class sort_indices
{
private:
    index_t *mparr;
public:
    sort_indices(index_t *parr) : mparr(parr) {}
    bool operator()(index_t i, index_t j) const { return mparr[i]<mparr[j]; }
};

// binary search tree using the lower bound
template <class T>
T lower_bound2(T *left, T *right, T val) {
    T* first = left;
    while (left < right) {
        T *middle = left + (right - left) / 2;
        if (*middle < val){
            left = middle + 1;
        }
        else{
            right = middle;
        }
    }
    if(val == *left){
        return distance(first, left);
    }
    else
        return distance(first, left-1);
}


int strength_matrix::strength_matrix_set(std::vector<index_t> &r, std::vector<index_t> &c, std::vector<value_t > &v, index_t m1, index_t m2, nnz_t m3, std::vector<index_t> &spl, MPI_Comm com){

    comm = com;
    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    M = m1;
    Mbig = m2;
    nnz_l = m3;
    split = spl;

    long procNum;
    unsigned long i;
    col_remote_size = 0; // number of remote columns
    nnz_l_local = 0;
    nnz_l_remote = 0;
//    int recvCount[nprocs];
    int* recvCount = (int*)malloc(sizeof(int)*nprocs);
    std::fill(recvCount, recvCount + nprocs, 0);
    nnzPerRow.assign(M,0);
    nnzPerRow_local.assign(M,0);

    // take care of the first element here, since there is "col[i-1]" in the for loop below, so "i" cannot start from 0.
    nnzPerRow[r[0]-split[rank]]++;
    if (c[0] >= split[rank] && c[0] < split[rank + 1]) {
        nnzPerRow_local[r[0]-split[rank]]++;
        nnz_l_local++;

        values_local.push_back(v[0]);
        row_local.push_back(r[0]);
        col_local.push_back(c[0]);

        //vElement_local.push_back(col[0]);
        vElementRep_local.push_back(1);

    } else{
        nnz_l_remote++;

        values_remote.push_back(v[0]);
        row_remote.push_back(r[0]);
        col_remote_size++; // number of remote columns
        col_remote.push_back(col_remote_size-1);
        col_remote2.push_back(c[0]);
//        nnz_col_remote[col_remote_size-1]++;
        nnz_col_remote.push_back(1);

        vElement_remote.push_back(c[0]);
        vElementRep_remote.push_back(1);
        recvCount[lower_bound2(&split[0], &split[nprocs], c[0])] = 1;
    }

    for (i = 1; i < nnz_l; i++) {
        nnzPerRow[r[i]-split[rank]]++;

        if (c[i] >= split[rank] && c[i] < split[rank+1]) {
            nnzPerRow_local[r[i]-split[rank]]++;
            nnz_l_local++;

            values_local.push_back(v[i]);
            row_local.push_back(r[i]);
            col_local.push_back(c[i]);

//            if (col[i] != col[i - 1]) {
//                vElementRep_local.push_back(1);
//            } else {
//                (*(vElementRep_local.end()-1))++;
//            }
        } else {
            nnz_l_remote++;
            values_remote.push_back(v[i]);
            row_remote.push_back(r[i]);
            // col_remote2 is the original col value and will be used in making strength matrix. col_remote will be used for matevec.
            col_remote2.push_back(c[i]);

            if (c[i] != c[i - 1]) {
                col_remote_size++;
                vElement_remote.push_back(c[i]);
                vElementRep_remote.push_back(1);
                procNum = lower_bound2(&split[0], &split[nprocs], c[i]);
                recvCount[procNum]++;
                nnz_col_remote.push_back(1);
            } else {
                (*(vElementRep_remote.end()-1))++;
                (*(nnz_col_remote.end()-1))++;
            }
            // the original col values are not being used for matvec. the ordering starts from 0, and goes up by 1.
            col_remote.push_back(col_remote_size-1);
//            nnz_col_remote[col_remote_size-1]++;
        }
    } // for i

    int* vIndexCount = (int*)malloc(sizeof(int)*nprocs);
    MPI_Alltoall(recvCount, 1, MPI_INT, vIndexCount, 1, MPI_INT, comm);

    numRecvProc = 0;
    numSendProc = 0;
    for(int i=0; i<nprocs; i++){
        if(recvCount[i]!=0){
            numRecvProc++;
            recvProcRank.push_back(i);
            recvProcCount.push_back(2*recvCount[i]); // make them double size for prolongation the communication in the aggregation function.
        }
        if(vIndexCount[i]!=0){
            numSendProc++;
            sendProcRank.push_back(i);
            sendProcCount.push_back(2*vIndexCount[i]); // make them double size for prolongation the communication in the aggregation function.
        }
    }

//    if (rank==0) cout << "rank=" << rank << ", numRecvProc=" << numRecvProc << ", numSendProc=" << numSendProc << endl;

    vdispls.resize(nprocs);
    rdispls.resize(nprocs);
    vdispls[0] = 0;
    rdispls[0] = 0;

    for (int i=1; i<nprocs; i++){
        vdispls[i] = vdispls[i-1] + vIndexCount[i-1];
        rdispls[i] = rdispls[i-1] + recvCount[i-1];
    }
    vIndexSize = vdispls[nprocs-1] + vIndexCount[nprocs-1];
    recvSize = rdispls[nprocs-1] + recvCount[nprocs-1];

    vIndex.resize(vIndexSize);
    MPI_Alltoallv(&*vElement_remote.begin(), recvCount, &*rdispls.begin(), MPI_UNSIGNED,
                  &vIndex[0], vIndexCount, &*vdispls.begin(), MPI_UNSIGNED, comm);

    free(vIndexCount);
    free(recvCount);

    // make them double size for prolongation the communication in the aggregation function.
    for (int i=1; i<nprocs; i++){
        vdispls[i] = 2*vdispls[i];
        rdispls[i] = 2*rdispls[i];
    }

/*    if (rank==0){
        cout << "vIndex: rank=" << rank  << endl;
        for(int i=0; i<vIndexSize; i++)
            cout << vIndex[i] << endl;
    }*/

    // change the indices from global to local
#pragma omp parallel for
    for (unsigned int i=0; i<vIndexSize; i++)
        vIndex[i] -= split[rank];

    // change the indices from global to local
#pragma omp parallel for
    for (unsigned int i=0; i<row_local.size(); i++)
        row_local[i] -= split[rank];

#pragma omp parallel for
    for (unsigned int i=0; i<row_remote.size(); i++)
        row_remote[i] -= split[rank];

    // vSend = vector values to send to other procs
    // vecValues = vector values that received from other procs
    // These will be used in matvec and they are set here to reduce the time of matvec.
    vSend.resize(2*vIndexSize); // make them double size for prolongation the communication in the aggregation function.
    vecValues.resize(2*recvSize); // make them double size for prolongation the communication in the aggregation function.

    indicesP_local.resize(nnz_l_local);
    for(i=0; i<nnz_l_local; i++)
        indicesP_local[i] = i;

    index_t *row_localP = &*row_local.begin();
    std::sort(&indicesP_local[0], &indicesP_local[nnz_l_local], sort_indices(row_localP));

//    indicesP_remote = (unsigned long*)malloc(sizeof(unsigned long)*nnz_l_remote);
//    for(i=0; i<nnz_l_remote; i++)
//        indicesP_remote[i] = i;
//    unsigned long* row_remoteP = &(*(row_remote.begin()));
//    std::sort(indicesP_remote, &indicesP_remote[nnz_l_remote], sort_indices(row_remoteP));

    return 0;
}


strength_matrix::~strength_matrix(){
//    free(vIndex);
//    free(vSend);
//    free(vecValues);
//    free(indicesP_local);
//    free(indicesP_remote);
//    rowIndex.resize(0);
//    col.resize(0);
//    values.resize(0);
}


int strength_matrix::erase(){
    M    = 0;
    Mbig = 0;
    nnz_l  = 0;
    nnz_l_local  = 0;
    nnz_l_remote = 0;
    col_remote_size = 0;
    vIndexSize = 0;
    recvSize = 0;
    numRecvProc = 0;
    numSendProc = 0;

    vIndex.clear();
    vSend.clear();
    vecValues.clear();
    split.clear();
    values_local.clear();
    values_remote.clear();
    row_local.clear();
    row_remote.clear();
    col_local.clear();
    col_remote.clear();
    col_remote2.clear();
    nnzPerRow.clear();
    nnzPerRow_local.clear();
    nnz_col_remote.clear();
    vElement_remote.clear();
    vElementRep_local.clear();
    vElementRep_remote.clear();
    indicesP_local.clear();
    indicesP_remote.clear();
    vdispls.clear();
    rdispls.clear();
    recvProcRank.clear();
    recvProcCount.clear();
    sendProcRank.clear();
    sendProcCount.clear();
    return 0;
}


void strength_matrix::print(int r){
//    int rank;
//    MPI_Comm_rank(comm, &rank);
//
//    if(rank==r)
//        for(unsigned int i=0; i<nnz_l; i++){
//            cout << "S:  " << "[" << Si2[i] << "," << Sj2[i] << "] = " << Sval2[i] << endl;
//            cout << "S:  " << "[" << (Si2[i] - A->split[rank]) << "," << Sj2[i] << "] = \t" << Sval2[i] << endl;
//        }
//
//    if (rank==r)
//        for(unsigned int i=0; i<M; i++){
//            for(long j=rowIndex[i]; j<rowIndex[i+1]; j++)
//                cout << "[" << i+1 << ",\t" << col[j]+1 << "] = \t" << values[j] << endl;
//        }
}
