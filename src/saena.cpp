#include "saena.hpp"
#include "saena_matrix.h"
#include "saena_vector.h"
#include "saena_object.h"
#include "grid.h"
#include "parUtils.h"
//#include "pugixml.hpp"

# define PETSC_PI 3.14159265358979323846

// ******************************* matrix *******************************

saena::matrix::matrix(MPI_Comm comm) {
    m_pImpl = new saena_matrix(comm);
}

saena::matrix::matrix() {
    m_pImpl = new saena_matrix();
}

// copy constructor
saena::matrix::matrix(const saena::matrix &B){
    m_pImpl = new saena_matrix(*B.m_pImpl);
    add_dup = B.add_dup;
}

saena::matrix& saena::matrix::operator=(const saena::matrix &B){
    if(this != &B) {
        delete m_pImpl;
        m_pImpl = new saena_matrix(*B.m_pImpl);
        add_dup = B.add_dup;
    }
    return *this;
}

void saena::matrix::set_comm(MPI_Comm comm) {
    m_pImpl->set_comm(comm);
}

MPI_Comm saena::matrix::get_comm(){
    return m_pImpl->comm;
}

saena::matrix::~matrix(){
//    m_pImpl->erase();
    delete m_pImpl;
}


int saena::matrix::read_file(const char *name) {
    m_pImpl->read_file(name);
    return 0;
}

int saena::matrix::read_file(const char *name, const std::string &input_type) {
    m_pImpl->read_file(name, input_type);
    return 0;
}


int saena::matrix::set(index_t i, index_t j, value_t val){

        if (!add_dup) {
            m_pImpl->set(i, j, val);
        } else {
            m_pImpl->set2(i, j, val);
        }

    return 0;
}

int saena::matrix::set(index_t* row, index_t* col, value_t* val, nnz_t nnz_local){

    if (!add_dup)
        m_pImpl->set(row, col, val, nnz_local);
    else
        m_pImpl->set2(row, col, val, nnz_local);

    return 0;
}

int saena::matrix::set(index_t i, index_t j, unsigned int size_x, unsigned int size_y, value_t* val){
    // ordering of val should be column-major.
    unsigned int ii, jj;
    nnz_t iter = 0;
    //todo: add openmp
    for(jj = 0; jj < size_y; jj++) {
        for(ii = 0; ii < size_x; ii++) {
            if( val[iter] != 0){
                if(!add_dup)
                    m_pImpl->set(i+ii, j+jj, val[iter]);
                else
                    m_pImpl->set2(i+ii, j+jj, val[iter]);

                iter++;
            }
        }
    }

    return 0;
}

int saena::matrix::set(index_t i, index_t j, unsigned int* di, unsigned int* dj, value_t* val, nnz_t nnz_local){
    nnz_t ii;
    for(ii = 0; ii < nnz_local; ii++) {
        if(val[ii] != 0){
            if(!add_dup)
                m_pImpl->set(i+di[ii], j+dj[ii], val[ii]);
            else
                m_pImpl->set2(i+di[ii], j+dj[ii], val[ii]);
        }
    }

    return 0;
}


void saena::matrix::set_p_order(int _p_order){
    m_pImpl->set_p_order(_p_order);
}

void saena::matrix::set_prodim(int _prodim){
    m_pImpl->set_prodim(_prodim);
}


void saena::matrix::set_num_threads(const int &nth){
    omp_set_num_threads(nth);
}


int saena::matrix::assemble(bool scale /*= true*/) {
    m_pImpl->assemble(scale);
    return 0;
}

int saena::matrix::assemble_band_matrix(){
    m_pImpl->matrix_setup();
    return 0;
}


int saena::matrix::assemble_writeToFile(const char *folder_name/* = ""*/){

    if(!m_pImpl->assembled){
        m_pImpl->repartition_nnz_initial();
        m_pImpl->matrix_setup(false);
        if(m_pImpl->enable_shrink) m_pImpl->compute_matvec_dummy_time();
        m_pImpl->writeMatrixToFile(folder_name);
    }else{
        m_pImpl->repartition_nnz_update();
        m_pImpl->matrix_setup_update(false);
        m_pImpl->writeMatrixToFile(folder_name);
    }

    return 0;
}

int saena::matrix::writeMatrixToFile(const std::string &name/* = ""*/) const{
    m_pImpl->writeMatrixToFile(name);
    return 0;
}


saena_matrix* saena::matrix::get_internal_matrix(){
    return m_pImpl;
}

index_t saena::matrix::get_num_rows(){
    return m_pImpl->Mbig;
}

index_t saena::matrix::get_num_local_rows() {
    return m_pImpl->M;
}

nnz_t saena::matrix::get_nnz(){
    return m_pImpl->nnz_g;
}

nnz_t saena::matrix::get_local_nnz(){
    return m_pImpl->nnz_l;
}

int saena::matrix::print(int ran, std::string name){
    m_pImpl->print_entry(ran, name);
    return 0;
}

int saena::matrix::set_shrink(bool val){
    m_pImpl->enable_shrink   = val;
    m_pImpl->enable_shrink_c = val;
    return 0;
}


int saena::matrix::erase(){
    m_pImpl->erase();
    return 0;
}

int saena::matrix::erase_lazy_update(){
    m_pImpl->erase_lazy_update();
    return 0;
}

int saena::matrix::erase_no_shrink_to_fit(){
    m_pImpl->erase_no_shrink_to_fit();
    return 0;
}

void saena::matrix::destroy(){
    m_pImpl->erase();
//    m_pImpl->~matrix();
}

int saena::matrix::add_duplicates(bool add) {
    if(add){
        add_dup = true;
        m_pImpl->add_duplicates = true;
    } else{
        add_dup = false;
        m_pImpl->add_duplicates = false;
    }
    return 0;
}


// ******************************* vector *******************************

saena::vector::vector(MPI_Comm comm) {
    m_pImpl = new saena_vector(comm);
}

saena::vector::vector() {
    m_pImpl = new saena_vector();
}

// copy constructor
saena::vector::vector(const saena::vector &B) {
    m_pImpl = new saena_vector(*B.m_pImpl);
    m_pImpl->set_dup_flag(B.m_pImpl->add_duplicates);
//    add_dup = B.add_dup;
}

saena::vector& saena::vector::operator=(const saena::vector &B) {
    delete m_pImpl;
    m_pImpl = new saena_vector(*B.m_pImpl);
    m_pImpl->set_dup_flag(B.m_pImpl->add_duplicates);
//    add_dup = B.add_dup;
    return *this;
}

void saena::vector::set_comm(MPI_Comm comm) {
    m_pImpl->set_comm(comm);
}

MPI_Comm saena::vector::get_comm() {
    return m_pImpl->comm;
}

saena::vector::~vector() {
//    m_pImpl->erase();
    delete m_pImpl;
}


int saena::vector::set_idx_offset(const index_t offset){
    m_pImpl->set_idx_offset(offset);
    return 0;
}


//int saena::vector::read_file(const char *name) {
//    m_pImpl->read_file(name);
//    return 0;
//}

//int saena::vector::read_file(const char *name, const std::string &input_type) {
//    m_pImpl->read_file(name, input_type);
//    return 0;
//}


int saena::vector::set(const index_t i, const value_t val){
    m_pImpl->set(i, val);
    return 0;
}

int saena::vector::set(const index_t* idx, const value_t* val, const index_t size){
    m_pImpl->set(idx, val, size);
    return 0;
}

int saena::vector::set(const value_t* val, const index_t size, const index_t offset){
    m_pImpl->set(val, size, offset);
    return 0;
}

int saena::vector::set(const value_t* val, const index_t size){
    m_pImpl->set(val, size);
    return 0;
}

int saena::vector::set_dup_flag(bool add){
    m_pImpl->set_dup_flag(add);
    return 0;
}


int saena::vector::assemble() {
    m_pImpl->assemble();
    return 0;
}


int saena::vector::get_vec(std::vector<double> &vec){
    m_pImpl->get_vec(vec);
    return 0;
}


int saena::vector::return_vec(std::vector<double> &u1, std::vector<double> &u2){
    m_pImpl->return_vec(u1, u2);
    return 0;
}


saena_vector* saena::vector::get_internal_vector(){
    return m_pImpl;
}

int saena::vector::print_entry(int rank_){
    m_pImpl->print_entry(rank_);
    return 0;
}


// ******************************* options *******************************

saena::options::options() = default;

saena::options::options(int vcycle_n, double relT, std::string sm, int preSm, int postSm){
    solver_max_iter         = vcycle_n;
    relative_tolerance = relT;
    smoother           = sm;
    preSmooth          = preSm;
    postSmooth         = postSm;
}

saena::options::options(char* name){
    printf("Enable pugi to be able to use function options(char* name)!");
    exit(EXIT_FAILURE);
    
/*
    pugi::xml_document doc;
    if (!doc.load_file(name))
        std::cout << "Could not find the xml file!" << std::endl;

    pugi::xml_node opts = doc.child("SAENA").first_child();

    pugi::xml_attribute attr = opts.first_attribute();
    solver_max_iter = std::stoi(attr.value());

    attr = attr.next_attribute();
    relative_tolerance = std::stod(attr.value());

    attr = attr.next_attribute();
    smoother = attr.value();

    attr = attr.next_attribute();
    preSmooth = std::stoi(attr.value());

    attr = attr.next_attribute();
    postSmooth = std::stoi(attr.value());

//    for (pugi::xml_attribute attr2 = opts.first_attribute(); attr2; attr2 = attr2.next_attribute())
//        std::cout << attr2.name() << " = " << attr2.value() << std::endl;

//    std::cout << "solver_max_iter = " << solver_max_iter << ", solver_tol = " << solver_tol
//              << ", smoother = " << smoother << ", preSmooth = " << preSmooth << ", postSmooth = " << postSmooth << std::endl;
*/
}

saena::options::~options() = default;

void saena::options::set(int max_it, double relT, std::string sm, int preSm, int postSm){
    solver_max_iter    = max_it;
    relative_tolerance = relT;
    smoother           = sm;
    preSmooth          = preSm;
    postSmooth         = postSm;
}


void saena::options::set_max_iter(int v){
    solver_max_iter = v;
}

void saena::options::set_relative_tolerance(double r){
    relative_tolerance = r;
}

void saena::options::set_smoother(std::string s){
    smoother = s;
}

void saena::options::set_preSmooth(int pr){
    preSmooth = pr;
}

void saena::options::set_postSmooth(int po){
    postSmooth = po;
}


int saena::options::get_max_iter(){
    return solver_max_iter;
}

double saena::options::get_relative_tolerance(){
    return relative_tolerance;
}

std::string saena::options::get_smoother(){
    return smoother;
}

int saena::options::get_preSmooth(){
    return preSmooth;
}

int saena::options::get_postSmooth(){
    return postSmooth;
}


// ******************************* amg *******************************

saena::amg::amg(){
    m_pImpl = new saena_object();
}

saena::amg::~amg(){
    delete m_pImpl;
}

void saena::amg::set_dynamic_levels(const bool &dl) {
    m_pImpl->set_dynamic_levels(dl);
}

int saena::amg::set_matrix(saena::matrix* A, saena::options* opts){
    m_pImpl->set_parameters(opts->get_max_iter(), opts->get_relative_tolerance(),
                            opts->get_smoother(), opts->get_preSmooth(), opts->get_postSmooth());
    m_pImpl->setup(A->get_internal_matrix());
    return 0;
}

int saena::amg::set_matrix(saena::matrix* A, saena::options* opts, std::vector<std::vector<int>> &m_l2g, std::vector<int> &m_g2u, int m_bdydof, std::vector<int> &order_dif){
    m_pImpl->set_parameters(opts->get_max_iter(), opts->get_relative_tolerance(),
                            opts->get_smoother(), opts->get_preSmooth(), opts->get_postSmooth());
    m_pImpl->setup(A->get_internal_matrix(), m_l2g, m_g2u, m_bdydof, order_dif);
    return 0;
}

int saena::amg::set_rhs(saena::vector &rhs){
    m_pImpl->set_repartition_rhs(rhs.get_internal_vector());
    return 0;
}


void saena::amg::set_num_threads(const int &nth){
    omp_set_num_threads(nth);
}


saena_object* saena::amg::get_object() {
    return m_pImpl;
}


int saena::amg::set_shrink_levels(std::vector<bool> sh_lev_vec) {
    m_pImpl->set_shrink_levels(sh_lev_vec);
    return 0;
}

int saena::amg::set_shrink_values(std::vector<int> sh_val_vec) {
    m_pImpl->set_shrink_values(sh_val_vec);
    return 0;
}


int saena::amg::switch_repartition(bool val) {
    m_pImpl->switch_repartition = val;
    return 0;
}


int saena::amg::set_repartition_threshold(float thre){
    m_pImpl->set_repartition_threshold(thre);
    return 0;
}


int saena::amg::switch_to_dense(bool val) {
    m_pImpl->switch_to_dense = val;
    return 0;
}


int saena::amg::set_dense_threshold(float thre){
    m_pImpl->dense_threshold = thre;
    return 0;
}


double saena::amg::get_dense_threshold(){
    return m_pImpl->dense_threshold;
}


MPI_Comm saena::amg::get_orig_comm(){
    return m_pImpl->get_orig_comm();
}


int saena::amg::solve_smoother(std::vector<value_t>& u, saena::options* opts){
    m_pImpl->set_parameters(opts->get_max_iter(), opts->get_relative_tolerance(),
                            opts->get_smoother(), opts->get_preSmooth(), opts->get_postSmooth());
    m_pImpl->solve_smoother(u);
    Grid *g = &m_pImpl->grids[0];
    g->rhs_orig->return_vec(u);
    return 0;
}

int saena::amg::solve(std::vector<value_t>& u, saena::options* opts){
    m_pImpl->set_parameters(opts->get_max_iter(), opts->get_relative_tolerance(),
                            opts->get_smoother(), opts->get_preSmooth(), opts->get_postSmooth());
    m_pImpl->solve(u);
    Grid *g = &m_pImpl->grids[0];
    g->rhs_orig->return_vec(u);
    return 0;
}


int saena::amg::solve_CG(std::vector<value_t>& u, saena::options* opts){
    m_pImpl->set_parameters(opts->get_max_iter(), opts->get_relative_tolerance(),
                            opts->get_smoother(), opts->get_preSmooth(), opts->get_postSmooth());
    m_pImpl->solve_CG(u);
    Grid *g = &m_pImpl->grids[0];
    g->rhs_orig->return_vec(u);
    return 0;
}

int saena::amg::solve_petsc(std::vector<value_t>& u){
    m_pImpl->solve_petsc(u);
    Grid *g = &m_pImpl->grids[0];
    g->rhs_orig->return_vec(u);
    return 0;
}

int saena::amg::solve_pCG(std::vector<value_t>& u, saena::options* opts){
    m_pImpl->set_parameters(opts->get_max_iter(), opts->get_relative_tolerance(),
                            opts->get_smoother(), opts->get_preSmooth(), opts->get_postSmooth());
    m_pImpl->solve_pCG(u);

    if(m_pImpl->remove_boundary){
//        m_pImpl->add_boundary_sol(u); // TODO: this part should be fixed
    } else {
        Grid *g = &m_pImpl->grids[0];
        g->rhs_orig->return_vec(u);
    }

    return 0;
}


int saena::amg::solve_GMRES(std::vector<value_t>& u, saena::options* opts){
    m_pImpl->set_parameters(opts->get_max_iter(), opts->get_relative_tolerance(),
                            opts->get_smoother(), opts->get_preSmooth(), opts->get_postSmooth());
    m_pImpl->GMRES(u);
    Grid *g = &m_pImpl->grids[0];
    g->rhs_orig->return_vec(u);
    return 0;
}


int saena::amg::solve_pGMRES(std::vector<value_t>& u, saena::options* opts){
    m_pImpl->set_parameters(opts->get_max_iter(), opts->get_relative_tolerance(),
                            opts->get_smoother(), opts->get_preSmooth(), opts->get_postSmooth());
    m_pImpl->pGMRES(u);
    Grid *g = &m_pImpl->grids[0];
    g->rhs_orig->return_vec(u);
    return 0;
}

int saena::amg::update1(saena::matrix* A_ne){
    m_pImpl->update1(A_ne->get_internal_matrix());
    return 0;
}


int saena::amg::update2(saena::matrix* A_ne){
    m_pImpl->update2(A_ne->get_internal_matrix());
    return 0;
}


int saena::amg::update3(saena::matrix* A_ne){
    m_pImpl->update3(A_ne->get_internal_matrix());
    return 0;
}


//int saena::amg::solve_pcg_update1 & 2 & 3
/*
int saena::amg::solve_pcg_update1(std::vector<value_t>& u, saena::options* opts){
    m_pImpl->set_parameters(opts->get_max_iter(), opts->get_relative_tolerance(),
                            opts->get_smoother(), opts->get_preSmooth(), opts->get_postSmooth());
    m_pImpl->solve_pcg_update1(u);
    return 0;
}


int saena::amg::solve_pcg_update2(std::vector<value_t>& u, saena::options* opts){
    m_pImpl->set_parameters(opts->get_max_iter(), opts->get_relative_tolerance(),
                            opts->get_smoother(), opts->get_preSmooth(), opts->get_postSmooth());
    m_pImpl->solve_pcg_update2(u);
    return 0;
}


int saena::amg::solve_pcg_update3(std::vector<value_t>& u, saena::options* opts){
    m_pImpl->set_parameters(opts->get_max_iter(), opts->get_relative_tolerance(),
                            opts->get_smoother(), opts->get_preSmooth(), opts->get_postSmooth());
    m_pImpl->solve_pcg_update3(u);
    return 0;
}
*/


void saena::amg::destroy(){
    m_pImpl->destroy();
}


int saena::amg::set_verbose(bool verb) {
    m_pImpl->verbose = verb;
//    m_pImpl->verbose_setup = verb;
    verbose = verb;
    return 0;
}


int saena::amg::set_multigrid_max_level(int max){
    m_pImpl->max_level = max;
    return 0;
}


int saena::amg::set_scale(bool sc){
    m_pImpl->scale = sc;
    return 0;
}


int saena::amg::set_sample_sz_percent(double s_sz_prcnt){
    m_pImpl->sample_sz_percent = s_sz_prcnt;
    return 0;
}


int saena::amg::matrix_diff(saena::matrix &A1, saena::matrix &B1){

    saena_matrix *A = A1.get_internal_matrix();
    saena_matrix *B = B1.get_internal_matrix();

//    if(A->active){
        MPI_Comm comm = A->comm;
        int nprocs, rank;
        MPI_Comm_size(comm, &nprocs);
        MPI_Comm_rank(comm, &rank);

        if(A->nnz_g != B->nnz_g)
            if(rank==0) std::cout << "error: matrix_diff(): A.nnz_g != B.nnz_g" << std::endl;

//        A->print_entry(-1);
//        B->print_entry(-1);

        MPI_Barrier(comm);
        printf("\nmatrix_diff: \n");
        MPI_Barrier(comm);

//        if(rank==0){
            for(nnz_t i = 0; i < A->entry.size(); i++){
//                if(!almost_zero(A->entry[i].val - B->entry[i].val)){
                    std::cout << A->entry[i] << "\t" << B->entry[i] << "\t" << A->entry[i].val - B->entry[i].val << std::endl;
//                }
            }
//        }
//    }

    MPI_Barrier(comm);
    printf("A->entry.size() = %lu, B->entry.size() = %lu \n", A->entry.size(), B->entry.size());
    MPI_Barrier(comm);

    return 0;
}


void saena::amg::matmat(saena::matrix *A, saena::matrix *B, saena::matrix *C, bool assemble /*=true*/, const bool print_timing /*=false*/){
    m_pImpl->matmat(A->get_internal_matrix(), B->get_internal_matrix(), C->get_internal_matrix(), assemble, print_timing);
}


void saena::amg::profile_matvecs(){
    m_pImpl->profile_matvecs();
}
