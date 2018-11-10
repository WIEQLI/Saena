#include "saena_object.h"
#include "saena_matrix.h"
#include "grid.h"
#include "aux_functions.h"
#include "dollar.hpp"
#include <cstdio>

#include <cstdlib>
#include <fstream>
#include <mpi.h>


int saena_object::update1(saena_matrix* A_new){

    // ************** update grids[0].A **************
//    this part is specific to solve_pcg_update2(), in comparison to solve_pcg().
//    the difference between this function and solve_pcg(): the finest level matrix (original LHS) is updated with
//    the new one.

    // first set A_new.eig_max_of_invdiagXA equal to the previous A's. Since we only need an upper bound, this is good enough.
    A_new->eig_max_of_invdiagXA = grids[0].A->eig_max_of_invdiagXA;

    grids[0].A = A_new;

    return 0;
}


int saena_object::update2(saena_matrix* A_new){

    // ************** update grids[i].A for all levels i **************

    // first set A_new.eig_max_of_invdiagXA equal to the previous A's. Since we only need an upper bound, this is good enough.
    // do the same for the next level matrices.
    A_new->eig_max_of_invdiagXA = grids[0].A->eig_max_of_invdiagXA;

    double eigen_temp;
    grids[0].A = A_new;
    for(int i = 0; i < max_level; i++){
        if(grids[i].A->active) {
            eigen_temp = grids[i].Ac.eig_max_of_invdiagXA;
            grids[i].Ac.erase();
            coarsen(&grids[i]);
            grids[i + 1].A = &grids[i].Ac;
            grids[i].Ac.eig_max_of_invdiagXA = eigen_temp;
//            Grid(&grids[i].Ac, max_level, i + 1);
        }
    }

    return 0;
}


int saena_object::update3(saena_matrix* A_new){

    // ************** update grids[i].A for all levels i **************

    // first set A_new.eig_max_of_invdiagXA equal to the previous A's. Since we only need an upper bound, this is good enough.
    // do the same for the next level matrices.
    A_new->eig_max_of_invdiagXA = grids[0].A->eig_max_of_invdiagXA;

    std::vector<cooEntry> A_diff;
    local_diff(*grids[0].A, *A_new, A_diff);
//    print_vector(A_diff, -1, "A_diff", grids[0].A->comm);
//    print_vector(grids[0].A->split, 0, "split", grids[0].A->comm);

    grids[0].A = A_new;
    for(int i = 0; i < max_level; i++){
        if(grids[i].A->active) {
//            if(rank==0) printf("_____________________________________\nlevel = %lu \n", i);
//            grids[i].Ac.print_entry(-1);
            coarsen_update_Ac(&grids[i], A_diff);
//            grids[i].Ac.print_entry(-1);
//            print_vector(A_diff, -1, "A_diff", grids[i].Ac.comm);
//            print_vector(grids[i+1].A->split, 0, "split", grids[i+1].A->comm);
        }
    }

//    saena_matrix* B = grids[0].Ac->get_internal_matrix();

    return 0;
}

//int saena_object::solve_pcg_update1
/*
int saena_object::solve_pcg_update1(std::vector<value_t>& u){

    MPI_Comm comm = grids[0].A->comm;
    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    // ************** check u size **************

    index_t u_size_local = u.size(), u_size_total;
    MPI_Allreduce(&u_size_local, &u_size_total, 1, MPI_UNSIGNED, MPI_SUM, grids[0].A->comm);
    if(grids[0].A->Mbig != u_size_total){
        if(rank==0) printf("Error: size of LHS (=%u) and the solution vector u (=%u) are not equal!\n", grids[0].A->Mbig, u_size_total);
        MPI_Finalize();
        return -1;
    }

    if(verbose_solve) if(rank == 0) printf("solve_pcg: check u size!\n");

    // ************** repartition u **************

    if(repartition)
        repartition_u(u);

    if(verbose_solve) if(rank == 0) printf("solve_pcg: repartition u!\n");

    // ************** solve **************

//    double temp;
//    dot(rhs, rhs, &temp, comm);
//    if(rank==0) std::cout << "norm(rhs) = " << sqrt(temp) << std::endl;

    std::vector<value_t> r(grids[0].A->M);
    grids[0].A->residual(u, grids[0].rhs, r);
    double initial_dot, current_dot, previous_dot;
    dotProduct(r, r, &initial_dot, comm);
    if(rank==0) std::cout << "******************************************************" << std::endl;
    if(rank==0) printf("\ninitial residual = %e \n\n", sqrt(initial_dot));

    // if max_level==0, it means only direct solver is being used inside the previous vcycle, and that is all needed.
    if(max_level == 0){
        vcycle(&grids[0], u, grids[0].rhs);
//        grids[0].A->print_entry(-1);
        grids[0].A->residual(u, grids[0].rhs, r);
//        print_vector(r, -1, "res", comm);
        dotProduct(r, r, &current_dot, comm);
//        if(rank==0) std::cout << "dot = " << current_dot << std::endl;

        if(rank==0){
            std::cout << "******************************************************" << std::endl;
            printf("\nfinal:\nonly using the direct solver! \nfinal absolute residual = %e"
                   "\nrelative residual       = %e \n\n", sqrt(current_dot), sqrt(current_dot/initial_dot));
            std::cout << "******************************************************" << std::endl;
        }

        // scale the solution u
        scale_vector(u, grids[0].A->inv_sq_diag);

        // repartition u back
        if(repartition)
            repartition_back_u(u);

        return 0;
    }

    std::vector<value_t> rho(grids[0].A->M, 0);
    vcycle(&grids[0], rho, r);

    if(verbose_solve) if(rank == 0) printf("solve_pcg: first vcycle!\n");

//    for(i = 0; i < r.size(); i++)
//        printf("rho[%lu] = %f,\t r[%lu] = %f \n", i, rho[i], i, r[i]);

//    if(rank==0){
//        printf("Vcycle #: absolute residual \tconvergence factor\n");
//        printf("--------------------------------------------------------\n");
//    }

    std::vector<value_t> h(grids[0].A->M);
    std::vector<value_t> p(grids[0].A->M);
    p = rho;

    int i;
    previous_dot = initial_dot;
    current_dot  = initial_dot;
    double rho_res, pdoth, alpha, beta;
    for(i = 0; i < vcycle_num; i++){
        grids[0].A->matvec(p, h);
        dotProduct(r, rho, &rho_res, comm);
        dotProduct(p, h, &pdoth, comm);
        alpha = rho_res / pdoth;
//        printf("rho_res = %e, pdoth = %e, alpha = %f \n", rho_res, pdoth, alpha);

#pragma omp parallel for
        for(index_t j = 0; j < u.size(); j++){
//            if(rank==0) printf("before u = %.10f \tp = %.10f \talpha = %f \n", u[j], p[j], alpha);
            u[j] -= alpha * p[j];
            r[j] -= alpha * h[j];
//            if(rank==0) printf("after  u = %.10f \tp = %.10f \talpha = %f \n", u[j], p[j], alpha);
        }

//        print_vector(u, -1, "v inside solve_pcg", grids[0].A->comm);

        previous_dot = current_dot;
        dotProduct(r, r, &current_dot, comm);
        // print the "absolute residual" and the "convergence factor":
//        if(rank==0) printf("Vcycle %d: %.10f  \t%.10f \n", i+1, sqrt(current_dot), sqrt(current_dot/previous_dot));
//        if(rank==0) printf("Vcycle %lu: aboslute residual = %.10f \n", i+1, sqrt(current_dot));
        if( current_dot/initial_dot < relative_tolerance * relative_tolerance )
            break;

        if(verbose) if(rank==0) printf("_______________________________ \n\n***** Vcycle %u *****\n", i+1);
        std::fill(rho.begin(), rho.end(), 0);
        vcycle(&grids[0], rho, r);
        dotProduct(r, rho, &beta, comm);
        beta /= rho_res;

#pragma omp parallel for
        for(index_t j = 0; j < u.size(); j++)
            p[j] = rho[j] + beta * p[j];
//        printf("beta = %e \n", beta);
    }

    // set number of iterations that took to find the solution
    // only do the following if the end of the previous for loop was reached.
    if(i == vcycle_num)
        i--;

    if(rank==0){
        std::cout << "\n******************************************************" << std::endl;
        printf("\nfinal:\nstopped at iteration    = %d \nfinal absolute residual = %e"
               "\nrelative residual       = %e \n\n", i+1, sqrt(current_dot), sqrt(current_dot/initial_dot));
        std::cout << "******************************************************" << std::endl;
    }

    if(verbose_solve) if(rank == 0) printf("solve_pcg: solve!\n");

    // ************** scale u **************

    scale_vector(u, grids[0].A->inv_sq_diag);

    // ************** repartition u back **************

//    print_vector(u, 2, "final u before repartition_back_u", comm);

    if(repartition)
        repartition_back_u(u);

    if(verbose_solve) if(rank == 0) printf("solve_pcg: repartition back u!\n");

//     print_vector(u, 0, "final u", comm);

    return 0;
}
*/

//int saena_object::solve_pcg_update1
/*
int saena_object::solve_pcg_update1(std::vector<value_t>& u, saena_matrix* A_new){

    MPI_Comm comm = grids[0].A->comm;
    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);
    bool solve_verbose = false;

    // ************** update grids[0].A **************
//    this part is specific to solve_pcg_update2(), in comparison to solve_pcg().
//    the difference between this function and solve_pcg(): the finest level matrix (original LHS) is updated with
//    the new one.

    // first set A_new.eig_max_of_invdiagXA equal to the previous A's. Since we only need an upper bound, this is good enough.
    A_new->eig_max_of_invdiagXA = grids[0].A->eig_max_of_invdiagXA;

    grids[0].A = A_new;

    // ************** check u size **************

    index_t u_size_local = u.size(), u_size_total;
    MPI_Allreduce(&u_size_local, &u_size_total, 1, MPI_UNSIGNED, MPI_SUM, grids[0].A->comm);
    if(grids[0].A->Mbig != u_size_total){
        if(rank==0) printf("Error: size of LHS (=%u) and the solution vector u (=%u) are not equal!\n", grids[0].A->Mbig, u_size_total);
        MPI_Finalize();
        return -1;
    }

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: check u size!\n");

    // ************** repartition u **************

    if(repartition)
        repartition_u(u);

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: repartition u!\n");

    // ************** solve **************

//    double temp;
//    dot(rhs, rhs, &temp, comm);
//    if(rank==0) std::cout << "norm(rhs) = " << sqrt(temp) << std::endl;

    std::vector<value_t> r(grids[0].A->M);
    grids[0].A->residual(u, grids[0].rhs, r);
    double initial_dot, current_dot, previous_dot;
    dotProduct(r, r, &initial_dot, comm);
    if(rank==0) std::cout << "******************************************************" << std::endl;
    if(rank==0) printf("\nsolve_pcg_update1\n");
    if(rank==0) printf("\ninitial residual = %e \n\n", sqrt(initial_dot));

    // if max_level==0, it means only direct solver is being used inside the previous vcycle, and that is all needed.
    if(max_level == 0){
        vcycle(&grids[0], u, grids[0].rhs);
        grids[0].A->residual(u, grids[0].rhs, r);
        dotProduct(r, r, &current_dot, comm);

        if(rank==0){
            std::cout << "******************************************************" << std::endl;
            printf("\nfinal:\nonly using the direct solver! \nfinal absolute residual = %e"
                           "\nrelative residual       = %e \n\n", sqrt(current_dot), sqrt(current_dot/initial_dot));
            std::cout << "******************************************************" << std::endl;
        }

        // scale the solution u
        scale_vector(u, grids[0].A->inv_sq_diag);

        // repartition u back
        if(repartition)
            repartition_back_u(u);

        return 0;
    }

    std::vector<value_t> rho(grids[0].A->M, 0);
    vcycle(&grids[0], rho, r);

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: first vcycle!\n");

//    for(i = 0; i < r.size(); i++)
//        printf("rho[%lu] = %f,\t r[%lu] = %f \n", i, rho[i], i, r[i]);

    std::vector<value_t> h(grids[0].A->M);
    std::vector<value_t> p(grids[0].A->M);
    p = rho;

    int i;
    previous_dot = initial_dot;
    current_dot  = initial_dot;
    double rho_res, pdoth, alpha, beta;
    for(i = 0; i < vcycle_num; i++){
        grids[0].A->matvec(p, h);
        dotProduct(r, rho, &rho_res, comm);
        dotProduct(p, h, &pdoth, comm);
        alpha = rho_res / pdoth;
//        printf("rho_res = %e, pdoth = %e, alpha = %f \n", rho_res, pdoth, alpha);

#pragma omp parallel for
        for(index_t j = 0; j < u.size(); j++){
            u[j] -= alpha * p[j];
            r[j] -= alpha * h[j];
        }

        previous_dot = current_dot;
        dotProduct(r, r, &current_dot, comm);
        // this prints the "absolute residual" and the "convergence factor":
//        if(rank==0) printf("Vcycle %d: %.10f  \t%.10f \n", i+1, sqrt(current_dot), sqrt(current_dot/previous_dot));
//        if(rank==0) printf("Vcycle %lu: aboslute residual = %.10f \n", i+1, sqrt(current_dot));
        if( current_dot/initial_dot < relative_tolerance * relative_tolerance )
            break;

        if(verbose) if(rank==0) printf("_______________________________ \n\n***** Vcycle %u *****\n", i+1);
        std::fill(rho.begin(), rho.end(), 0);
        vcycle(&grids[0], rho, r);
        dotProduct(r, rho, &beta, comm);
        beta /= rho_res;

#pragma omp parallel for
        for(index_t j = 0; j < u.size(); j++)
            p[j] = rho[j] + beta * p[j];
//        printf("beta = %e \n", beta);
    }

    // set number of iterations that took to find the solution
    // only do the following if the end of the previous for loop was reached.
    if(i == vcycle_num)
        i--;

    if(rank==0){
        std::cout << "******************************************************" << std::endl;
        printf("\nfinal:\nstopped at iteration    = %d \nfinal absolute residual = %e"
                       "\nrelative residual       = %e \n\n", i+1, sqrt(current_dot), sqrt(current_dot/initial_dot));
        std::cout << "******************************************************" << std::endl;
    }

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: solve!\n");

    // ************** scale u **************

    scale_vector(u, grids[0].A->inv_sq_diag);

    // ************** repartition u back **************

    if(repartition)
        repartition_back_u(u);

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: repartition back u!\n");

    return 0;
}
*/

//int saena_object::solve_pcg_update2
/*
int saena_object::solve_pcg_update2(std::vector<value_t>& u){

    MPI_Comm comm = grids[0].A->comm;
    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    // ************** check u size **************

    index_t u_size_local = u.size(), u_size_total;
    MPI_Allreduce(&u_size_local, &u_size_total, 1, MPI_UNSIGNED, MPI_SUM, grids[0].A->comm);
    if(grids[0].A->Mbig != u_size_total){
        if(rank==0) printf("Error: size of LHS (=%u) and the solution vector u (=%u) are not equal!\n", grids[0].A->Mbig, u_size_total);
        MPI_Finalize();
        return -1;
    }

    if(verbose_solve) if(rank == 0) printf("solve_pcg: check u size!\n");

    // ************** repartition u **************

    if(repartition)
        repartition_u(u);

    if(verbose_solve) if(rank == 0) printf("solve_pcg: repartition u!\n");

    // ************** solve **************

//    double temp;
//    dot(rhs, rhs, &temp, comm);
//    if(rank==0) std::cout << "norm(rhs) = " << sqrt(temp) << std::endl;

    std::vector<value_t> r(grids[0].A->M);
    grids[0].A->residual(u, grids[0].rhs, r);
    double initial_dot, current_dot, previous_dot;
    dotProduct(r, r, &initial_dot, comm);
    if(rank==0) std::cout << "******************************************************" << std::endl;
    if(rank==0) printf("\ninitial residual = %e \n\n", sqrt(initial_dot));

    // if max_level==0, it means only direct solver is being used inside the previous vcycle, and that is all needed.
    if(max_level == 0){
        vcycle(&grids[0], u, grids[0].rhs);
//        grids[0].A->print_entry(-1);
        grids[0].A->residual(u, grids[0].rhs, r);
//        print_vector(r, -1, "res", comm);
        dotProduct(r, r, &current_dot, comm);
//        if(rank==0) std::cout << "dot = " << current_dot << std::endl;

        if(rank==0){
            std::cout << "******************************************************" << std::endl;
            printf("\nfinal:\nonly using the direct solver! \nfinal absolute residual = %e"
                   "\nrelative residual       = %e \n\n", sqrt(current_dot), sqrt(current_dot/initial_dot));
            std::cout << "******************************************************" << std::endl;
        }

        // scale the solution u
        scale_vector(u, grids[0].A->inv_sq_diag);

        // repartition u back
        if(repartition)
            repartition_back_u(u);

        return 0;
    }

    std::vector<value_t> rho(grids[0].A->M, 0);
    vcycle(&grids[0], rho, r);

    if(verbose_solve) if(rank == 0) printf("solve_pcg: first vcycle!\n");

//    for(i = 0; i < r.size(); i++)
//        printf("rho[%lu] = %f,\t r[%lu] = %f \n", i, rho[i], i, r[i]);

//    if(rank==0){
//        printf("Vcycle #: absolute residual \tconvergence factor\n");
//        printf("--------------------------------------------------------\n");
//    }

    std::vector<value_t> h(grids[0].A->M);
    std::vector<value_t> p(grids[0].A->M);
    p = rho;

    int i;
    previous_dot = initial_dot;
    current_dot  = initial_dot;
    double rho_res, pdoth, alpha, beta;
    for(i = 0; i < vcycle_num; i++){
        grids[0].A->matvec(p, h);
        dotProduct(r, rho, &rho_res, comm);
        dotProduct(p, h, &pdoth, comm);
        alpha = rho_res / pdoth;
//        printf("rho_res = %e, pdoth = %e, alpha = %f \n", rho_res, pdoth, alpha);

#pragma omp parallel for
        for(index_t j = 0; j < u.size(); j++){
//            if(rank==0) printf("before u = %.10f \tp = %.10f \talpha = %f \n", u[j], p[j], alpha);
            u[j] -= alpha * p[j];
            r[j] -= alpha * h[j];
//            if(rank==0) printf("after  u = %.10f \tp = %.10f \talpha = %f \n", u[j], p[j], alpha);
        }

//        print_vector(u, -1, "v inside solve_pcg", grids[0].A->comm);

        previous_dot = current_dot;
        dotProduct(r, r, &current_dot, comm);
        // print the "absolute residual" and the "convergence factor":
//        if(rank==0) printf("Vcycle %d: %.10f  \t%.10f \n", i+1, sqrt(current_dot), sqrt(current_dot/previous_dot));
//        if(rank==0) printf("Vcycle %lu: aboslute residual = %.10f \n", i+1, sqrt(current_dot));
        if( current_dot/initial_dot < relative_tolerance * relative_tolerance )
            break;

        if(verbose) if(rank==0) printf("_______________________________ \n\n***** Vcycle %u *****\n", i+1);
        std::fill(rho.begin(), rho.end(), 0);
        vcycle(&grids[0], rho, r);
        dotProduct(r, rho, &beta, comm);
        beta /= rho_res;

#pragma omp parallel for
        for(index_t j = 0; j < u.size(); j++)
            p[j] = rho[j] + beta * p[j];
//        printf("beta = %e \n", beta);
    }

    // set number of iterations that took to find the solution
    // only do the following if the end of the previous for loop was reached.
    if(i == vcycle_num)
        i--;

    if(rank==0){
        std::cout << "\n******************************************************" << std::endl;
        printf("\nfinal:\nstopped at iteration    = %d \nfinal absolute residual = %e"
               "\nrelative residual       = %e \n\n", i+1, sqrt(current_dot), sqrt(current_dot/initial_dot));
        std::cout << "******************************************************" << std::endl;
    }

    if(verbose_solve) if(rank == 0) printf("solve_pcg: solve!\n");

    // ************** scale u **************

    scale_vector(u, grids[0].A->inv_sq_diag);

    // ************** repartition u back **************

//    print_vector(u, 2, "final u before repartition_back_u", comm);

    if(repartition)
        repartition_back_u(u);

    if(verbose_solve) if(rank == 0) printf("solve_pcg: repartition back u!\n");

//     print_vector(u, 0, "final u", comm);

    return 0;
}
*/

//int saena_object::solve_pcg_update2
/*
int saena_object::solve_pcg_update2(std::vector<value_t>& u, saena_matrix* A_new){

    MPI_Comm comm = grids[0].A->comm;
    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);
    unsigned long i, j;
    bool solve_verbose = false;

    // ************** update grids[i].A for all levels i **************

    // first set A_new.eig_max_of_invdiagXA equal to the previous A's. Since we only need an upper bound, this is good enough.
    // do the same for the next level matrices.
    A_new->eig_max_of_invdiagXA = grids[0].A->eig_max_of_invdiagXA;

    double eigen_temp;
    grids[0].A = A_new;
    for(i = 0; i < max_level; i++){
        if(grids[i].A->active) {
            eigen_temp = grids[i].Ac.eig_max_of_invdiagXA;
            grids[i].Ac.erase();
            coarsen(&grids[i]);
            grids[i + 1].A = &grids[i].Ac;
            grids[i].Ac.eig_max_of_invdiagXA = eigen_temp;
//            Grid(&grids[i].Ac, max_level, i + 1);
        }
    }

    // ************** check u size **************

    unsigned int u_size_local = u.size();
    unsigned int u_size_total;
    MPI_Allreduce(&u_size_local, &u_size_total, 1, MPI_UNSIGNED, MPI_SUM, grids[0].A->comm);
    if(grids[0].A->Mbig != u_size_total){
        if(rank==0) printf("Error: size of LHS (=%u) and the solution vector u (=%u) are not equal!\n", grids[0].A->Mbig, u_size_total);
        MPI_Finalize();
        return -1;
    }

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: check u size!\n");

    // ************** repartition u **************
    if(repartition)
        repartition_u(u);

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: repartition u!\n");

    // ************** solve **************

//    double temp;
//    dot(rhs, rhs, &temp, comm);
//    if(rank==0) std::cout << "norm(rhs) = " << sqrt(temp) << std::endl;

    std::vector<double> r(grids[0].A->M);
    grids[0].A->residual(u, grids[0].rhs, r);
    double initial_dot, current_dot, previous_dot;
    dotProduct(r, r, &initial_dot, comm);
    if(rank==0) std::cout << "******************************************************" << std::endl;
    if(rank==0) printf("\nsolve_pcg_update2\n");
    if(rank==0) printf("\ninitial residual = %e \n\n", sqrt(initial_dot));

    // if max_level==0, it means only direct solver is being used inside the previous vcycle, and that is all needed.
    if(max_level == 0){

        vcycle(&grids[0], u, grids[0].rhs);
        grids[0].A->residual(u, grids[0].rhs, r);
        dotProduct(r, r, &current_dot, comm);

        if(rank==0){
            std::cout << "******************************************************" << std::endl;
            printf("\nfinal:\nonly using the direct solver! \nfinal absolute residual = %e"
                           "\nrelative residual       = %e \n\n", sqrt(current_dot), sqrt(current_dot/initial_dot));
            std::cout << "******************************************************" << std::endl;
        }

        // scale the solution u
        scale_vector(u, grids[0].A->inv_sq_diag);

        // repartition u back
        if(repartition)
            repartition_back_u(u);

        return 0;
    }

    std::vector<double> rho(grids[0].A->M, 0);
    vcycle(&grids[0], rho, r);

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: first vcycle!\n");

//    for(i = 0; i < r.size(); i++)
//        printf("rho[%lu] = %f,\t r[%lu] = %f \n", i, rho[i], i, r[i]);

    std::vector<double> h(grids[0].A->M);
    std::vector<double> p(grids[0].A->M);
    p = rho;

    previous_dot = initial_dot;
    current_dot  = initial_dot;
    double rho_res, pdoth, alpha, beta;
    for(i=0; i<vcycle_num; i++){
        grids[0].A->matvec(p, h);
        dotProduct(r, rho, &rho_res, comm);
        dotProduct(p, h, &pdoth, comm);
        alpha = rho_res / pdoth;
//        printf("rho_res = %e, pdoth = %e, alpha = %f \n", rho_res, pdoth, alpha);

#pragma omp parallel for
        for(j = 0; j < u.size(); j++){
            u[j] -= alpha * p[j];
            r[j] -= alpha * h[j];
        }

        previous_dot = current_dot;
        dotProduct(r, r, &current_dot, comm);
        if( current_dot/initial_dot < relative_tolerance * relative_tolerance )
            break;

        if(verbose) if(rank==0) printf("_______________________________ \n\n***** Vcycle %lu *****\n", i+1);
        // this prints the "absolute residual" and the "convergence factor":
//        if(rank==0) printf("Vcycle %lu: %.10f  \t%.10f \n", i+1, sqrt(current_dot), sqrt(current_dot/previous_dot));
        std::fill(rho.begin(), rho.end(), 0);
        vcycle(&grids[0], rho, r);
        dotProduct(r, rho, &beta, comm);
        beta /= rho_res;

#pragma omp parallel for
        for(j = 0; j < u.size(); j++)
            p[j] = rho[j] + beta * p[j];
//        printf("beta = %e \n", beta);
    }

    // set number of iterations that took to find the solution
    // only do the following if the end of the previous for loop was reached.
    if(i == vcycle_num)
        i--;

    if(rank==0){
        std::cout << "******************************************************" << std::endl;
        printf("\nfinal:\nstopped at iteration    = %ld \nfinal absolute residual = %e"
                       "\nrelative residual       = %e \n\n", ++i, sqrt(current_dot), sqrt(current_dot/initial_dot));
        std::cout << "******************************************************" << std::endl;
    }

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: solve!\n");

    // ************** scale u **************

    scale_vector(u, grids[0].A->inv_sq_diag);

    // ************** repartition u back **************

    if(repartition)
        repartition_back_u(u);

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: repartition back u!\n");

    return 0;
}
*/

//int saena_object::solve_pcg_update3
/*
int saena_object::solve_pcg_update3(std::vector<value_t>& u){

    MPI_Comm comm = grids[0].A->comm;
    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    // ************** check u size **************

    index_t u_size_local = u.size(), u_size_total;
    MPI_Allreduce(&u_size_local, &u_size_total, 1, MPI_UNSIGNED, MPI_SUM, grids[0].A->comm);
    if(grids[0].A->Mbig != u_size_total){
        if(rank==0) printf("Error: size of LHS (=%u) and the solution vector u (=%u) are not equal!\n", grids[0].A->Mbig, u_size_total);
        MPI_Finalize();
        return -1;
    }

    if(verbose_solve) if(rank == 0) printf("solve_pcg: check u size!\n");

    // ************** repartition u **************

    if(repartition)
        repartition_u(u);

    if(verbose_solve) if(rank == 0) printf("solve_pcg: repartition u!\n");

    // ************** solve **************

//    double temp;
//    dot(rhs, rhs, &temp, comm);
//    if(rank==0) std::cout << "norm(rhs) = " << sqrt(temp) << std::endl;

    std::vector<value_t> r(grids[0].A->M);
    grids[0].A->residual(u, grids[0].rhs, r);
    double initial_dot, current_dot, previous_dot;
    dotProduct(r, r, &initial_dot, comm);
    if(rank==0) std::cout << "******************************************************" << std::endl;
    if(rank==0) printf("\ninitial residual = %e \n\n", sqrt(initial_dot));

    // if max_level==0, it means only direct solver is being used inside the previous vcycle, and that is all needed.
    if(max_level == 0){
        vcycle(&grids[0], u, grids[0].rhs);
//        grids[0].A->print_entry(-1);
        grids[0].A->residual(u, grids[0].rhs, r);
//        print_vector(r, -1, "res", comm);
        dotProduct(r, r, &current_dot, comm);
//        if(rank==0) std::cout << "dot = " << current_dot << std::endl;

        if(rank==0){
            std::cout << "******************************************************" << std::endl;
            printf("\nfinal:\nonly using the direct solver! \nfinal absolute residual = %e"
                   "\nrelative residual       = %e \n\n", sqrt(current_dot), sqrt(current_dot/initial_dot));
            std::cout << "******************************************************" << std::endl;
        }

        // scale the solution u
        scale_vector(u, grids[0].A->inv_sq_diag);

        // repartition u back
        if(repartition)
            repartition_back_u(u);

        return 0;
    }

    std::vector<value_t> rho(grids[0].A->M, 0);
    vcycle(&grids[0], rho, r);

    if(verbose_solve) if(rank == 0) printf("solve_pcg: first vcycle!\n");

//    for(i = 0; i < r.size(); i++)
//        printf("rho[%lu] = %f,\t r[%lu] = %f \n", i, rho[i], i, r[i]);

//    if(rank==0){
//        printf("Vcycle #: absolute residual \tconvergence factor\n");
//        printf("--------------------------------------------------------\n");
//    }

    std::vector<value_t> h(grids[0].A->M);
    std::vector<value_t> p(grids[0].A->M);
    p = rho;

    int i;
    previous_dot = initial_dot;
    current_dot  = initial_dot;
    double rho_res, pdoth, alpha, beta;
    for(i = 0; i < vcycle_num; i++){
        grids[0].A->matvec(p, h);
        dotProduct(r, rho, &rho_res, comm);
        dotProduct(p, h, &pdoth, comm);
        alpha = rho_res / pdoth;
//        printf("rho_res = %e, pdoth = %e, alpha = %f \n", rho_res, pdoth, alpha);

#pragma omp parallel for
        for(index_t j = 0; j < u.size(); j++){
//            if(rank==0) printf("before u = %.10f \tp = %.10f \talpha = %f \n", u[j], p[j], alpha);
            u[j] -= alpha * p[j];
            r[j] -= alpha * h[j];
//            if(rank==0) printf("after  u = %.10f \tp = %.10f \talpha = %f \n", u[j], p[j], alpha);
        }

//        print_vector(u, -1, "v inside solve_pcg", grids[0].A->comm);

        previous_dot = current_dot;
        dotProduct(r, r, &current_dot, comm);
        // print the "absolute residual" and the "convergence factor":
//        if(rank==0) printf("Vcycle %d: %.10f  \t%.10f \n", i+1, sqrt(current_dot), sqrt(current_dot/previous_dot));
//        if(rank==0) printf("Vcycle %lu: aboslute residual = %.10f \n", i+1, sqrt(current_dot));
        if( current_dot/initial_dot < relative_tolerance * relative_tolerance )
            break;

        if(verbose) if(rank==0) printf("_______________________________ \n\n***** Vcycle %u *****\n", i+1);
        std::fill(rho.begin(), rho.end(), 0);
        vcycle(&grids[0], rho, r);
        dotProduct(r, rho, &beta, comm);
        beta /= rho_res;

#pragma omp parallel for
        for(index_t j = 0; j < u.size(); j++)
            p[j] = rho[j] + beta * p[j];
//        printf("beta = %e \n", beta);
    }

    // set number of iterations that took to find the solution
    // only do the following if the end of the previous for loop was reached.
    if(i == vcycle_num)
        i--;

    if(rank==0){
        std::cout << "\n******************************************************" << std::endl;
        printf("\nfinal:\nstopped at iteration    = %d \nfinal absolute residual = %e"
               "\nrelative residual       = %e \n\n", i+1, sqrt(current_dot), sqrt(current_dot/initial_dot));
        std::cout << "******************************************************" << std::endl;
    }

    if(verbose_solve) if(rank == 0) printf("solve_pcg: solve!\n");

    // ************** scale u **************

    scale_vector(u, grids[0].A->inv_sq_diag);

    // ************** repartition u back **************

//    print_vector(u, 2, "final u before repartition_back_u", comm);

    if(repartition)
        repartition_back_u(u);

    if(verbose_solve) if(rank == 0) printf("solve_pcg: repartition back u!\n");

//     print_vector(u, 0, "final u", comm);

    return 0;
}
*/

//int saena_object::solve_pcg_update3
/*
int saena_object::solve_pcg_update3(std::vector<value_t>& u, saena_matrix* A_new){

    MPI_Comm comm = grids[0].A->comm;
    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);
    unsigned long i, j;
    bool solve_verbose = false;

    // ************** update grids[i].A for all levels i **************

    // first set A_new.eig_max_of_invdiagXA equal to the previous A's. Since we only need an upper bound, this is good enough.
    // do the same for the next level matrices.
    A_new->eig_max_of_invdiagXA = grids[0].A->eig_max_of_invdiagXA;

    std::vector<cooEntry> A_diff;
    local_diff(*grids[0].A, *A_new, A_diff);
//    print_vector(A_diff, -1, "A_diff", grids[0].A->comm);
//    print_vector(grids[0].A->split, 0, "split", grids[0].A->comm);

    grids[0].A = A_new;
    for(i = 0; i < max_level; i++){
        if(grids[i].A->active) {
//            if(rank==0) printf("_____________________________________\nlevel = %lu \n", i);
//            grids[i].Ac.print_entry(-1);
            coarsen_update_Ac(&grids[i], A_diff);
//            grids[i].Ac.print_entry(-1);
//            print_vector(A_diff, -1, "A_diff", grids[i].Ac.comm);
//            print_vector(grids[i+1].A->split, 0, "split", grids[i+1].A->comm);
        }
    }

//    saena_matrix* B = grids[0].Ac->get_internal_matrix();

    // ************** check u size **************

    unsigned int u_size_local = u.size();
    unsigned int u_size_total;
    MPI_Allreduce(&u_size_local, &u_size_total, 1, MPI_UNSIGNED, MPI_SUM, grids[0].A->comm);
    if(grids[0].A->Mbig != u_size_total){
        if(rank==0) printf("Error: size of LHS (=%u) and the solution vector u (=%u) are not equal!\n", grids[0].A->Mbig, u_size_total);
        MPI_Finalize();
        return -1;
    }

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: check u size!\n");

    // ************** repartition u **************

    if(repartition)
        repartition_u(u);

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: repartition u!\n");

    // ************** solve **************

//    double temp;
//    dot(rhs, rhs, &temp, comm);
//    if(rank==0) std::cout << "norm(rhs) = " << sqrt(temp) << std::endl;

    std::vector<double> r(grids[0].A->M);
    grids[0].A->residual(u, grids[0].rhs, r);
    double initial_dot, current_dot, previous_dot;
    dotProduct(r, r, &initial_dot, comm);
    if(rank==0) std::cout << "******************************************************" << std::endl;
    if(rank==0) printf("\nsolve_pcg_update3\n");
    if(rank==0) printf("\ninitial residual = %e \n\n", sqrt(initial_dot));

    // if max_level==0, it means only direct solver is being used inside the previous vcycle, and that is all needed.
    if(max_level == 0){

        vcycle(&grids[0], u, grids[0].rhs);
        grids[0].A->residual(u, grids[0].rhs, r);
        dotProduct(r, r, &current_dot, comm);

        if(rank==0){
            std::cout << "******************************************************" << std::endl;
            printf("\nfinal:\nonly using the direct solver! \nfinal absolute residual = %e"
                           "\nrelative residual       = %e \n\n", sqrt(current_dot), sqrt(current_dot/initial_dot));
            std::cout << "******************************************************" << std::endl;
        }

        // scale the solution u
        scale_vector(u, grids[0].A->inv_sq_diag);

        // repartition u back
        if(repartition)
            repartition_back_u(u);

        return 0;
    }

    std::vector<double> rho(grids[0].A->M, 0);
    vcycle(&grids[0], rho, r);

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: first vcycle!\n");

//    for(i = 0; i < r.size(); i++)
//        printf("rho[%lu] = %f,\t r[%lu] = %f \n", i, rho[i], i, r[i]);

    std::vector<double> h(grids[0].A->M);
    std::vector<double> p(grids[0].A->M);
    p = rho;

    previous_dot = initial_dot;
    current_dot = initial_dot;
    double rho_res, pdoth, alpha, beta;
    for(i=0; i<vcycle_num; i++){
        grids[0].A->matvec(p, h);
        dotProduct(r, rho, &rho_res, comm);
        dotProduct(p, h, &pdoth, comm);
        alpha = rho_res / pdoth;
//        printf("rho_res = %e, pdoth = %e, alpha = %f \n", rho_res, pdoth, alpha);

#pragma omp parallel for
        for(j = 0; j < u.size(); j++){
            u[j] -= alpha * p[j];
            r[j] -= alpha * h[j];
        }

        previous_dot = current_dot;
        dotProduct(r, r, &current_dot, comm);
        if( current_dot/initial_dot < relative_tolerance * relative_tolerance )
            break;

        if(verbose || solve_verbose) if(rank==0) printf("_______________________________ \n\n***** Vcycle %lu *****\n", i+1);
        // this prints the "absolute residual" and the "convergence factor":
//        if(rank==0) printf("Vcycle %lu: %.10f  \t%.10f \n", i+1, sqrt(current_dot), sqrt(current_dot/previous_dot));
        std::fill(rho.begin(), rho.end(), 0);
        vcycle(&grids[0], rho, r);
        dotProduct(r, rho, &beta, comm);
        beta /= rho_res;

#pragma omp parallel for
        for(j = 0; j < u.size(); j++)
            p[j] = rho[j] + beta * p[j];
//        printf("beta = %e \n", beta);
    }

    // set number of iterations that took to find the solution
    // only do the following if the end of the previous for loop was reached.
    if(i == vcycle_num)
        i--;

    if(rank==0){
        std::cout << "******************************************************" << std::endl;
        printf("\nfinal:\nstopped at iteration    = %ld \nfinal absolute residual = %e"
                       "\nrelative residual       = %e \n\n", ++i, sqrt(current_dot), sqrt(current_dot/initial_dot));
        std::cout << "******************************************************" << std::endl;
    }

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: solve!\n");

    // ************** scale u **************

    scale_vector(u, grids[0].A->inv_sq_diag);

    // ************** repartition u back **************

    if(repartition)
        repartition_back_u(u);

    if(solve_verbose) if(rank == 0) printf("verbose: solve_pcg_update: repartition back u!\n");

    return 0;
}
*/


int saena_object::local_diff(saena_matrix &A, saena_matrix &B, std::vector<cooEntry> &C){

    if(A.active){

        MPI_Comm comm = A.comm;
        int nprocs, rank;
        MPI_Comm_size(comm, &nprocs);
        MPI_Comm_rank(comm, &rank);

        if(A.nnz_g != B.nnz_g)
            if(rank==0) std::cout << "error: local_diff(): A.nnz_g != B.nnz_g" << std::endl;

//        C.clear();
        C.resize(A.nnz_l_local);
        index_t loc_size = 0;
        for(nnz_t i = 0; i < A.nnz_l_local; i++){
            if(!almost_zero(A.values_local[i] - B.values_local[i])){
//                if(rank==1) printf("%u \t%u \t%f \n", A.row_local[i], A.col_local[i], A.values_local[i]-B.values_local[i]);
                C[loc_size] = cooEntry(A.row_local[i], A.col_local[i], B.values_local[i]-A.values_local[i]);
                loc_size++;
            }
        }
        C.resize(loc_size);

        // this part sets the parameters needed to be set until the end of repartition().
//        C.Mbig = A.Mbig;
//        C.M = A.M;
//        C.split = A.split;
//        C.nnz_l = loc_size;
//        C.nnz_l_local = C.nnz_l;
//        MPI_Allreduce(&C.nnz_l, &C.nnz_g, 1, MPI_UNSIGNED_LONG, MPI_SUM, C.comm);

        // the only part needed from matrix_setup() for coarsen2().
//        C.indicesP_local.resize(C.nnz_l_local);
//#pragma omp parallel for
//        for (nnz_t i = 0; i < C.nnz_l_local; i++)
//            C.indicesP_local[i] = i;
//        index_t *row_localP = &*C.row_local.begin();
//        std::sort(&C.indicesP_local[0], &C.indicesP_local[C.nnz_l_local], sort_indices(row_localP));

//        C.matrix_setup();
    }

    return 0;
}


int saena_object::coarsen_update_Ac(Grid *grid, std::vector<cooEntry> &diff){

    // This function computes delta_Ac = RAP in which A is diff = diag_block(A) - diag_block(A_new) at each level.
    // Then, updates Ac with the delta_Ac. Finally, it saves delta_Ac in diff for doing the same operation
    // for the next level.

    saena_matrix *A    = grid->A;
    prolong_matrix *P  = &grid->P;
    restrict_matrix *R = &grid->R;
    saena_matrix *Ac   = &grid->Ac;

    MPI_Comm comm = A->comm;
//    Ac->active_old_comm = true;

    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    if(verbose_coarsen2){
        MPI_Barrier(comm);
        printf("start of coarsen2: rank = %d, nprocs: %d, P.nnz_l = %lu, P.nnz_g = %lu, R.nnz_l = %lu,"
               " R.nnz_g = %lu, R.M = %u, R->nnz_l_local = %lu, R->nnz_l_remote = %lu \n\n", rank, nprocs,
               P->nnz_l, P->nnz_g, R->nnz_l, R->nnz_g, R->M, R->nnz_l_local, R->nnz_l_remote);
    }

    prolong_matrix RA_temp(comm); // RA_temp is being used to remove duplicates while pushing back to RA.

    // ************************************* RA_temp - A local *************************************
    // Some local and remote elements of RA_temp are computed here using local R and local A.
    // Note: A local means whole entries of A on this process, not just the diagonal block.

    std::sort(diff.begin(), diff.end(), row_major);

    // alloacted memory for AMaxM, instead of A.M to avoid reallocation of memory for when receiving data from other procs.
    unsigned int* AnnzPerRow = (unsigned int*)malloc(sizeof(unsigned int)*A->M);
    std::fill(&AnnzPerRow[0], &AnnzPerRow[A->M], 0);
    for(nnz_t i = 0; i < diff.size(); i++){
//        if(rank==0) printf("%u\n", diff[i].row);
        AnnzPerRow[diff[i].row]++;
    }

//    MPI_Barrier(A->comm);
//    if(rank==0){
//        printf("rank = %d, AnnzPerRow: \n", rank);
//        for(long i=0; i<A->M; i++)
//            printf("%lu \t%u \n", i, AnnzPerRow[i]);}

    // alloacted memory for AMaxM+1, instead of A.M+1 to avoid reallocation of memory for when receiving data from other procs.
    unsigned int* AnnzPerRowScan = (unsigned int*)malloc(sizeof(unsigned int)*(A->M+1));
    AnnzPerRowScan[0] = 0;
    for(index_t i = 0; i < A->M; i++){
        AnnzPerRowScan[i+1] = AnnzPerRowScan[i] + AnnzPerRow[i];
//        if(rank==1) printf("i=%lu, AnnzPerRow=%d, AnnzPerRowScan = %d\n", i+A->split[rank], AnnzPerRow[i], AnnzPerRowScan[i+1]);
    }

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 1: rank = %d\n", rank); MPI_Barrier(comm);}

    index_t jstart, jend;
    if(!R->entry_local.empty()) {
        for (index_t i = 0; i < R->nnz_l_local; i++) {
//            if(rank==0) std::cout << "i=" << i << "\tR[" << R->entry_local[i].row << ", " << R->entry_local[i].col
//                                  << "]=" << R->entry_local[i].val << std::endl;
            jstart = AnnzPerRowScan[R->entry_local[i].col - P->split[rank]];
            jend   = AnnzPerRowScan[R->entry_local[i].col - P->split[rank] + 1];
            if(jend - jstart == 0) continue;
            for (index_t j = jstart; j < jend; j++) {
//                if(rank==0) std::cout << "i=" << i << ", j=" << j
//                                      << "   \tdiff[" << diff[j].row
//                                      << ", " << diff[j].col << "]=\t" << diff[j].val
//                                      << "         \tR[" << R->entry_local[i].row << ", " << R->entry_local[i].col
//                                      << "]=\t" << R->entry_local[i].val << std::endl;
                RA_temp.entry.push_back(cooEntry(R->entry_local[i].row,
                                                 diff[j].col,
                                                 R->entry_local[i].val * diff[j].val));
            }
        }
    }

//    print_vector(RA_temp.entry, -1, "RA_temp.entry", comm);

    free(AnnzPerRow);
    free(AnnzPerRowScan);

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 2: rank = %d\n", rank); MPI_Barrier(comm);}

    std::sort(RA_temp.entry.begin(), RA_temp.entry.end());

//    print_vector(RA_temp.entry, -1, "RA_temp.entry: after sort", comm);

    prolong_matrix RA(comm);

    // remove duplicates.
    unsigned long entry_size = 0;
    for(nnz_t i=0; i<RA_temp.entry.size(); i++){
        RA.entry.push_back(RA_temp.entry[i]);
        while(i<RA_temp.entry.size()-1 && RA_temp.entry[i] == RA_temp.entry[i+1]){ // values of entries with the same row and col should be added.
            RA.entry.back().val += RA_temp.entry[i+1].val;
            i++;
        }
//        if(rank==1) std::cout << std::endl << "final: " << std::endl << RA.entry[RA.entry.size()-1].val << std::endl;
        entry_size++;
    }

//    print_vector(RA.entry, -1, "RA.entry", comm);

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 3: rank = %d\n", rank); MPI_Barrier(comm);}

    // ************************************* RAP_temp - P local *************************************
    // Some local and remote elements of RAP_temp are computed here.
    // Note: P local means whole entries of P on this process, not just the diagonal block.

    prolong_matrix RAP_temp(comm); // RAP_temp is being used to remove duplicates while pushing back to RAP.

    unsigned int* PnnzPerRow = (unsigned int*)malloc(sizeof(unsigned int)*P->M);
    std::fill(&PnnzPerRow[0], &PnnzPerRow[P->M], 0);
    for(nnz_t i=0; i<P->nnz_l_local; i++){
//        if(rank==1) printf("%u\n", P->entry_local[i].row);
        PnnzPerRow[P->entry_local[i].row]++;
    }

//    if(rank==1) for(long i=0; i<P->M; i++) std::cout << PnnzPerRow[i] << std::endl;

    unsigned int* PnnzPerRowScan = (unsigned int*)malloc(sizeof(unsigned int)*(P->M+1));
    PnnzPerRowScan[0] = 0;
    for(nnz_t i = 0; i < P->M; i++){
        PnnzPerRowScan[i+1] = PnnzPerRowScan[i] + PnnzPerRow[i];
//        if(rank==2) printf("i=%lu, PnnzPerRow=%d, PnnzPerRowScan = %d\n", i, PnnzPerRow[i], PnnzPerRowScan[i]);
    }

    long procNum = 0;
    std::vector<nnz_t> left_block_nnz(nprocs, 0);
    if(!RA.entry.empty()){
        for (nnz_t i = 0; i < RA.entry.size(); i++) {
            procNum = lower_bound2(&P->split[0], &P->split[nprocs], RA.entry[i].col);
            left_block_nnz[procNum]++;
//        if(rank==1) printf("rank=%d, col = %lu, procNum = %ld \n", rank, R->entry_remote[0].col, procNum);
        }
    }

    std::vector<nnz_t> left_block_nnz_scan(nprocs+1);
    left_block_nnz_scan[0] = 0;
    for(int i = 0; i < nprocs; i++)
        left_block_nnz_scan[i+1] = left_block_nnz_scan[i] + left_block_nnz[i];

//    print_vector(left_block_nnz_scan, -1, "left_block_nnz_scan", comm);

    // find row-wise ordering for A and save it in indicesP
//    std::vector<nnz_t> indicesP_Prolong(P->nnz_l_local);
//    for(nnz_t i=0; i<P->nnz_l_local; i++)
//        indicesP_Prolong[i] = i;
//    std::sort(&indicesP_Prolong[0], &indicesP_Prolong[P->nnz_l], sort_indices2(&*P->entry.begin()));

    //....................
    // note: Here we want to multiply local RA by local P, but whole RA.entry is local because of how it was made earlier in this function.
    //....................

    for(nnz_t i=left_block_nnz_scan[rank]; i<left_block_nnz_scan[rank+1]; i++){
        for(nnz_t j = PnnzPerRowScan[RA.entry[i].col - P->split[rank]]; j < PnnzPerRowScan[RA.entry[i].col - P->split[rank] + 1]; j++){

//            if(rank==3) std::cout << RA.entry[i].row + P->splitNew[rank] << "\t" << P->entry[indicesP_Prolong[j]].col << "\t" << RA.entry[i].val * P->entry[indicesP_Prolong[j]].val << std::endl;

            RAP_temp.entry.emplace_back(cooEntry(RA.entry[i].row + P->splitNew[rank],  // Ac.entry should have global indices at the end.
                                                 P->entry_local[P->indicesP_local[j]].col,
                                                 RA.entry[i].val * P->entry_local[P->indicesP_local[j]].val));
        }
    }

//    print_vector(RAP_temp.entry, 0, "RAP_temp.entry", comm);

    free(PnnzPerRow);
    free(PnnzPerRowScan);

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 4: rank = %d\n", rank); MPI_Barrier(comm);}

    std::sort(RAP_temp.entry.begin(), RAP_temp.entry.end());

//    print_vector(RAP_temp.entry, 0, "RAP_temp.entry: after sort", comm);

    // erase_keep_remote() was called on Ac, so the remote elements are already in Ac.
    // Now resize it to store new local entries.
    Ac->entry_temp.resize(RAP_temp.entry.size());

    // remove duplicates.
    entry_size = 0;
    for(nnz_t i=0; i<RAP_temp.entry.size(); i++){
//        std::cout << RAP_temp.entry[i] << std::endl;
//        Ac->entry.push_back(RAP_temp.entry[i]);
        Ac->entry_temp[entry_size] = RAP_temp.entry[i];
        while(i<RAP_temp.entry.size()-1 && RAP_temp.entry[i] == RAP_temp.entry[i+1]){ // values of entries with the same row and col should be added.
//            if(rank==0) std::cout << Ac->entry_temp[entry_size] << std::endl;
            Ac->entry_temp[entry_size].val += RAP_temp.entry[i+1].val;
            i++;
        }
        entry_size++;
        // todo: pruning. don't hard code tol. does this make the matrix non-symmetric?
//        if( abs(Ac->entry.back().val) < 1e-6)
//            Ac->entry.pop_back();
    }

    Ac->entry_temp.resize(entry_size);
    Ac->entry_temp.shrink_to_fit();
//    print_vector(Ac->entry_temp, -1, "Ac->entry_temp", Ac->comm);
//    if(rank==0) printf("rank %d: entry_size = %lu \n", rank, entry_size);

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 5: rank = %d\n", rank); MPI_Barrier(comm);}

    // ********** setup matrix **********

    // decide to partition based on number of rows or nonzeros.
//    if(switch_repartition && Ac->density >= repartition_threshold)
//        Ac->repartition4(); // based on number of rows
//    else
    Ac->repartition_nnz_update_Ac(); // based on number of nonzeros

    diff.clear();
    diff.swap(Ac->entry_temp);

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 6: rank = %d\n", rank); MPI_Barrier(comm);}

//    repartition_u_shrink_prepare(grid);

//    if(Ac->shrinked)
//        Ac->shrink_cpu();

    // todo: try to reduce matrix_setup() for this case.
    if(Ac->active)
        Ac->matrix_setup();

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("end of coarsen2:  rank = %d\n", rank); MPI_Barrier(comm);}

    return 0;
} // end of coarsen_update_Ac()


//int saena_object::coarsen_update_Ac -> implemented based on the older version of matmat.
/*
int saena_object::coarsen_update_Ac(Grid *grid, std::vector<cooEntry> &diff){

    // This function computes delta_Ac = RAP in which A is diff = diag_block(A) - diag_block(A_new) at each level.
    // Then, updates Ac with the delta_Ac. Finally, it saves delta_Ac in diff for doing the same operation
    // for the next level.

    saena_matrix *A    = grid->A;
    prolong_matrix *P  = &grid->P;
    restrict_matrix *R = &grid->R;
    saena_matrix *Ac   = &grid->Ac;

    MPI_Comm comm = A->comm;
//    Ac->active_old_comm = true;

    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    if(verbose_coarsen2){
        MPI_Barrier(comm);
        printf("start of coarsen2: rank = %d, nprocs: %d, P.nnz_l = %lu, P.nnz_g = %lu, R.nnz_l = %lu,"
               " R.nnz_g = %lu, R.M = %u, R->nnz_l_local = %lu, R->nnz_l_remote = %lu \n\n", rank, nprocs,
               P->nnz_l, P->nnz_g, R->nnz_l, R->nnz_g, R->M, R->nnz_l_local, R->nnz_l_remote);
    }

    prolong_matrix RA_temp(comm); // RA_temp is being used to remove duplicates while pushing back to RA.

    // ************************************* RA_temp - A local *************************************
    // Some local and remote elements of RA_temp are computed here using local R and local A.
    // Note: A local means whole entries of A on this process, not just the diagonal block.

    std::sort(diff.begin(), diff.end(), row_major);

    // alloacted memory for AMaxM, instead of A.M to avoid reallocation of memory for when receiving data from other procs.
    unsigned int* AnnzPerRow = (unsigned int*)malloc(sizeof(unsigned int)*A->M);
    std::fill(&AnnzPerRow[0], &AnnzPerRow[A->M], 0);
    for(nnz_t i = 0; i < diff.size(); i++){
//        if(rank==0) printf("%u\n", diff[i].row);
        AnnzPerRow[diff[i].row]++;
    }

//    MPI_Barrier(A->comm);
//    if(rank==0){
//        printf("rank = %d, AnnzPerRow: \n", rank);
//        for(long i=0; i<A->M; i++)
//            printf("%lu \t%u \n", i, AnnzPerRow[i]);}

    // alloacted memory for AMaxM+1, instead of A.M+1 to avoid reallocation of memory for when receiving data from other procs.
    unsigned int* AnnzPerRowScan = (unsigned int*)malloc(sizeof(unsigned int)*(A->M+1));
    AnnzPerRowScan[0] = 0;
    for(index_t i = 0; i < A->M; i++){
        AnnzPerRowScan[i+1] = AnnzPerRowScan[i] + AnnzPerRow[i];
//        if(rank==1) printf("i=%lu, AnnzPerRow=%d, AnnzPerRowScan = %d\n", i+A->split[rank], AnnzPerRow[i], AnnzPerRowScan[i+1]);
    }

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 1: rank = %d\n", rank); MPI_Barrier(comm);}

    index_t jstart, jend;
    if(!R->entry_local.empty()) {
        for (index_t i = 0; i < R->nnz_l_local; i++) {
//            if(rank==0) std::cout << "i=" << i << "\tR[" << R->entry_local[i].row << ", " << R->entry_local[i].col
//                                  << "]=" << R->entry_local[i].val << std::endl;
            jstart = AnnzPerRowScan[R->entry_local[i].col - P->split[rank]];
            jend   = AnnzPerRowScan[R->entry_local[i].col - P->split[rank] + 1];
            if(jend - jstart == 0) continue;
            for (index_t j = jstart; j < jend; j++) {
//                if(rank==0) std::cout << "i=" << i << ", j=" << j
//                                      << "   \tdiff[" << diff[j].row
//                                      << ", " << diff[j].col << "]=\t" << diff[j].val
//                                      << "         \tR[" << R->entry_local[i].row << ", " << R->entry_local[i].col
//                                      << "]=\t" << R->entry_local[i].val << std::endl;
                RA_temp.entry.push_back(cooEntry(R->entry_local[i].row,
                                                 diff[j].col,
                                                 R->entry_local[i].val * diff[j].val));
            }
        }
    }

//    print_vector(RA_temp.entry, -1, "RA_temp.entry", comm);

    free(AnnzPerRow);
    free(AnnzPerRowScan);

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 2: rank = %d\n", rank); MPI_Barrier(comm);}

    std::sort(RA_temp.entry.begin(), RA_temp.entry.end());

//    print_vector(RA_temp.entry, -1, "RA_temp.entry: after sort", comm);

    prolong_matrix RA(comm);
    RA.entry.resize(RA_temp.entry.size());

    // remove duplicates.
    unsigned long entry_size = 0;
    for(nnz_t i=0; i<RA_temp.entry.size(); i++){
//        RA.entry.push_back(RA_temp.entry[i]);
        RA.entry[entry_size] = RA_temp.entry[i];
        while(i<RA_temp.entry.size()-1 && RA_temp.entry[i] == RA_temp.entry[i+1]){ // values of entries with the same row and col should be added.
//            RA.entry.back().val += RA_temp.entry[i+1].val;
            RA.entry[entry_size].val += RA_temp.entry[i+1].val;
            i++;
        }
//        if(rank==1) std::cout << std::endl << "final: " << std::endl << RA.entry[RA.entry.size()-1].val << std::endl;
        entry_size++;
    }

    RA.entry.resize(entry_size);
    RA.entry.shrink_to_fit();

//    print_vector(RA.entry, -1, "RA.entry", comm);

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 3: rank = %d\n", rank); MPI_Barrier(comm);}

    // ************************************* RAP_temp - P local *************************************
    // Some local and remote elements of RAP_temp are computed here.
    // Note: P local means whole entries of P on this process, not just the diagonal block.

    prolong_matrix RAP_temp(comm); // RAP_temp is being used to remove duplicates while pushing back to RAP.

    unsigned int* PnnzPerRow = (unsigned int*)malloc(sizeof(unsigned int)*P->M);
    std::fill(&PnnzPerRow[0], &PnnzPerRow[P->M], 0);
    for(nnz_t i=0; i<P->nnz_l_local; i++){
//        if(rank==1) printf("%u\n", P->entry_local[i].row);
        PnnzPerRow[P->entry_local[i].row]++;
    }

//    if(rank==1) for(long i=0; i<P->M; i++) std::cout << PnnzPerRow[i] << std::endl;

    unsigned int* PnnzPerRowScan = (unsigned int*)malloc(sizeof(unsigned int)*(P->M+1));
    PnnzPerRowScan[0] = 0;
    for(nnz_t i = 0; i < P->M; i++){
        PnnzPerRowScan[i+1] = PnnzPerRowScan[i] + PnnzPerRow[i];
//        if(rank==2) printf("i=%lu, PnnzPerRow=%d, PnnzPerRowScan = %d\n", i, PnnzPerRow[i], PnnzPerRowScan[i]);
    }

    long procNum = 0;
    std::vector<nnz_t> left_block_nnz(nprocs, 0);
    if(!RA.entry.empty()){
        for (nnz_t i = 0; i < RA.entry.size(); i++) {
            procNum = lower_bound2(&P->split[0], &P->split[nprocs], RA.entry[i].col);
            left_block_nnz[procNum]++;
//        if(rank==1) printf("rank=%d, col = %lu, procNum = %ld \n", rank, R->entry_remote[0].col, procNum);
        }
    }

    std::vector<nnz_t> left_block_nnz_scan(nprocs+1);
    left_block_nnz_scan[0] = 0;
    for(int i = 0; i < nprocs; i++)
        left_block_nnz_scan[i+1] = left_block_nnz_scan[i] + left_block_nnz[i];

//    print_vector(left_block_nnz_scan, -1, "left_block_nnz_scan", comm);

    // find row-wise ordering for A and save it in indicesP
//    std::vector<nnz_t> indicesP_Prolong(P->nnz_l_local);
//    for(nnz_t i=0; i<P->nnz_l_local; i++)
//        indicesP_Prolong[i] = i;
//    std::sort(&indicesP_Prolong[0], &indicesP_Prolong[P->nnz_l], sort_indices2(&*P->entry.begin()));

    //....................
    // note: Here we want to multiply local RA by local P, but whole RA.entry is local because of how it was made earlier in this function.
    //....................

    for(nnz_t i=left_block_nnz_scan[rank]; i<left_block_nnz_scan[rank+1]; i++){
        for(nnz_t j = PnnzPerRowScan[RA.entry[i].col - P->split[rank]]; j < PnnzPerRowScan[RA.entry[i].col - P->split[rank] + 1]; j++){

//            if(rank==3) std::cout << RA.entry[i].row + P->splitNew[rank] << "\t" << P->entry[indicesP_Prolong[j]].col << "\t" << RA.entry[i].val * P->entry[indicesP_Prolong[j]].val << std::endl;

            RAP_temp.entry.emplace_back(cooEntry(RA.entry[i].row + P->splitNew[rank],  // Ac.entry should have global indices at the end.
                                                 P->entry_local[P->indicesP_local[j]].col,
                                                 RA.entry[i].val * P->entry_local[P->indicesP_local[j]].val));
        }
    }

//    print_vector(RAP_temp.entry, 0, "RAP_temp.entry", comm);

    free(PnnzPerRow);
    free(PnnzPerRowScan);

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 4: rank = %d\n", rank); MPI_Barrier(comm);}

    std::sort(RAP_temp.entry.begin(), RAP_temp.entry.end());

//    print_vector(RAP_temp.entry, 0, "RAP_temp.entry: after sort", comm);

    // erase_keep_remote() was called on Ac, so the remote elements are already in Ac.
    // Now resize it to store new local entries.
    Ac->entry_temp.resize(RAP_temp.entry.size());

    // remove duplicates.
    entry_size = 0;
    for(nnz_t i=0; i<RAP_temp.entry.size(); i++){
//        std::cout << RAP_temp.entry[i] << std::endl;
//        Ac->entry.push_back(RAP_temp.entry[i]);
        Ac->entry_temp[entry_size] = RAP_temp.entry[i];
        while(i<RAP_temp.entry.size()-1 && RAP_temp.entry[i] == RAP_temp.entry[i+1]){ // values of entries with the same row and col should be added.
//            if(rank==0) std::cout << Ac->entry_temp[entry_size] << std::endl;
            Ac->entry_temp[entry_size].val += RAP_temp.entry[i+1].val;
            i++;
        }
        entry_size++;
        // todo: pruning. don't hard code tol. does this make the matrix non-symmetric?
//        if( abs(Ac->entry.back().val) < 1e-6)
//            Ac->entry.pop_back();
    }

    Ac->entry_temp.resize(entry_size);
    Ac->entry_temp.shrink_to_fit();
//    print_vector(Ac->entry_temp, -1, "Ac->entry_temp", Ac->comm);
//    if(rank==0) printf("rank %d: entry_size = %lu \n", rank, entry_size);

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 5: rank = %d\n", rank); MPI_Barrier(comm);}

    // ********** setup matrix **********

    // decide to partition based on number of rows or nonzeros.
//    if(switch_repartition && Ac->density >= repartition_threshold)
//        Ac->repartition4(); // based on number of rows
//    else
    Ac->repartition_nnz_update_Ac(); // based on number of nonzeros

    diff.clear();
    diff.swap(Ac->entry_temp);

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen2: step 6: rank = %d\n", rank); MPI_Barrier(comm);}

//    repartition_u_shrink_prepare(grid);

//    if(Ac->shrinked)
//        Ac->shrink_cpu();

    // todo: try to reduce matrix_setup() for this case.
    if(Ac->active)
        Ac->matrix_setup();

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("end of coarsen2:  rank = %d\n", rank); MPI_Barrier(comm);}

    return 0;
} // end of coarsen_update_Ac()
*/


// int saena_object::coarsen2
/*
int saena_object::coarsen2(saena_matrix* A, prolong_matrix* P, restrict_matrix* R, saena_matrix* Ac){
    // this function is similar to the coarsen(), but does R*A*P for only local (diagonal) blocks.

    // todo: to improve the performance of this function, consider using the arrays used for RA also for RAP.
    // todo: this way allocating and freeing memory will be halved.

    MPI_Comm comm = A->comm;
//    Ac->active_old_comm = true;

//    int rank1, nprocs1;
//    MPI_Comm_size(comm, &nprocs1);
//    MPI_Comm_rank(comm, &rank1);
//    if(A->active_old_comm)
//        printf("rank = %d, nprocs = %d active\n", rank1, nprocs1);

    int nprocs, rank;
    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    if(verbose_coarsen2){
        MPI_Barrier(comm);
        printf("start of coarsen: rank = %d, nprocs: %d, A->M = %u, A.nnz_l = %lu, A.nnz_g = %lu, P.nnz_l = %lu, P.nnz_g = %lu, R.nnz_l = %lu,"
                       " R.nnz_g = %lu, R.M = %u, R->nnz_l_local = %lu, R->nnz_l_remote = %lu \n\n", rank, nprocs, A->M, A->nnz_l,
               A->nnz_g, P->nnz_l, P->nnz_g, R->nnz_l, R->nnz_g, R->M, R->nnz_l_local, R->nnz_l_remote);
    }

//    unsigned long i, j;
    prolong_matrix RA_temp(comm); // RA_temp is being used to remove duplicates while pushing back to RA.

    // ************************************* RA_temp - A local *************************************
    // Some local and remote elements of RA_temp are computed here using local R and local A.

    unsigned int AMaxNnz, AMaxM;
    MPI_Allreduce(&A->nnz_l, &AMaxNnz, 1, MPI_UNSIGNED_LONG, MPI_MAX, comm);
    MPI_Allreduce(&A->M, &AMaxM, 1, MPI_UNSIGNED, MPI_MAX, comm);
//    MPI_Barrier(comm); printf("\nrank=%d, AMaxNnz=%d, AMaxM = %d \n", rank, AMaxNnz, AMaxM); MPI_Barrier(comm);
    // todo: is this way better than using the previous Allreduce? reduce on processor 0, then broadcast to other processors.

    // alloacted memory for AMaxM, instead of A.M to avoid reallocation of memory for when receiving data from other procs.
//    unsigned int* AnnzPerRow = (unsigned int*)malloc(sizeof(unsigned int)*AMaxM);
//    std::fill(&AnnzPerRow[0], &AnnzPerRow[AMaxM], 0);
    std::vector<index_t> AnnzPerRow(AMaxM, 0);
    for(nnz_t i=0; i<A->nnz_l; i++)
        AnnzPerRow[A->entry[i].row - A->split[rank]]++;

//    MPI_Barrier(A->comm);
//    if(rank==0){
//        printf("rank = %d, AnnzPerRow: \n", rank);
//        for(i=0; i<A->M; i++)
//            printf("%lu \t%u \n", i, AnnzPerRow[i]);
//    }

    // alloacted memory for AMaxM+1, instead of A.M+1 to avoid reallocation of memory for when receiving data from other procs.
//    unsigned int* AnnzPerRowScan = (unsigned int*)malloc(sizeof(unsigned int)*(AMaxM+1));
    std::vector<nnz_t> AnnzPerRowScan(AMaxM+1);
    AnnzPerRowScan[0] = 0;
    for(index_t i=0; i<A->M; i++){
        AnnzPerRowScan[i+1] = AnnzPerRowScan[i] + AnnzPerRow[i];
//        if(rank==1) printf("i=%lu, AnnzPerRow=%d, AnnzPerRowScan = %d\n", i+A->split[rank], AnnzPerRow[i], AnnzPerRowScan[i+1]);
    }

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen: step 1: rank = %d", rank); MPI_Barrier(comm);}

    // todo: combine indicesP and indicesPRecv together.
    // find row-wise ordering for A and save it in indicesP
//    unsigned long* indicesP = (unsigned long*)malloc(sizeof(unsigned long)*A->nnz_l);
    std::vector<nnz_t> indicesP(A->nnz_l);
    for(nnz_t i=0; i<A->nnz_l; i++)
        indicesP[i] = i;
    std::sort(&indicesP[0], &indicesP[A->nnz_l], sort_indices2(&*A->entry.begin()));

    unsigned long jstart, jend;
    if(!R->entry_local.empty()) {
        for (nnz_t i = 0; i < R->nnz_l_local; i++) {
            jstart = AnnzPerRowScan[R->entry_local[i].col - P->split[rank]];
            jend   = AnnzPerRowScan[R->entry_local[i].col - P->split[rank] + 1];
            if(jend - jstart == 0) continue;
            for (nnz_t j = jstart; j < jend; j++) {
//            if(rank==0) std::cout << A->entry[indicesP[j]].row << "\t" << A->entry[indicesP[j]].col << "\t" << A->entry[indicesP[j]].val
//                             << "\t" << R->entry_local[i].col << "\t" << R->entry_local[i].col - P->split[rank] << std::endl;
                RA_temp.entry.push_back(cooEntry(R->entry_local[i].row,
                                                 A->entry[indicesP[j]].col,
                                                 R->entry_local[i].val * A->entry[indicesP[j]].val));
            }
        }
    }

//    free(indicesP);
    indicesP.clear();
    indicesP.shrink_to_fit();

//    if(rank==0){
//        std::cout << "\nRA_temp.entry.size = " << RA_temp.entry.size() << std::endl;
//        for(i=0; i<RA_temp.entry.size(); i++)
//            std::cout << RA_temp.entry[i].row + R->splitNew[rank] << "\t" << RA_temp.entry[i].col << "\t" << RA_temp.entry[i].val << std::endl;}

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen: step 2: rank = %d", rank); MPI_Barrier(comm);}

    // todo: check this: since entries of RA_temp with these row indices only exist on this processor,
    // todo: duplicates happen only on this processor, so sorting should be done locally.
    std::sort(RA_temp.entry.begin(), RA_temp.entry.end());

//    MPI_Barrier(A->comm);
//    if(rank==1)
//        for(j=0; j<RA_temp.entry.size(); j++)
//            std::cout << RA_temp.entry[j].row + P->splitNew[rank] << "\t" << RA_temp.entry[j].col << "\t" << RA_temp.entry[j].val << std::endl;

    prolong_matrix RA(comm);

    // todo: here
    // remove duplicates.
    for(nnz_t i=0; i<RA_temp.entry.size(); i++){
        RA.entry.push_back(RA_temp.entry[i]);
//        if(rank==1) std::cout << std::endl << "start:" << std::endl << RA_temp.entry[i].val << std::endl;
        while(i<RA_temp.entry.size()-1 && RA_temp.entry[i] == RA_temp.entry[i+1]){ // values of entries with the same row and col should be added.
            RA.entry.back().val += RA_temp.entry[i+1].val;
            i++;
//            if(rank==1) std::cout << RA_temp.entry[i+1].val << std::endl;
        }
//        if(rank==1) std::cout << std::endl << "final: " << std::endl << RA.entry[RA.entry.size()-1].val << std::endl;
        // todo: pruning. don't hard code tol. does this make the matrix non-symmetric?
//        if( abs(RA.entry.back().val) < 1e-6)
//            RA.entry.pop_back();
//        if(rank==1) std::cout << "final: " << std::endl << RA.entry.back().val << std::endl;
    }

//    MPI_Barrier(comm);
//    if(rank==0){
//        std::cout << "RA.entry.size = " << RA.entry.size() << std::endl;
//        for(j=0; j<RA.entry.size(); j++)
//            std::cout << RA.entry[j].row + P->splitNew[rank] << "\t" << RA.entry[j].col << "\t" << RA.entry[j].val << std::endl;}
//    MPI_Barrier(comm);

    // find the start and end nnz iterator of each block of R.
    // use A.split for this part to find each block corresponding to each processor's A.
//    unsigned int* left_block_nnz = (unsigned int*)malloc(sizeof(unsigned int)*(nprocs));
//    std::fill(left_block_nnz, &left_block_nnz[nprocs], 0);
    std::vector<int> left_block_nnz(nprocs, 0);

//    MPI_Barrier(comm); printf("rank=%d entry = %ld \n", rank, R->entry_remote[0].col); MPI_Barrier(comm);

    // find the owner of the first R.remote element.
    long procNum = 0;
//    unsigned int nnzIter = 0;
    if(!R->entry_remote.empty()){
        for (nnz_t i = 0; i < R->entry_remote.size(); i++) {
            procNum = lower_bound2(&*A->split.begin(), &*A->split.end(), R->entry_remote[i].col);
            left_block_nnz[procNum]++;
//        if(rank==1) printf("rank=%d, col = %lu, procNum = %ld \n", rank, R->entry_remote[0].col, procNum);
//        if(rank==1) std::cout << "\nprocNum = " << procNum << "   \tcol = " << R->entry_remote[0].col
//                              << "  \tnnzIter = " << nnzIter << "\t first" << std::endl;
//        nnzIter++;
        }
    }

//    unsigned int* left_block_nnz_scan = (unsigned int*)malloc(sizeof(unsigned int)*(nprocs+1));
    std::vector<nnz_t> left_block_nnz_scan(nprocs+1);
    left_block_nnz_scan[0] = 0;
    for(int i = 0; i < nprocs; i++)
        left_block_nnz_scan[i+1] = left_block_nnz_scan[i] + left_block_nnz[i];

    // ************************************* RAP_temp - P local *************************************
    // Some local and remote elements of RAP_temp are computed here.

    prolong_matrix RAP_temp(comm); // RAP_temp is being used to remove duplicates while pushing back to RAP.
    index_t P_max_M;
    MPI_Allreduce(&P->M, &P_max_M, 1, MPI_UNSIGNED, MPI_MAX, comm);
//    MPI_Barrier(comm); printf("rank=%d, PMaxNnz=%d \n", rank, PMaxNnz); MPI_Barrier(comm);
    // todo: is this way better than using the previous Allreduce? reduce on processor 0, then broadcast to other processors.

//    unsigned int* PnnzPerRow = (unsigned int*)malloc(sizeof(unsigned int)*P_max_M);
//    std::fill(&PnnzPerRow[0], &PnnzPerRow[P->M], 0);
    std::vector<index_t> PnnzPerRow(P_max_M, 0);
    for(nnz_t i=0; i<P->nnz_l; i++){
        PnnzPerRow[P->entry[i].row]++;
    }

//    if(rank==1)
//        for(i=0; i<P->M; i++)
//            std::cout << PnnzPerRow[i] << std::endl;

//    unsigned int* PnnzPerRowScan = (unsigned int*)malloc(sizeof(unsigned int)*(P_max_M+1));
    std::vector<nnz_t> PnnzPerRowScan(P_max_M+1);
    PnnzPerRowScan[0] = 0;
    for(index_t i = 0; i < P->M; i++){
        PnnzPerRowScan[i+1] = PnnzPerRowScan[i] + PnnzPerRow[i];
//        if(rank==2) printf("i=%lu, PnnzPerRow=%d, PnnzPerRowScan = %d\n", i, PnnzPerRow[i], PnnzPerRowScan[i]);
    }

//    std::fill(left_block_nnz, &left_block_nnz[nprocs], 0);
    left_block_nnz.assign(nprocs, 0);
    if(!RA.entry.empty()){
        for (nnz_t i = 0; i < RA.entry.size(); i++) {
            procNum = lower_bound2(&P->split[0], &P->split[nprocs], RA.entry[i].col);
            left_block_nnz[procNum]++;
//        if(rank==1) printf("rank=%d, col = %lu, procNum = %ld \n", rank, R->entry_remote[0].col, procNum);
        }
    }

    left_block_nnz_scan[0] = 0;
    for(int i = 0; i < nprocs; i++)
        left_block_nnz_scan[i+1] = left_block_nnz_scan[i] + left_block_nnz[i];

//    if(rank==1){
//        std::cout << "RABlockStart: " << std::endl;
//        for(i=0; i<nprocs+1; i++)
//            std::cout << R_block_nnz_scan[i] << std::endl;}

    // todo: combine indicesP_Prolong and indicesP_ProlongRecv together.
    // find row-wise ordering for A and save it in indicesP
//    unsigned long* indicesP_Prolong = (unsigned long*)malloc(sizeof(unsigned long)*P->nnz_l);
    std::vector<nnz_t> indicesP_Prolong(P->nnz_l);
    for(nnz_t i=0; i<P->nnz_l; i++)
        indicesP_Prolong[i] = i;
    std::sort(&indicesP_Prolong[0], &indicesP_Prolong[P->nnz_l], sort_indices2(&*P->entry.begin()));

    for(nnz_t i=left_block_nnz_scan[rank]; i<left_block_nnz_scan[rank+1]; i++){
        for(nnz_t j = PnnzPerRowScan[RA.entry[i].col - P->split[rank]]; j < PnnzPerRowScan[RA.entry[i].col - P->split[rank] + 1]; j++){

//            if(rank==3) std::cout << RA.entry[i].row + P->splitNew[rank] << "\t" << P->entry[indicesP_Prolong[j]].col << "\t" << RA.entry[i].val * P->entry[indicesP_Prolong[j]].val << std::endl;

            RAP_temp.entry.emplace_back(cooEntry(RA.entry[i].row + P->splitNew[rank],  // Ac.entry should have global indices at the end.
                                                 P->entry[indicesP_Prolong[j]].col,
                                                 RA.entry[i].val * P->entry[indicesP_Prolong[j]].val));
        }
    }

//    if(rank==1)
//        for(i=0; i<RAP_temp.entry.size(); i++)
//            std::cout << RAP_temp.entry[i].row << "\t" << RAP_temp.entry[i].col << "\t" << RAP_temp.entry[i].val << std::endl;

//    free(indicesP_Prolong);
//    free(PnnzPerRow);
//    free(PnnzPerRowScan);
//    free(left_block_nnz);
//    free(left_block_nnz_scan);

    indicesP_Prolong.clear();
    PnnzPerRow.clear();
    PnnzPerRowScan.clear();
    left_block_nnz.clear();
    left_block_nnz_scan.clear();

    indicesP_Prolong.shrink_to_fit();
    PnnzPerRow.shrink_to_fit();
    PnnzPerRowScan.shrink_to_fit();
    left_block_nnz.shrink_to_fit();
    left_block_nnz_scan.shrink_to_fit();

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen: step 3: rank = %d", rank); MPI_Barrier(comm);}

    std::sort(RAP_temp.entry.begin(), RAP_temp.entry.end());

//    if(rank==2)
//        for(j=0; j<RAP_temp.entry.size(); j++)
//            std::cout << RAP_temp.entry[j].row << "\t" << RAP_temp.entry[j].col << "\t" << RAP_temp.entry[j].val << std::endl;

    // todo:here
    // remove duplicates.
    for(nnz_t i=0; i<RAP_temp.entry.size(); i++){
        Ac->entry.push_back(RAP_temp.entry[i]);
        while(i<RAP_temp.entry.size()-1 && RAP_temp.entry[i] == RAP_temp.entry[i+1]){ // values of entries with the same row and col should be added.
            Ac->entry.back().val += RAP_temp.entry[i+1].val;
            i++;
        }
        // todo: pruning. don't hard code tol. does this make the matrix non-symmetric?
//        if( abs(Ac->entry.back().val) < 1e-6)
//            Ac->entry.pop_back();
    }
//    MPI_Barrier(comm); printf("rank=%d here6666666666666!!!!!!!! \n", rank); MPI_Barrier(comm);

//    par::sampleSort(Ac_temp, Ac->entry, comm);
//    Ac->entry = Ac_temp;

//    if(rank==1){
//        std::cout << "after sort:" << std::endl;
//        for(j=0; j<Ac->entry.size(); j++)
//            std::cout << Ac->entry[j] << std::endl;
//    }

    Ac->nnz_l = Ac->entry.size();
    MPI_Allreduce(&Ac->nnz_l, &Ac->nnz_g, 1, MPI_UNSIGNED_LONG, MPI_SUM, comm);
    Ac->Mbig = P->Nbig;
    Ac->M = P->splitNew[rank+1] - P->splitNew[rank];
    Ac->split = P->splitNew;
    Ac->cpu_shrink_thre1 = A->cpu_shrink_thre1;
    Ac->last_M_shrink = A->last_M_shrink;
//    Ac->last_nnz_shrink = A->last_nnz_shrink;
    Ac->last_density_shrink = A->last_density_shrink;
    Ac->comm = A->comm;
    Ac->comm_old = A->comm;
    Ac->active_old_comm = true;
//    printf("\nrank = %d, Ac->Mbig = %u, Ac->M = %u, Ac->nnz_l = %u, Ac->nnz_g = %u \n", rank, Ac->Mbig, Ac->M, Ac->nnz_l, Ac->nnz_g);

//    if(verbose_coarsen){
//        printf("\nrank = %d, Ac->Mbig = %u, Ac->M = %u, Ac->nnz_l = %u, Ac->nnz_g = %u \n", rank, Ac->Mbig, Ac->M, Ac->nnz_l, Ac->nnz_g);}

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen: step 4: rank = %d", rank); MPI_Barrier(comm);}

//    MPI_Barrier(comm);
//    if(rank==0){
//        for(i = 0; i < Ac->nnz_l; i++)
//            std::cout << i << "\t" << Ac->entry[i] << std::endl;
//        std::cout << std::endl;}
//    MPI_Barrier(comm);
//    if(rank==1){
//        for(i = 0; i < Ac->nnz_l; i++)
//            std::cout << i << "\t" << Ac->entry[i] << std::endl;
//        std::cout << std::endl;}
//    MPI_Barrier(comm);
//    if(rank==2){
//        for(i = 0; i < Ac->nnz_l; i++)
//            std::cout << i << "\t" << Ac->entry[i] << std::endl;
//        std::cout << std::endl;}
//    MPI_Barrier(comm);

//    printf("rank=%d \tA: Mbig=%u, nnz_g = %u, nnz_l = %u, M = %u \tAc: Mbig=%u, nnz_g = %u, nnz_l = %u, M = %u \n",
//            rank, A->Mbig, A->nnz_g, A->nnz_l, A->M, Ac->Mbig, Ac->nnz_g, Ac->nnz_l, Ac->M);
//    MPI_Barrier(comm);
//    if(rank==1)
//        for(i=0; i<nprocs+1; i++)
//            std::cout << Ac->split[i] << std::endl;


    // ********** check for cpu shrinking **********
    // if number of rows on Ac < threshold*number of rows on A, then shrink.
    // redistribute Ac from processes 4k+1, 4k+2 and 4k+3 to process 4k.

    // todo: is this part required for coarsen2()?
//    if( (nprocs >= Ac->cpu_shrink_thre2) && (Ac->last_M_shrink >= (Ac->Mbig * A->cpu_shrink_thre1)) ){

//        shrink_cpu_A(Ac, P->splitNew);

//        MPI_Barrier(comm);
//        if(rank==0) std::cout << "\nafter shrink: Ac->last_M_shrink = " << Ac->last_M_shrink << ", Ac->Mbig = " << Ac->Mbig
//                              << ", mult = " << Ac->Mbig * A->cpu_shrink_thre1 << std::endl;
//        MPI_Barrier(comm);
//    }

    // ********** setup matrix **********

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("coarsen: step 5: rank = %d", rank); MPI_Barrier(comm);}

    if(Ac->active) // there is another if(active) in matrix_setup().
        Ac->matrix_setup();

    if(verbose_coarsen2){
        MPI_Barrier(comm); printf("end of coarsen: step 6: rank = %d", rank); MPI_Barrier(comm);}

    return 0;
} // end of SaenaObject::coarsen
*/