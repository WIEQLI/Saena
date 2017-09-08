#ifndef SAENA_PROLONGMATRIX_H
#define SAENA_PROLONGMATRIX_H

#include <vector>
#include <mpich/mpi.h>
#include "auxFunctions.h"

class prolongMatrix {
// A matrix of this class is ordered <<ONLY>> if it is defined in createProlongation function in AMGClass.cpp.
// Otherwise it can be ordered using the following line:
//#include <algorithm>
//std::sort(P->entry.begin(), P->entry.end());
// It is ordered first column-wise, then row-wise, using std:sort with cooEntry class "< operator".
// duplicates are removed in createProlongation function in AMGClass.cpp.
private:

public:
    unsigned int Mbig;
    unsigned int Nbig;
    unsigned int M;
    unsigned long nnz_g;
    unsigned long nnz_l;

    unsigned int nnz_l_local;
    unsigned int nnz_l_remote;
    unsigned long col_remote_size; // this is the same as vElement_remote.size()

    std::vector<unsigned long> split;
    std::vector<unsigned long> splitNew;

//    std::vector<unsigned long> row;
//    std::vector<unsigned long> col;
//    std::vector<double> values;

    std::vector<cooEntry> entry;
    std::vector<cooEntry> entry_local;
    std::vector<cooEntry> entry_remote;
    std::vector<unsigned long> row_local;
    std::vector<unsigned long> row_remote;
    std::vector<unsigned long> col_remote; // index starting from 0, instead of the original column index
//    std::vector<unsigned long> col_remote2; //original col index
//    std::vector<double> values_local;
//    std::vector<double> values_remote;
//    std::vector<unsigned long> col_local;

    std::vector<unsigned int> nnzPerRow_local;
    std::vector<unsigned int> nnzPerRowScan_local;
    std::vector<unsigned int> nnzPerCol_remote; //todo: number of columns is large!
    std::vector<unsigned long> vElement_remote;
    std::vector<unsigned long> vElement_remote_t;
    std::vector<unsigned long> vElementRep_local;
    std::vector<unsigned long> vElementRep_remote;
//    std::vector<unsigned int> nnz_row_remote;

    bool arrays_defined = false; // set to true if findLocalRemote function is called. it will be used for destructor.
    int vIndexSize;
    int vIndexSize_t;
    unsigned long* vIndex;
    double* vSend;
    cooEntry* vSend_t;
    double* vecValues;
    cooEntry* vecValues_t;
//    int* vecValues2;
//    unsigned long* recvIndex_t;

    std::vector<int> vdispls;
    std::vector<int> vdispls_t;
    std::vector<int> rdispls;
    std::vector<int> rdispls_t;
    std::vector<int> recvProcRank;
    std::vector<int> recvProcRank_t;
    std::vector<int> recvProcCount;
    std::vector<int> recvProcCount_t;
    std::vector<int> sendProcRank;
    std::vector<int> sendProcRank_t;
    std::vector<int> sendProcCount;
    std::vector<int> sendProcCount_t;
    int recvSize;
    int recvSize_t;
    int numRecvProc;
    int numRecvProc_t;
    int numSendProc;
    int numSendProc_t;

    unsigned long* indicesP_local;
    unsigned long* indicesP_remote;

    MPI_Comm comm;

    prolongMatrix();
    prolongMatrix(MPI_Comm com);
    ~prolongMatrix();
    int findLocalRemote(cooEntry* entry);
    int matvec(double* v, double* w);
};


#endif //SAENA_PROLONGMATRIX_H
