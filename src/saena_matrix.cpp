#include "saena_matrix.h"
#include "parUtils.h"
#include "dollar.hpp"

#include <fstream>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>
#include <omp.h>
#include <printf.h>
#include "mpi.h"


saena_matrix::saena_matrix(){}


saena_matrix::saena_matrix(MPI_Comm com) {
    comm = com;
    comm_old = com;
}


int saena_matrix::read_file(const char* Aname){
    read_file(Aname, nullptr);
}


int saena_matrix::read_file(const char* Aname, const std::string &input_type) {
    // the following variables of saena_matrix class will be set in this function:
    // Mbig", "nnz_g", "initial_nnz_l", "data"
    // "data" is only required for repartition function.

    int rank, nprocs;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    read_from_file = true;

    std::string filename = Aname;
    size_t extIndex = filename.find_last_of(".");
    std::string file_extension = filename.substr(extIndex+1, 3);

//    if(rank==0) std::cout << "file_extension: " << file_extension << std::endl;

    std::string bin_filename = filename.substr(0, extIndex) + ".bin";
//    if(rank==0) std::cout << "bin_filename: " << bin_filename << std::endl;

    if(file_extension == "mtx"){

        std::ifstream inFile_check_bin(bin_filename.c_str());

        if (inFile_check_bin.is_open()){

            if(rank==0) std::cout << "\nA binary file with the same name exists. Using that file instead of the mtx"
                                     " file.\n\n";
            inFile_check_bin.close();

        } else {

            // write the file in binary by proc 0.
            if(rank==0){

                std::cout << "\nFirst a binary file with name \"" << bin_filename
                          << "\" will be created in the same directory. \n\n";

                std::string outFileName = filename.substr(0, extIndex) + ".bin";

                std::ifstream inFile(filename.c_str());

                if (!inFile.is_open()){
                    std::cout << "Could not open the file!" << std::endl;
//            return -1;
                }

                // ignore comments
                while (inFile.peek() == '%') inFile.ignore(2048, '\n');

                // M and N are the size of the matrix with nnz nonzeros
                unsigned int M, N, nnz;
                inFile >> M >> N >> nnz;

//                printf("M = %u, N = %u, nnz = %u \n", M, N, nnz);

                std::ofstream outFile;
                outFile.open(outFileName.c_str(), std::ios::out | std::ios::binary);

                std::vector<cooEntry> entry_temp1;
//                std::vector<cooEntry> entry;
                // number of nonzeros is less than 2*nnz, considering the diagonal
                // that's why there is a resize for entry when nnz is found.

                unsigned int a, b, i = 0;
                double c;

                if(input_type.c_str() == nullptr){

                    while(inFile >> a >> b >> c){
                        entry_temp1.resize(nnz);
                        // for mtx format, rows and columns start from 1, instead of 0.
//                        std::cout << "a = " << a << ", b = " << b << ", value = " << c << std::endl;
                        entry_temp1[i] = cooEntry(a-1, b-1, c);
                        i++;
//                        cout << entry_temp1[i] << endl;

                    }

                } else if (input_type == "triangle"){

                    while(inFile >> a >> b >> c){
                        entry_temp1.resize(2*nnz);
                        // for mtx format, rows and columns start from 1, instead of 0.
//                        std::cout << "a = " << a << ", b = " << b << ", value = " << c << std::endl;
                        entry_temp1[i] = cooEntry(a-1, b-1, c);
                        i++;
//                        cout << entry_temp1[i] << endl;
                        // add the lower triangle, not any diagonal entry
                        if(a != b){
                            entry_temp1[i] = cooEntry(b-1, a-1, c);
                            i++;
                            nnz++;
                        }
                    }
                    entry_temp1.resize(nnz);

                } else if (input_type == "pattern"){ // add 1 for value for a pattern matrix

                    while(inFile >> a >> b){
                        entry_temp1.resize(nnz);
                        // for mtx format, rows and columns start from 1, instead of 0.
//                        std::cout << "a = " << a << ", b = " << b << std::endl;
                        entry_temp1[i] = cooEntry(a-1, b-1, double(1));
                        i++;
//                        cout << entry_temp1[i] << endl;

                    }

                } else if(input_type == "tripattern") {

                    while(inFile >> a >> b){
                        entry_temp1.resize(2*nnz);
                        // for mtx format, rows and columns start from 1, instead of 0.
//                        std::cout << "a = " << a << ", b = " << b << std::endl;
                        entry_temp1[i] = cooEntry(a-1, b-1, double(1));
                        i++;
//                        std::cout << entry_temp1[i] << std::endl;

                        // add the lower triangle, not any diagonal entry
                        if(a != b){
                            entry_temp1[i] = cooEntry(b-1, a-1, double(1));
                            i++;
                            nnz++;
                        }
                    }
                    entry_temp1.resize(nnz);

                } else {
                    std::cerr << "the input type is not acceptable!" << std::endl;
                    MPI_Finalize();
                    return -1;
                }

                std::sort(entry_temp1.begin(), entry_temp1.end());

                for(i = 0; i < nnz; i++){
//                    std::cout << entry_temp1[i] << std::endl;
                    outFile.write((char*)&entry_temp1[i].row, sizeof(index_t));
                    outFile.write((char*)&entry_temp1[i].col, sizeof(index_t));
                    outFile.write((char*)&entry_temp1[i].val, sizeof(value_t));
                }

                inFile.close();
                outFile.close();
            }

        }

        // wait until the binary file writing by proc 0 is done.
        MPI_Barrier(comm);

    } else {
        if(file_extension != "bin" && rank==0) printf("The extension of file should be either mtx or bin! \n");
    }

    // find number of general nonzeros of the input matrix
    struct stat st;
    if(stat(bin_filename.c_str(), &st)){
        if(rank==0) printf("\nError: File does not exist!\n");
//        abort();
    }

    nnz_g = st.st_size / (2*sizeof(index_t) + sizeof(value_t));

    // find initial local nonzero
    initial_nnz_l = nnz_t(floor(1.0 * nnz_g / nprocs)); // initial local nnz
    if (rank == nprocs - 1) {
        initial_nnz_l = nnz_g - (nprocs - 1) * initial_nnz_l;
    }

    if(verbose_saena_matrix){
        MPI_Barrier(comm);
        printf("saena_matrix: part 1. rank = %d, nnz_g = %lu, initial_nnz_l = %lu \n", rank, nnz_g, initial_nnz_l);
        MPI_Barrier(comm);}

//    printf("\nrank = %d, nnz_g = %lu, initial_nnz_l = %lu \n", rank, nnz_g, initial_nnz_l);

    data_unsorted.resize(initial_nnz_l);
    cooEntry_row* datap = &data_unsorted[0];

    // *************************** read the matrix ****************************

    MPI_Status status;
    MPI_File fh;
    MPI_Offset offset;

    int mpiopen = MPI_File_open(comm, bin_filename.c_str(), MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);
    if (mpiopen) {
        if (rank == 0) std::cout << "Unable to open the matrix file!" << std::endl;
        MPI_Finalize();
    }

    //offset = rank * initial_nnz_l * 24; // row index(long=8) + column index(long=8) + value(double=8) = 24
    // the offset for the last process will be wrong if you use the above formula,
    // because initial_nnz_l of the last process will be used, instead of the initial_nnz_l of the other processes.

    offset = rank * nnz_t(floor(1.0 * nnz_g / nprocs)) * (2*sizeof(index_t) + sizeof(value_t));

    MPI_File_read_at(fh, offset, datap, initial_nnz_l, cooEntry_row::mpi_datatype(), &status);

//    int count;
//    MPI_Get_count(&status, MPI_UNSIGNED_LONG, &count);
    //printf("process %d read %d lines of triples\n", rank, count);
    MPI_File_close(&fh);

//    print_vector(data_unsorted, -1, "data_unsorted", comm);
//    printf("rank = %d \t\t\t before sort: data_unsorted size = %lu\n", rank, data_unsorted.size());

    remove_duplicates();

    // after removing duplicates, initial_nnz_l and nnz_g will be smaller, so update them.
    initial_nnz_l = data.size();
    MPI_Allreduce(&initial_nnz_l, &nnz_g, 1, MPI_UNSIGNED_LONG, MPI_SUM, comm);

    // *************************** find Mbig (global number of rows) ****************************
    // Since data[] has row-major order, the last element on the last process is the number of rows.
    // Broadcast it from the last process to the other processes.

    cooEntry last_element = data.back();
    Mbig = last_element.row + 1; // since indices start from 0, not 1.
    MPI_Bcast(&Mbig, 1, MPI_UNSIGNED, nprocs-1, comm);

    if(verbose_saena_matrix){
        MPI_Barrier(comm);
        printf("saena_matrix: part 2. rank = %d, nnz_g = %lu, initial_nnz_l = %lu, Mbig = %u \n", rank, nnz_g, initial_nnz_l, Mbig);
        MPI_Barrier(comm);}

//    print_vector(data, -1, "data", comm);

}


saena_matrix::~saena_matrix() {}


int saena_matrix::set(index_t row, index_t col, value_t val){

    cooEntry_row temp_new = cooEntry_row(row, col, val);
    std::pair<std::set<cooEntry_row>::iterator, bool> p = data_coo.insert(temp_new);

    if (!p.second){
        auto hint = p.first; // hint is std::set<cooEntry>::iterator
        hint++;
        data_coo.erase(p.first);
        // in the case of duplicate, if the new value is zero, remove the older one and don't insert the zero.
        if(!almost_zero(val))
            data_coo.insert(hint, temp_new);
    }

    // if the entry is zero and it was not a duplicate, just erase it.
    if(p.second && almost_zero(val))
        data_coo.erase(p.first);

    return 0;
}


int saena_matrix::set(index_t* row, index_t* col, value_t* val, nnz_t nnz_local){

    if(nnz_local <= 0){
        printf("size in the set function is either zero or negative!");
        return 0;
    }

    cooEntry_row temp_new;
    std::pair<std::set<cooEntry_row>::iterator, bool> p;

    // todo: isn't it faster to allocate memory for nnz_local, then assign, instead of inserting one by one.
    for(unsigned int i=0; i<nnz_local; i++){

        temp_new = cooEntry_row(row[i], col[i], val[i]);
        p = data_coo.insert(temp_new);

        if (!p.second){
            auto hint = p.first; // hint is std::set<cooEntry>::iterator
            hint++;
            data_coo.erase(p.first);
            // if the entry is zero and it was not a duplicate, just erase it.
            if(!almost_zero(val[i]))
                data_coo.insert(hint, temp_new);
        }

        // if the entry is zero, erase it.
        if(p.second && almost_zero(val[i]))
            data_coo.erase(p.first);
    }

    return 0;
}


int saena_matrix::set2(index_t row, index_t col, value_t val){

    // todo: if there are duplicates with different values on two different processors, what should happen?
    // todo: which one should be removed? Hari said "do it randomly".

    cooEntry_row temp_old;
    cooEntry_row temp_new = cooEntry_row(row, col, val);

    std::pair<std::set<cooEntry_row>::iterator, bool> p = data_coo.insert(temp_new);

    if (!p.second){
        temp_old = *(p.first);
        temp_new.val += temp_old.val;

        std::set<cooEntry_row>::iterator hint = p.first;
        hint++;
        data_coo.erase(p.first);
        data_coo.insert(hint, temp_new);
    }

    return 0;
}


int saena_matrix::set2(index_t* row, index_t* col, value_t* val, nnz_t nnz_local){

    if(nnz_local <= 0){
        printf("size in the set function is either zero or negative!");
        return 0;
    }

    cooEntry_row temp_old, temp_new;
    std::pair<std::set<cooEntry_row>::iterator, bool> p;

    for(unsigned int i=0; i<nnz_local; i++){
        if(!almost_zero(val[i])){
            temp_new = cooEntry_row(row[i], col[i], val[i]);
            p = data_coo.insert(temp_new);

            if (!p.second){
                temp_old = *(p.first);
                temp_new.val += temp_old.val;

                std::set<cooEntry_row>::iterator hint = p.first;
                hint++;
                data_coo.erase(p.first);
                data_coo.insert(hint, temp_new);
            }
        }
    }

    return 0;
}


// int saena_matrix::set3(unsigned int row, unsigned int col, double val)
/*
int saena_matrix::set3(unsigned int row, unsigned int col, double val){

    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    // update the matrix size if required.
//    if(row >= Mbig)
//        Mbig = row + 1; // "+ 1" is there since row starts from 0, not 1.
//    if(col >= Mbig)
//        Mbig = col + 1;

//    auto proc_num = lower_bound2(&*split.begin(), &*split.end(), (unsigned long)row);
//    printf("proc_num = %ld\n", proc_num);

    cooEntry recv_buf;
    cooEntry send_buf(row, col, val);

//    if(rank == proc_num)
//        MPI_Recv(&send_buf, 1, cooEntry::mpi_datatype(), , 0, comm, NULL);
//    if(rank != )
//        MPI_Send(&recv_buf, 1, cooEntry::mpi_datatype(), proc_num, 0, comm);

    //todo: change send_buf to recv_buf after completing the communication for the parallel version.
    auto position = lower_bound2(&*entry.begin(), &*entry.end(), send_buf);
//    printf("position = %lu \n", position);
//    printf("%lu \t%lu \t%f \n", entry[position].row, entry[position].col, entry[position].val);

    if(send_buf == entry[position]){
        if(add_duplicates){
            entry[position].val += send_buf.val;
        }else{
            entry[position].val = send_buf.val;
        }
    }else{
        printf("\nAttention: the structure of the matrix is being changed, so matrix.assemble() is required to call after being done calling matrix.set()!\n\n");
        entry.push_back(send_buf);
        std::sort(&*entry.begin(), &*entry.end());
        nnz_g++;
        nnz_l++;
    }

//    printf("\nentry:\n");
//    for(long i = 0; i < nnz_l; i++)
//        std::cout << entry[i] << std::endl;

    return 0;
}

int saena_matrix::set3(unsigned int* row, unsigned int* col, double* val, unsigned int nnz_local){

    if(nnz_local <= 0){
        printf("size in the set function is either zero or negative!");
        return 0;
    }

    cooEntry temp;
    long position;
    for(unsigned int i = 0; i < nnz_local; i++){
        temp = cooEntry(row[i], col[i], val[i]);
        position = lower_bound2(&*entry.begin(), &*entry.end(), temp);
        if(temp == entry[position]){
            if(add_duplicates){
                entry[position].val += temp.val;
            }else{
                entry[position].val  = temp.val;
            }
        }else{
            printf("\nAttention: the structure of the matrix is being changed, so matrix.assemble() is required to call after being done calling matrix.set()!\n\n");
            entry.push_back(temp);
            std::sort(&*entry.begin(), &*entry.end());
            nnz_g++;
            nnz_l++;
        }
    }

//    printf("\nentry:\n");
//    for(long i = 0; i < nnz_l; i++)
//        std::cout << entry[i] << std::endl;

    return 0;
}
*/


void saena_matrix::set_comm(MPI_Comm com){
    comm = com;
    comm_old = com;
}


int saena_matrix::destroy(){
    return 0;
}


int saena_matrix::erase(){
//    data.clear();
//    data.shrink_to_fit();

    entry.clear();
    split.clear();
    split_old.clear();
    values_local.clear();
    row_local.clear();
    values_remote.clear();
    row_remote.clear();
    col_local.clear();
    col_remote.clear();
    col_remote2.clear();
    nnzPerRow_local.clear();
    nnzPerCol_remote.clear();
    inv_diag.clear();
    vdispls.clear();
    rdispls.clear();
    recvProcRank.clear();
    recvProcCount.clear();
    sendProcRank.clear();
    sendProcCount.clear();
    sendProcCount.clear();
//    vElementRep_local.clear();
    vElementRep_remote.clear();

    entry.shrink_to_fit();
    split.shrink_to_fit();
    split_old.shrink_to_fit();
    values_local.shrink_to_fit();
    values_remote.shrink_to_fit();
    row_local.shrink_to_fit();
    row_remote.shrink_to_fit();
    col_local.shrink_to_fit();
    col_remote.shrink_to_fit();
    col_remote2.shrink_to_fit();
    nnzPerRow_local.shrink_to_fit();
    nnzPerCol_remote.shrink_to_fit();
    inv_diag.shrink_to_fit();
    vdispls.shrink_to_fit();
    rdispls.shrink_to_fit();
    recvProcRank.shrink_to_fit();
    recvProcCount.shrink_to_fit();
    sendProcRank.shrink_to_fit();
    sendProcCount.shrink_to_fit();
    sendProcCount.shrink_to_fit();
//    vElementRep_local.shrink_to_fit();
    vElementRep_remote.shrink_to_fit();

    if(free_zfp_buff){
        free(zfp_send_buffer);
        free(zfp_recv_buffer);
    }

    M = 0;
    Mbig = 0;
    nnz_g = 0;
    nnz_l = 0;
    nnz_l_local = 0;
    nnz_l_remote = 0;
    col_remote_size = 0;
    recvSize = 0;
    numRecvProc = 0;
    numSendProc = 0;
    assembled = false;

    return 0;
}


int saena_matrix::erase2(){
//    data.clear();
//    data.shrink_to_fit();

    entry.clear();
    split.clear();
    split_old.clear();
    values_local.clear();
    row_local.clear();
    values_remote.clear();
    row_remote.clear();
    col_local.clear();
    col_remote.clear();
    col_remote2.clear();
    nnzPerRow_local.clear();
    nnzPerCol_remote.clear();
    inv_diag.clear();
    vdispls.clear();
    rdispls.clear();
    recvProcRank.clear();
    recvProcCount.clear();
    sendProcRank.clear();
    sendProcCount.clear();
//    vElementRep_local.clear();
    vElementRep_remote.clear();
    vIndex.clear();
    vSend.clear();
    vecValues.clear();
    vSendULong.clear();
    vecValuesULong.clear();
    indicesP_local.clear();
    indicesP_remote.clear();
    recvCount.clear();
    recvCountScan.clear();
    sendCount.clear();
    sendCountScan.clear();
    iter_local_array.clear();
    iter_remote_array.clear();
    iter_local_array2.clear();
    iter_remote_array2.clear();
    vElement_remote.clear();
    w_buff.clear();

    entry.shrink_to_fit();
    split.shrink_to_fit();
    split_old.shrink_to_fit();
    values_local.shrink_to_fit();
    values_remote.shrink_to_fit();
    row_local.shrink_to_fit();
    row_remote.shrink_to_fit();
    col_local.shrink_to_fit();
    col_remote.shrink_to_fit();
    col_remote2.shrink_to_fit();
    nnzPerRow_local.shrink_to_fit();
    nnzPerCol_remote.shrink_to_fit();
    inv_diag.shrink_to_fit();
    vdispls.shrink_to_fit();
    rdispls.shrink_to_fit();
    recvProcRank.shrink_to_fit();
    recvProcCount.shrink_to_fit();
    sendProcRank.shrink_to_fit();
    sendProcCount.shrink_to_fit();
//    vElementRep_local.shrink_to_fit();
    vElementRep_remote.shrink_to_fit();
    vIndex.shrink_to_fit();
    vSend.shrink_to_fit();
    vecValues.shrink_to_fit();
    vSendULong.shrink_to_fit();
    vecValuesULong.shrink_to_fit();
    indicesP_local.shrink_to_fit();
    indicesP_remote.shrink_to_fit();
    recvCount.shrink_to_fit();
    recvCountScan.shrink_to_fit();
    sendCount.shrink_to_fit();
    sendCountScan.shrink_to_fit();
    iter_local_array.shrink_to_fit();
    iter_remote_array.shrink_to_fit();
    iter_local_array2.shrink_to_fit();
    iter_remote_array2.shrink_to_fit();
    vElement_remote.shrink_to_fit();
    w_buff.shrink_to_fit();

    if(free_zfp_buff){
        free(zfp_send_buffer);
        free(zfp_recv_buffer);
    }

//    M = 0;
//    Mbig = 0;
//    nnz_g = 0;
//    nnz_l = 0;
//    nnz_l_local = 0;
//    nnz_l_remote = 0;
//    col_remote_size = 0;
//    recvSize = 0;
//    numRecvProc = 0;
//    numSendProc = 0;
//    vIndexSize = 0;
//    shrinked = false;
//    active = true;
    assembled = false;
    freeBoolean = false;

    return 0;
}


int saena_matrix::erase_update_local(){

//    row_local_temp.clear();
//    col_local_temp.clear();
//    values_local_temp.clear();
//    row_local.swap(row_local_temp);
//    col_local.swap(col_local_temp);
//    values_local.swap(values_local_temp);

//    entry.clear();
    // push back the remote part
//    for(unsigned long i = 0; i < row_remote.size(); i++)
//        entry.emplace_back(cooEntry(row_remote[i], col_remote2[i], values_remote[i]));

//    split.clear();
//    split_old.clear();
    values_local.clear();
    row_local.clear();
    col_local.clear();
    row_remote.clear();
    col_remote.clear();
    col_remote2.clear();
    values_remote.clear();
    nnzPerRow_local.clear();
    nnzPerCol_remote.clear();
    inv_diag.clear();
    vdispls.clear();
    rdispls.clear();
    recvProcRank.clear();
    recvProcCount.clear();
    sendProcRank.clear();
    sendProcCount.clear();
    sendProcCount.clear();
//    vElementRep_local.clear();
    vElementRep_remote.clear();

//    M = 0;
//    Mbig = 0;
//    nnz_g = 0;
//    nnz_l = 0;
//    nnz_l_local = 0;
    nnz_l_remote = 0;
    col_remote_size = 0;
    recvSize = 0;
    numRecvProc = 0;
    numSendProc = 0;
    assembled = false;

    return 0;
}


int saena_matrix::erase_keep_remote2(){

    entry.clear();

    // push back the remote part
    for(unsigned long i = 0; i < row_remote.size(); i++)
        entry.emplace_back(cooEntry(row_remote[i], col_remote2[i], values_remote[i]));

    split.clear();
    split_old.clear();
    values_local.clear();
    row_local.clear();
    values_remote.clear();
    row_remote.clear();
    col_local.clear();
    col_remote.clear();
    col_remote2.clear();
    nnzPerRow_local.clear();
    nnzPerCol_remote.clear();
    inv_diag.clear();
    vdispls.clear();
    rdispls.clear();
    recvProcRank.clear();
    recvProcCount.clear();
    sendProcRank.clear();
    sendProcCount.clear();
//    vElementRep_local.clear();
    vElementRep_remote.clear();
    vIndex.clear();
    vSend.clear();
    vecValues.clear();
    vSendULong.clear();
    vecValuesULong.clear();
    indicesP_local.clear();
    indicesP_remote.clear();
    recvCount.clear();
    recvCountScan.clear();
    sendCount.clear();
    sendCountScan.clear();
    iter_local_array.clear();
    iter_remote_array.clear();
    iter_local_array2.clear();
    iter_remote_array2.clear();
    vElement_remote.clear();
    w_buff.clear();

    // erase_keep_remote() is used in coarsen2(), so keep the memory reserved for performance.
    // so don't use shrink_to_fit() on these vectors.

    M = 0;
    Mbig = 0;
    nnz_g = 0;
    nnz_l = 0;
    nnz_l_local = 0;
    nnz_l_remote = 0;
    col_remote_size = 0;
    recvSize = 0;
    numRecvProc = 0;
    numSendProc = 0;
    vIndexSize = 0;
//    assembled = false;
//    shrinked = false;
//    active = true;
    freeBoolean = false;

    return 0;
}


int saena_matrix::erase_after_shrink() {

    row_local.clear();
    col_local.clear();
    values_local.clear();

    row_remote.clear();
    col_remote.clear();
    col_remote2.clear();
    values_remote.clear();

//    vElementRep_local.clear();
    vElementRep_remote.clear();
    vElement_remote.clear();

//    nnzPerRow_local.clear();
//    nnzPerCol_remote.clear();
//    inv_diag.clear();
//    vdispls.clear();
//    rdispls.clear();
//    recvProcRank.clear();
//    recvProcCount.clear();
//    sendProcRank.clear();
//    sendProcCount.clear();
//    vIndex.clear();
//    vSend.clear();
//    vecValues.clear();
//    vSendULong.clear();
//    vecValuesULong.clear();
//    indicesP_local.clear();
//    indicesP_remote.clear();
//    recvCount.clear();
//    recvCountScan.clear();
//    sendCount.clear();
//    sendCountScan.clear();
//    iter_local_array.clear();
//    iter_remote_array.clear();
//    iter_local_array2.clear();
//    iter_remote_array2.clear();
//    w_buff.clear();

    return 0;
}


int saena_matrix::erase_after_decide_shrinking() {

//    row_local.clear();
    col_local.clear();
    values_local.clear();

    row_remote.clear();
    col_remote.clear();
    col_remote2.clear();
    values_remote.clear();

//    vElementRep_local.clear();
    vElementRep_remote.clear();
    vElement_remote.clear();

//    nnzPerRow_local.clear();
    nnzPerCol_remote.clear();
//    inv_diag.clear();
//    vdispls.clear();
//    rdispls.clear();
    recvProcRank.clear();
    recvProcCount.clear();
    sendProcRank.clear();
    sendProcCount.clear();
//    vIndex.clear();
//    vSend.clear();
//    vecValues.clear();
//    vSendULong.clear();
//    vecValuesULong.clear();
//    indicesP_local.clear();
//    indicesP_remote.clear();
//    recvCount.clear();
//    recvCountScan.clear();
//    sendCount.clear();
//    sendCountScan.clear();
//    iter_local_array.clear();
//    iter_remote_array.clear();
//    iter_local_array2.clear();
//    iter_remote_array2.clear();
//    w_buff.clear();

    return 0;
}


int saena_matrix::set_zero(){

#pragma omp parallel for
    for(nnz_t i = 0; i < nnz_l; i++)
        entry[i].val = 0;

    values_local.clear();
    values_remote.clear();

    return 0;
}


int saena_matrix::residual(std::vector<value_t>& u, std::vector<value_t>& rhs, std::vector<value_t>& res){
    // Vector res = A*u - rhs;

    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

//    printf("residual start!!!\n");

    // First check if u is zero or not. If it is zero, matvec is not required.
    bool zero_vector_local = true, zero_vector;
//#pragma omp parallel for
    for(index_t i = 0; i < M; i++){
        if(u[i] != 0){
            zero_vector_local = false;
            break;
        }
    }

    MPI_Allreduce(&zero_vector_local, &zero_vector, 1, MPI_CXX_BOOL, MPI_LOR, comm);

//    if(!zero_vector)
//        matvec(u, res);
//    #pragma omp parallel for
//    for(index_t i = 0; i < M; i++)
//        res[i] = -rhs[i];

    if(zero_vector){
        #pragma omp parallel for
        for(index_t i = 0; i < M; i++)
            res[i] = -rhs[i];
    } else{
        matvec(u, res);

        #pragma omp parallel for
        for(index_t i = 0; i < M; i++){
//            if(rank==0 && i==0) std::cout << i << "\t" << res[i] << "\t" << rhs[i] << "\t" << res[i]-rhs[i] << std::endl;
//            if(rank==0 && i==8) printf("i = %u \tu = %.16f \tAu = %.16f \tres = %.16f \n", i, u[i], res[i], res[i]-rhs[i]);
            res[i] -= rhs[i];
        }
    }

//    print_vector(res, -1, "res", comm);

    return 0;
}


int saena_matrix::jacobi(int iter, std::vector<value_t>& u, std::vector<value_t>& rhs, std::vector<value_t>& temp) {

// Ax = rhs
// u = u - (D^(-1))(Au - rhs)
// 1. B.matvec(u, one) --> put the value of matvec in one.
// 2. two = one - rhs
// 3. three = inverseDiag * two * omega
// 4. four = u - three

//    int rank;
//    MPI_Comm_rank(comm, &rank);

    printf("jacobi!!!\n");

    for(int j = 0; j < iter; j++){
        matvec(u, temp);

#pragma omp parallel for
        for(index_t i = 0; i < M; i++){
            temp[i] -= rhs[i];
            temp[i] *= inv_diag[i] * jacobi_omega;
            u[i]    -= temp[i];
        }
    }

    return 0;
}


int saena_matrix::chebyshev(int iter, std::vector<value_t>& u, std::vector<value_t>& rhs, std::vector<value_t>& res, std::vector<value_t>& d){

//    int rank;
//    MPI_Comm_rank(comm, &rank);

//    eig_max_of_invdiagXA *= 10;

    double alpha = 0.25 * eig_max_of_invdiagXA; //homg: 0.25 * eig_max
    double beta = eig_max_of_invdiagXA;
    double delta = (beta - alpha)/2;
    double theta = (beta + alpha)/2;
    double s1 = theta/delta;
    double rhok = 1/s1;
    double rhokp1, d1, d2;

    // first loop
    residual(u, rhs, res);
#pragma omp parallel for
    for(index_t i = 0; i < u.size(); i++){
        d[i] = (-res[i] * inv_diag[i]) / theta;
        u[i] += d[i];
//        if(rank==0) printf("inv_diag[%u] = %f, \tres[%u] = %f, \td[%u] = %f, \tu[%u] = %f \n",
//                           i, inv_diag[i], i, res[i], i, d[i], i, u[i]);
    }

    for(int i = 1; i < iter; i++){
        rhokp1 = 1 / (2*s1 - rhok);
        d1     = rhokp1 * rhok;
        d2     = 2*rhokp1 / delta;
        rhok   = rhokp1;
        residual(u, rhs, res);

#pragma omp parallel for
        for(index_t j = 0; j < u.size(); j++){
            d[j] = ( d1 * d[j] ) + ( d2 * (-res[j] * inv_diag[j]));
            u[j] += d[j];
//            if(rank==0) printf("inv_diag[%u] = %f, \tres[%u] = %f, \td[%u] = %f, \tu[%u] = %f \n",
//                               j, inv_diag[j], j, res[j], j, d[j], j, u[j]);
        }
    }

    return 0;
}


int saena_matrix::print_entry(int ran){

    // if ran >= 0 print_entry the matrix entries on proc with rank = ran
    // otherwise print_entry the matrix entries on all processors in order. (first on proc 0, then proc 1 and so on.)

    int rank, nprocs;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    index_t iter = 0;
    if(ran >= 0) {
        if (rank == ran) {
            printf("\nmatrix on proc = %d \n", ran);
            printf("nnz = %lu \n", nnz_l);
            for (auto i:entry) {
                std::cout << iter << "\t" << i << std::endl;
                iter++;
            }
        }
    } else{
        for(index_t proc = 0; proc < nprocs; proc++){
            MPI_Barrier(comm);
            if (rank == proc) {
                printf("\nmatrix on proc = %d \n", proc);
                printf("nnz = %lu \n", nnz_l);
                for (auto i:entry) {
                    std::cout << iter << "\t" << i << std::endl;
                    iter++;
                }
            }
            MPI_Barrier(comm);
        }
    }

    return 0;
}


int saena_matrix::print_info(int ran) {

    // if ran >= 0 print the matrix info on proc with rank = ran
    // otherwise print the matrix info on all processors in order. (first on proc 0, then proc 1 and so on.)

    int rank, nprocs;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    if(ran >= 0) {
        if (rank == ran) {
            printf("\nmatrix A info on proc = %d \n", ran);
            printf("Mbig = %u, \tM = %u, \tnnz_g = %lu, \tnnz_l = %lu \n", Mbig, M, nnz_g, nnz_l);
        }
    } else{
        MPI_Barrier(comm);
        if(rank==0) printf("\nmatrix A info:      Mbig = %u, \tnnz_g = %lu \n", Mbig, nnz_g);
        for(index_t proc = 0; proc < nprocs; proc++){
            MPI_Barrier(comm);
            if (rank == proc) {
                printf("matrix A on rank %d: M    = %u, \tnnz_l = %lu \n", proc, M, nnz_l);
            }
            MPI_Barrier(comm);
        }
    }

    return 0;
}


int saena_matrix::writeMatrixToFile(){
    // the matrix file will be written in the HOME directory.

    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    if(rank==0) printf("The matrix file will be written in the HOME directory. \n");
    writeMatrixToFile("");
}


int saena_matrix::writeMatrixToFile(const char *folder_name){
    // Create txt files with name Ac-r0.txt for processor 0, Ac-r1.txt for processor 1, etc.
    // Then, concatenate them in terminal: cat Ac-r0.txt Ac-r1.txt > Ac.txt
    // row and column indices of txt files should start from 1, not 0.
    // write the files inside ${HOME}/folder_name
    // this is the default case for the sorting which is column-major.

    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    const char* homeDir = getenv("HOME");

    std::ofstream outFileTxt;
    std::string outFileNameTxt = homeDir;
    outFileNameTxt += "/";
    outFileNameTxt += folder_name;
    outFileNameTxt += "/mat-r";
    outFileNameTxt += std::to_string(rank);
    outFileNameTxt += ".txt";
    outFileTxt.open(outFileNameTxt);

    if(rank==0) std::cout << "\nWriting the matrix in: " << outFileNameTxt << std::endl;

    std::vector<cooEntry> entry_temp1 = entry;
    std::vector<cooEntry> entry_temp2;
    par::sampleSort(entry_temp1, entry_temp2, comm);

    // sort row-wise
//    std::vector<cooEntry_row> entry_temp1(entry.size());
//    std::memcpy(&*entry_temp1.begin(), &*entry.begin(), entry.size() * sizeof(cooEntry));
//    std::vector<cooEntry_row> entry_temp2;
//    par::sampleSort(entry_temp1, entry_temp2, comm);

    // first line of the file: row_size col_size nnz
    if(rank==0)
        outFileTxt << Mbig << "\t" << Mbig << "\t" << nnz_g << std::endl;

    for (nnz_t i = 0; i < entry_temp2.size(); i++) {
//        if(A->entry[i].row == A->entry[i].col)
//            continue;

//        if(rank==0) std::cout  << A->entry[i].row + 1 << "\t" << A->entry[i].col + 1 << "\t" << A->entry[i].val << std::endl;
        outFileTxt << entry_temp2[i].row + 1 << "\t" << entry_temp2[i].col + 1 << "\t" << entry_temp2[i].val << std::endl;
    }

    outFileTxt.clear();
    outFileTxt.close();

    return 0;
}