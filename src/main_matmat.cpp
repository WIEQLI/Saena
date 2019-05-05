#include "grid.h"
#include "saena_object.h"
#include "saena_matrix.h"
#include "saena.hpp"

#include "petsc_functions.h"
//#include "combblas_functions.h"

#include <iostream>
#include <algorithm>
#include <fstream>
#include <sys/stat.h>
#include <vector>
#include <omp.h>
#include <dollar.hpp>
#include "mpi.h"


int main(int argc, char* argv[]){

    MPI_Init(&argc, &argv);
    MPI_Comm comm = MPI_COMM_WORLD;
    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    bool verbose = false;

    if(argc != 3){
        if(rank == 0){
//            std::cout << "This is how you can make a 3DLaplacian: ./Saena <x grid size> <y grid size> <z grid size>" << std::endl;
            std::cout << "This is how you can make a banded matrix: ./Saena <local size> <bandwidth>" << std::endl;
        }
        MPI_Finalize();
        return -1;
    }

    if(!rank) std::cout << "nprocs = " << nprocs << std::endl;

    // *************************** initialize the matrix: banded ****************************
    double t1 = MPI_Wtime();

    int M(std::stoi(argv[1]));
    int band(std::stoi(argv[2]));

    saena::matrix A(comm);
    saena::band_matrix(A, M, band);

    // ********** print matrix and time **********

    double t2 = MPI_Wtime();
    if(verbose) print_time(t1, t2, "Matrix Assemble:", comm);
//    print_time(t1, t2, "Matrix Assemble:", comm);

//    A.print(0);
//    A.get_internal_matrix()->print_info(0);
//    A.get_internal_matrix()->writeMatrixToFile("matrix_folder/matrix");

//    petsc_viewer(A.get_internal_matrix());

// *************************** checking the correctness of matrix-matrix product ****************************

    saena::amg solver;
    saena::matrix C(comm);
    solver.matmat(&A, &A, &C);

//    petsc_check_matmat(A.get_internal_matrix(), A.get_internal_matrix(), C.get_internal_matrix());

// *************************** matrix-matrix product ****************************

/*
    double matmat_time = 0;
    int matmat_iter_warmup = 5;
    int matmat_iter = 5;

    saena::amg solver;
//    saena::matrix C(comm);

    // warm-up
    for(int i = 0; i < matmat_iter_warmup; i++){
        solver.matmat_ave(&A, &A, matmat_time);
    }

    MPI_Barrier(comm);
    matmat_time = 0;
    for(int i = 0; i < matmat_iter; i++){
        solver.matmat_ave(&A, &A, matmat_time);
    }

    if(!rank) printf("Saena matmat:\n%f\n", matmat_time / matmat_iter);
*/

//    petsc_viewer(A.get_internal_matrix());
//    petsc_viewer(C.get_internal_matrix());
//    saena_object *obj1 = solver.get_object();

//    petsc_matmat_ave(A.get_internal_matrix(), A.get_internal_matrix(), matmat_iter);
//    petsc_matmat(A.get_internal_matrix(), A.get_internal_matrix());

    // *************************** CombBLAS ****************************

//    combblas_matmult_DoubleBuff();
//    int combblas_matmult_Synch();
//    int combblas_GalerkinNew();

    // *************************** finalize ****************************

//    if(rank==0) dollar::text(std::cout);

    MPI_Finalize();
    return 0;
}