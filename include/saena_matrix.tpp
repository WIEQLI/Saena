#pragma once

#include "saena_matrix.h"

inline void saena_matrix::matvec(std::vector<value_t>& v, std::vector<value_t>& w){
    if(switch_to_dense && density >= dense_threshold){
        std::cout << "dense matvec is commented out!" << std::endl;
        // uncomment to enable DENSE
//            if(!dense_matrix_generated){
//                generate_dense_matrix();
//            }
//            dense_matrix.matvec(v, w);

    }else{
        matvec_sparse(v,w);
//            matvec_sparse_zfp(v,w);
    }
}

// Vector res = A * u - rhs;
inline void saena_matrix::residual(std::vector<value_t>& u, std::vector<value_t>& rhs, std::vector<value_t>& res){
    matvec(u, res);
#pragma omp parallel for
    for(index_t i = 0; i < M; ++i){
        res[i] -= rhs[i];
    }
}

// Vector res = rhs - A * u
inline void saena_matrix::residual_negative(std::vector<value_t>& u, std::vector<value_t>& rhs, std::vector<value_t>& res){
    matvec(u, res);
#pragma omp parallel for
    for(index_t i = 0; i < M; i++){
        res[i] = rhs[i] - res[i];
    }
}