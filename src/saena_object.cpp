#include "superlu_ddefs.h"
#include "saena_object.h"
#include "saena_matrix.h"
#include "strength_matrix.h"
//#include "prolong_matrix.h"
//#include "restrict_matrix.h"
#include "grid.h"
#include "aux_functions.h"
#include "ietl_saena.h"
//#include "dollar.hpp"

//#include "petsc_functions.h"

#include <cstdio>
#include <random>
#include <mpi.h>


void saena_object::set_parameters(int max_iter, double tol, std::string sm, int preSm, int postSm){
//    maxLevel = l-1; // maxLevel does not include fine level. fine level is 0.
    solver_max_iter = max_iter;
    solver_tol      = tol;
    smoother        = sm;
    preSmooth       = preSm;
    postSmooth      = postSm;
}


MPI_Comm saena_object::get_orig_comm(){
    return grids[0].A->comm;
}


int saena_object::setup(saena_matrix* A, const std::vector<std::vector<int>> &m_l2g /*= {}*/, const std::vector<int> &m_g2u /*= {}*/, int m_bdydof /*= 0*/) {
    int nprocs, rank, rank_new;
    MPI_Comm_size(A->comm, &nprocs);
    MPI_Comm_rank(A->comm, &rank);

    #pragma omp parallel default(none) shared(rank, nprocs)
    if(!rank && omp_get_thread_num()==0)
        printf("\nnumber of processes = %d\nnumber of threads   = %d\n\n", nprocs, omp_get_num_threads());

#ifdef __DEBUG1__
    if(verbose_setup){
        MPI_Barrier(A->comm);
        if(!rank){
            printf("_____________________________\n\n");
            printf("level = 0 \nnumber of procs = %d \nmatrix size \t= %d \nnonzero \t= %lu \ndensity \t= %.6f \n",
                   nprocs, A->Mbig, A->nnz_g, A->density);
        }
        MPI_Barrier(A->comm);
    }
    if(verbose_setup_steps){
        MPI_Barrier(A->comm);

        if(!rank){
#ifdef SPLIT_NNZ
            printf("\nsplit based on nnz\n");
#endif
#ifdef SPLIT_SIZE
            printf("\nsplit based on matrix size\n");
#endif
            std::cout << "coarsen_method: " << coarsen_method << std::endl;
            printf("\nsetup: start: find_eig()\n");
        }

        MPI_Barrier(A->comm);
    }
#endif

    if(smoother=="chebyshev"){
        find_eig(*A);
    }

    if(fabs(sample_sz_percent - 1) < 1e-4)
        doSparsify = false;

#ifdef __DEBUG1__
    if(verbose_setup_steps){
        MPI_Barrier(A->comm);
        if(!rank) printf("setup: generate_dense_matrix()\n");
        MPI_Barrier(A->comm);
    }
#endif

    A->switch_to_dense = switch_to_dense;
    A->dense_threshold = dense_threshold;
    if(switch_to_dense && A->density > dense_threshold) {
        A->generate_dense_matrix();
    }

#ifdef __DEBUG1__
    if(verbose_setup_steps){
        MPI_Barrier(A->comm);
        if(!rank) printf("setup: mesh info\n");
        MPI_Barrier(A->comm);
    }
#endif

    std::vector< std::vector< std::vector<int> > > map_all;
    if(!m_l2g.empty())
        map_all.emplace_back(m_l2g);

    std::vector< std::vector<int> > g2u_all;
    if(!g2u_all.empty())
        g2u_all.emplace_back(m_g2u);

#ifdef __DEBUG1__
    if(verbose_setup_steps){
        MPI_Barrier(A->comm);
        if(!rank) printf("setup: level 0\n");
        MPI_Barrier(A->comm);
    }
#endif

    grids.resize(max_level + 1);
    grids[0] = Grid(A,0);

    int res = 0;
    for(int i = 0; i < max_level; ++i){

#ifdef __DEBUG1__
        if(verbose_setup_steps){
            MPI_Barrier(A->comm);
            if(!rank) printf("\nsetup: level %d\n", i+1);
            MPI_Barrier(A->comm);
        }
#endif

        if (shrink_level_vector.size() > i + 1 && shrink_level_vector[i+1])
            grids[i].A->enable_shrink_next_level = true;
        if (shrink_values_vector.size() > i + 1)
            grids[i].A->cpu_shrink_thre2_next_level = shrink_values_vector[i+1];

        res = coarsen(&grids[i], map_all, g2u_all); // create P, R and Ac for grid[i]

        if(res != 0){
            if(res == 1){
                max_level = i + 1;
            }else if(res == 2){
                max_level = i;
                break;
            }else{
                printf("Invalid return value in saena_object::setup()");
                exit(EXIT_FAILURE);
            }
        }

        grids[i + 1] = Grid(&grids[i].Ac, i + 1);   // Pass A to grids[i+1] (created as Ac in grids[i])
        grids[i].coarseGrid = &grids[i + 1];        // connect grids[i+1] to grids[i]

        if(grids[i].Ac.active) {
#ifdef __DEBUG1__
            if(verbose_setup_steps){
                MPI_Barrier(A->comm);
                if(!rank) printf("setup: find_eig()\n");
                MPI_Barrier(A->comm);
            }
#endif

            if (smoother == "chebyshev") {
                find_eig(grids[i].Ac);
            }

            if (verbose_setup) {
                MPI_Comm_rank(grids[i].Ac.comm, &rank_new);

                if (!rank_new) {
//                    MPI_Comm_size(grids[i].Ac.comm, &nprocs);
                    printf("_____________________________\n\n");
                    printf("level = %d \nnumber of procs = %d \nmatrix size \t= %d \nnonzero \t= %lu"
                           "\ndensity \t= %.6f \ncoarsen method \t= %s\n",
                           grids[i + 1].currentLevel, grids[i + 1].A->total_active_procs, grids[i + 1].A->Mbig, grids[i + 1].A->nnz_g,
                           grids[i + 1].A->density, (grids[i].A->p_order == 1 ? "h-coarsen" : "p-coarsen"));
                }
            }
        }else{
            printf("!grids[i].Ac.active\n");
            break;
        }

    }

    // max_level is the lowest on the active processors in the last grid. So MPI_MIN is used in the following MPI_Allreduce.
    int max_level_send = max_level;
    MPI_Allreduce(&max_level_send, &max_level, 1, MPI_INT, MPI_MIN, grids[0].A->comm);
    grids.resize(max_level);

#ifdef __DEBUG1__
    if(verbose_setup_steps){
        MPI_Barrier(A->comm);
        if(!rank) printf("setup: finished creating the hierarchy. max_level = %u \n", max_level);
        MPI_Barrier(A->comm);
    }
#endif

    if(max_level == 0){
        A_coarsest = A;
    }else{
        A_coarsest = &grids.back().Ac;
    }

#ifdef __DEBUG1__
    {
//    MPI_Barrier(A->comm);
//    for(int l = 0; l < max_level; ++l){
//        printf("\nlevel = %d\n", l);
//        if(grids[l].Ac.active) {
//            printf("rank = %d active\n", rank);
//        } else {
//            printf("rank = %d not active\n", rank);
//        }
//        MPI_Barrier(A->comm);
//    }

/*
    // grids[i+1].row_reduction_min is 0 by default. for the active processors in the last grid, it will be non-zero.
    // that's why MPI_MAX is used in the following MPI_Allreduce.
    float row_reduction_min_send = grids[i].row_reduction_min;
    MPI_Allreduce(&row_reduction_min_send, &grids[i].row_reduction_min, 1, MPI_FLOAT, MPI_MAX, grids[0].A->comm);
    // delete the coarsest level, if the size is not reduced much.
    if (grids[i].row_reduction_min > row_reduction_up_thrshld) {
        grids.pop_back();
        max_level--;
    }
*/
    }
#endif

    if(verbose_setup){
        MPI_Barrier(A->comm);
        if(!rank){
            printf("_____________________________\n\n");
            printf("number of levels = << %d >> (the finest level is 0)\n", max_level);
            if(doSparsify) printf("final sample size percent = %f\n", 1.0 * sample_prcnt_numer / sample_prcnt_denom);
            printf("\n******************************************************\n");
        }
        MPI_Barrier(A->comm);
    }

#ifdef __DEBUG1__
    if(verbose_setup_steps){
        MPI_Barrier(A->comm);
        if(!rank) printf("rank %d: setup done!\n", rank);
        MPI_Barrier(A->comm);
    }
//    if(rank==0) dollar::text(std::cout);
#endif

    return 0;
}


int saena_object::coarsen(Grid *grid, std::vector< std::vector< std::vector<int> > > &map_all, std::vector< std::vector<int> > &g2u_all){

#ifdef __DEBUG1__
    int nprocs = -1, rank = -1;
    MPI_Comm_size(grid->A->comm, &nprocs);
    MPI_Comm_rank(grid->A->comm, &rank);

//    if(verbose_level_setup){
//        MPI_Barrier(grid->A->comm);
//        printf("rank = %d, start of coarsen: level = %d \n", rank, grid->currentLevel);
//        MPI_Barrier(grid->A->comm);
//    }

//    grid->A->print_info(-1);
    double t1 = 0, t2 = 0;
#endif

    // **************************** create_prolongation ****************************

#ifdef __DEBUG1__
    t1 = omp_get_wtime();
#endif

    int ret_val = create_prolongation(grid, map_all,g2u_all);

    if(ret_val == 2){
        return ret_val;
    }

#ifdef __DEBUG1__
    t2 = omp_get_wtime();
    if(verbose_coarsen) print_time(t1, t2, "Prolongation: level "+std::to_string(grid->currentLevel), grid->A->comm);

//    MPI_Barrier(grid->A->comm); printf("rank %d: here after create_prolongation!!! \n", rank); MPI_Barrier(grid->A->comm);
//    print_vector(grid->P.split, 0, "grid->P.split", grid->A->comm);
//    print_vector(grid->P.splitNew, 0, "grid->P.splitNew", grid->A->comm);
//    grid->P.print_info(-1);
//    grid->P.print_entry(-1);
#endif

    // **************************** restriction ****************************

#ifdef __DEBUG1__
    t1 = omp_get_wtime();
#endif

    grid->R.transposeP(&grid->P);

#ifdef __DEBUG1__
    t2 = omp_get_wtime();
    if(verbose_coarsen) print_time(t1, t2, "Restriction: level "+std::to_string(grid->currentLevel), grid->A->comm);

//    MPI_Barrier(grid->A->comm); printf("rank %d: here after transposeP!!! \n", rank); MPI_Barrier(grid->A->comm);
//    print_vector(grid->R.entry_local, -1, "grid->R.entry_local", grid->A->comm);
//    print_vector(grid->R.entry_remote, -1, "grid->R.entry_remote", grid->A->comm);
//    grid->R.print_info(-1);
//    grid->R.print_entry(-1);
#endif

    // **************************** compute_coarsen ****************************

#ifdef __DEBUG1__
//    MPI_Barrier(grid->A->comm);
//    double t11 = MPI_Wtime();
    t1 = omp_get_wtime();
#endif

    compute_coarsen(grid);

#ifdef __DEBUG1__
    t2 = omp_get_wtime();
//    double t22 = MPI_Wtime();
    if(verbose_coarsen) print_time(t1, t2, "compute_coarsen: level "+std::to_string(grid->currentLevel), grid->A->comm);
//    print_time_ave(t22-t11, "compute_coarsen: level "+std::to_string(grid->currentLevel), grid->A->comm);

//    MPI_Barrier(grid->A->comm); printf("rank %d: here after compute_coarsen!!! \n", rank); MPI_Barrier(grid->A->comm);
//    if(grid->Ac.active) print_vector(grid->Ac.split, 1, "grid->Ac.split", grid->Ac.comm);
//    if(grid->Ac.active) print_vector(grid->Ac.entry, 1, "grid->Ac.entry", grid->A->comm);

//    printf("rank = %d, M = %u, nnz_l = %lu, nnz_g = %lu, Ac.M = %u, Ac.nnz_l = %lu \n",
//           rank, grid->A->M, grid->A->nnz_l, grid->A->nnz_g, grid->Ac.M, grid->Ac.nnz_l);

//    int rank1;
//    MPI_Comm_rank(grid->A->comm, &rank1);
//    printf("Mbig = %u, M = %u, nnz_l = %lu, nnz_g = %lu \n", grid->Ac.Mbig, grid->Ac.M, grid->Ac.nnz_l, grid->Ac.nnz_g);
//    print_vector(grid->Ac.entry, 0, "grid->Ac.entry", grid->Ac.comm);
#endif

    // **************************** compute_coarsen in PETSc ****************************
//    petsc_viewer(&grid->Ac);

    // this part is only for experiments.
//    petsc_coarsen(&grid->R, grid->A, &grid->P);
//    petsc_coarsen_PtAP(&grid->R, grid->A, &grid->P);
//    petsc_coarsen_2matmult(&grid->R, grid->A, &grid->P);
//    petsc_check_matmatmat(&grid->R, grid->A, &grid->P, &grid->Ac);

//    map_matmat.clear();

    return ret_val;
}


int saena_object::create_prolongation(Grid *grid, std::vector< std::vector< std::vector<int> > > &map_all, std::vector< std::vector<int> > &g2u_all) {

    int ret_val = 0;
    if(grid->A->p_order == 1){
        ret_val = SA(grid);
    }else{
        pcoarsen(grid, map_all, g2u_all);
    }

    return ret_val;
}


int saena_object::scale_vector(std::vector<value_t>& v, std::vector<value_t>& w) {

#pragma omp parallel for
    for(index_t i = 0; i < v.size(); i++)
        v[i] *= w[i];

    return 0;
}


int saena_object::find_eig(saena_matrix& A){

//    find_eig_Elemental(A);
    find_eig_ietl(A);

    return 0;
}
